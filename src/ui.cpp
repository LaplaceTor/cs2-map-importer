#include "ui.h"
#include "ui/ui_interface.h"
#include "mapimporter.h"
#include "appcore.h"

#include <QFileDialog>
#include <thread>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QDateTime>

Importer::Importer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    java_installed(false),
    vmf_default_path("C:\\"),
    content_folder_to_save("C:\\"),
    vpk_signatures_moved(false),
    log_file(nullptr),
    log_stream(nullptr)

{
    ui->setupUi(this);

    app_dir = QCoreApplication::applicationDirPath();

    log("Initializing CS2 Importer...");

    java_installed = AppCore::check_java();

    set_tooltips();
    set_stylesheets();
    get_addon_name();
    get_launch_options();

    if (!java_installed) {
        ui->bsp_button->setToolTip("Java is missing. BSP decompilation is disabled.");
        ui->bsp_button->setEnabled(false);
        log("Warning: Java is missing. BSP decompilation disabled.");
    }

    load_from_cfg();

    connect(ui->cs2_button, &QPushButton::clicked, this, &Importer::select_cs2_folder);
    connect(ui->s1_button, &QPushButton::clicked, this, &Importer::select_s1_folder);
    connect(ui->vmf_button, &QPushButton::clicked, this, &Importer::select_vmf);
    connect(ui->bsp_button, &QPushButton::clicked, this, &Importer::select_bsp);
    connect(ui->validate_cs2_button, &QPushButton::clicked, this, &Importer::validate_cs2);
    connect(ui->validate_s1_button, &QPushButton::clicked, this, &Importer::validate_s1);
    connect(ui->addon_edit, &QLineEdit::textChanged, this, &Importer::get_addon_name);
    connect(ui->s1_game_combo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        s1game_basefolder.clear();
        ui->s1_label->setText("Not selected");
        ui->s1_label->setStyleSheet("background-color:rgb(255, 0, 0)");
    });
    connect(ui->go_button, &QPushButton::clicked, this, &Importer::go);

    connect(ui->usebsp_checkbox, &QCheckBox::toggled, this, &Importer::on_usebsp_toggled);
    connect(ui->usebsp_nomergeinstances_checkbox, &QCheckBox::toggled, this, &Importer::on_usebsp_nomergeinstances_toggled);

    connect(ui->usebsp_checkbox, &QCheckBox::stateChanged, this, &Importer::get_launch_options);
    connect(ui->usebsp_nomergeinstances_checkbox, &QCheckBox::stateChanged, this, &Importer::get_launch_options);
    connect(ui->skipdeps_checkbox, &QCheckBox::stateChanged, this, &Importer::get_launch_options);

    // Initial state
    ui->usebsp_nomergeinstances_checkbox->setEnabled(ui->usebsp_checkbox->isChecked());

    log("Initializing CS2 Importer... Finished");
}

Importer::~Importer()
{
    if (log_stream) {
        delete log_stream;
        log_stream = nullptr;
    }
    if (log_file) {
        if (log_file->isOpen()) {
            log_file->close();
        }
        delete log_file;
        log_file = nullptr;
    }
    delete ui;
}

void Importer::log(const QString& message)
{
    ui->log_output->appendPlainText(message);
    // Auto-scroll to bottom
    ui->log_output->moveCursor(QTextCursor::End);

    if (log_stream && log_file && log_file->isOpen()) {
        *log_stream << message << "\n";
        log_stream->flush();
    }
}

void Importer::validate_cs2()
{
    QDesktopServices::openUrl(QUrl("steam://validate/730"));
}

void Importer::validate_s1()
{
    if (s1_game_type == "css") {
        QDesktopServices::openUrl(QUrl("steam://validate/240"));
    } else if (s1_game_type == "csgo") {
        QDesktopServices::openUrl(QUrl("steam://validate/4465480"));
    }
}

void Importer::set_stylesheets()
{
    ui->cs2_label->setStyleSheet("background-color:rgb(255, 0, 0)");
    ui->s1_label->setStyleSheet("background-color:rgb(255, 0, 0)");
    ui->map_label->setStyleSheet("background-color:rgb(255, 0, 0)");
}

void Importer::set_tooltips()
{
    ui->cs2_button->setToolTip("Use \"Counter-Strike Global Offensive\" folder or any folder inside it.");
    ui->s1_button->setToolTip("Use \"csgo legacy\" folder or any folder inside it.");
    ui->vmf_button->setToolTip("Does not need to be in a \"maps\" folder, one will be created then deleted afterwards if necessary.");
    ui->usebsp_checkbox->setToolTip("This runs the map through a special vbsp process to generate clean map geometry from brushes, removing hidden faces and stitching up edges, making the CS2 version easier to work with in Hammer. It preserves world (vis) brushes and func_detail brushes for compatibility with Source 2. This parameter will also merge all func_instances in your map. Note that the final geometry will be triangulated, but cleaning it up is a fairly simple process, which will be explained in another guide.");
    ui->usebsp_nomergeinstances_checkbox->setToolTip("Use this instead of -usebsp if you wish to both generate clean geo and also preserve func_instances. Note that this takes a little longer as it has to run through the import process twice. The final geometry will also be triangulated.");
    ui->skipdeps_checkbox->setToolTip("Optional: skips importing all dependencies/content and only generates the vmap file(s). This provides a 'quick' import when iterating entities for example. Do not run with this if you are importing for the first time.");
}

void Importer::select_cs2_folder()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select a folder:", "C:\\", QFileDialog::ShowDirsOnly);
    if (path.isEmpty()) return;

    QString gameinfo_path = QDir(path).filePath("game/csgo/gameinfo.gi");
    QFile file(gameinfo_path);
    bool valid = false;

    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QRegularExpression regex("^\\s*game\\s+\"Counter-Strike 2\"\\s*$");
        while (!in.atEnd()) {
            if (regex.match(in.readLine()).hasMatch()) {
                valid = true;
                break;
            }
        }
        file.close();
    }

    if (!valid) {
        QMessageBox::critical(this, "Invalid CS2 Folder", "The selected folder is not a valid CS2 installation.\nPlease make sure to select a folder where game/csgo/gameinfo.gi contains 'game \"Counter-Strike 2\"'.");
        QTimer::singleShot(0, this, &Importer::select_cs2_folder);
        return;
    }

    set_cs2_folder(path);
}

void Importer::set_cs2_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        cs2_basefolder = path;
        ui->cs2_label->setText("Selected");
        ui->cs2_label->setStyleSheet("background-color:rgb(0, 255, 0)");
    }
}

void Importer::select_s1_folder()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select a folder:", "C:\\", QFileDialog::ShowDirsOnly);
    if (path.isEmpty()) return;

    QString selected_game = ui->s1_game_combo->currentText();
    bool valid = false;

    if (selected_game == "CSGO") {
        QString gameinfo_path_csgo = QDir(path).filePath("csgo/gameinfo.txt");
        QFile file_csgo(gameinfo_path_csgo);

        if (file_csgo.exists() && file_csgo.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file_csgo);
            QRegularExpression regex("^\\s*game\\s+\"Counter-Strike: Global Offensive\"\\s*$");
            while (!in.atEnd()) {
                if (regex.match(in.readLine()).hasMatch()) {
                    valid = true;
                    s1_game_type = "csgo";
                    break;
                }
            }
            file_csgo.close();
        }
    } else if (selected_game == "CSS") {
        QString gameinfo_path_css = QDir(path).filePath("cstrike/gameinfo.txt");
        QFile file_css(gameinfo_path_css);

        if (file_css.exists() && file_css.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file_css);
            QRegularExpression regex("^\\s*game\\s+\"Counter-Strike Source\"\\s*$");
            while (!in.atEnd()) {
                if (regex.match(in.readLine()).hasMatch()) {
                    valid = true;
                    s1_game_type = "css";
                    break;
                }
            }
            file_css.close();
        }
    }

    if (!valid) {
        if (selected_game == "CSGO") {
            QMessageBox::critical(this, "Invalid Source 1 Folder", "The selected folder is not a valid CS:GO legacy installation.\nPlease make sure to select a folder where csgo/gameinfo.txt contains 'game \"Counter-Strike: Global Offensive\"'.");
        } else {
            QMessageBox::critical(this, "Invalid Source 1 Folder", "The selected folder is not a valid Counter-Strike Source installation.\nPlease make sure to select a folder where cstrike/gameinfo.txt contains 'game \"Counter-Strike Source\"'.");
        }
        QTimer::singleShot(0, this, &Importer::select_s1_folder);
        return;
    }

    set_s1_folder(path);
}

void Importer::set_s1_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        s1game_basefolder = path;
        ui->s1_label->setText("Selected");
        ui->s1_label->setStyleSheet("background-color:rgb(0, 255, 0)");
    }
}

void Importer::select_vmf()
{
    QString path = QFileDialog::getOpenFileName(this, "Select a VMF", vmf_default_path, "VMF files (*.vmf)");
    if (path.isEmpty()) return;

    bsp_file.clear();
    QFileInfo fileInfo(path);
    map_name = fileInfo.baseName();
    content_folder = fileInfo.absolutePath();

    QString target_maps_dir = QDir(app_dir).filePath(QString("maps/%1/maps").arg(map_name));
    QDir().mkpath(target_maps_dir);

    QString target_vmf_path = QDir(target_maps_dir).filePath(fileInfo.fileName());

    if (fileInfo.absoluteFilePath() != target_vmf_path) {
        if (QFile::exists(target_vmf_path)) {
            QFile::remove(target_vmf_path);
        }
        QFile::copy(fileInfo.absoluteFilePath(), target_vmf_path);
    }

    content_folder_to_save = content_folder;
    content_folder = QDir(app_dir).filePath(QString("maps/%1").arg(map_name));
    log("VMF set up at: " + target_vmf_path);

    ui->map_label->setText("Selected");
    ui->map_label->setStyleSheet("background-color:rgb(0, 255, 0)");
}

void Importer::select_bsp()
{
    QString path = QFileDialog::getOpenFileName(this, "Select a BSP", vmf_default_path, "BSP files (*.bsp)");
    if (path.isEmpty()) return;

    bsp_file = path;
    QFileInfo fileInfo(path);
    map_name = fileInfo.baseName();
    content_folder_to_save = fileInfo.absolutePath();

    ui->map_label->setText("Selected");
    ui->map_label->setStyleSheet("background-color:rgb(0, 255, 0)");
}

void Importer::on_usebsp_toggled(bool checked)
{
    ui->usebsp_nomergeinstances_checkbox->setEnabled(checked);
    if (!checked) {
        ui->usebsp_nomergeinstances_checkbox->setChecked(false);
    }
}

void Importer::on_usebsp_nomergeinstances_toggled(bool checked)
{
    if (checked) {
        ui->usebsp_checkbox->setChecked(true);
    }
}

void Importer::get_addon_name()
{
    addon_name = ui->addon_edit->text();
}

void Importer::get_launch_options()
{
    QStringList options;
    if (ui->usebsp_checkbox->isChecked()) {
        if (ui->usebsp_nomergeinstances_checkbox->isChecked()) {
            options.append("-usebsp_nomergeinstances");
        } else {
            options.append("-usebsp");
        }
    }
    if (ui->skipdeps_checkbox->isChecked()) options.append("-skipdeps");
    launch_options = options.join(" ");
}

void Importer::save_to_cfg()
{
    QString usebsp_state = ui->usebsp_checkbox->isChecked() ? "True" : "False";
    QString nomerge_state = ui->usebsp_nomergeinstances_checkbox->isChecked() ? "True" : "False";
    QString skipdeps_state = ui->skipdeps_checkbox->isChecked() ? "True" : "False";

    QString temp = QString("%1\n%2\n%3\n%4\n%5\n%6\n%7")
        .arg(usebsp_state)
        .arg(nomerge_state)
        .arg(skipdeps_state)
        .arg(cs2_basefolder)
        .arg(s1game_basefolder)
        .arg(content_folder_to_save)
        .arg(s1_game_type);

    QFile file("cs2importer.cfg");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << temp;
        file.close();
    }
}

void Importer::load_from_cfg()
{
    QFile file("cs2importer.cfg");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QStringList temp;
        while (!in.atEnd()) {
            temp.append(in.readLine().trimmed());
        }
        file.close();

        if (temp.isEmpty()) return;

        if (temp[0] == "True" || temp[0] == "False") {
            if (temp.size() >= 6) {
                ui->usebsp_checkbox->setChecked(temp[0] == "True");
                ui->usebsp_nomergeinstances_checkbox->setChecked(temp[1] == "True");
                ui->usebsp_nomergeinstances_checkbox->setEnabled(temp[0] == "True");
                ui->skipdeps_checkbox->setChecked(temp[2] == "True");
                set_cs2_folder(temp[3]);

                // For backward compatibility: if s1_game_type wasn't saved, try to infer it.
                if (temp.size() >= 7) {
                    s1_game_type = temp[6];
                } else {
                    s1_game_type = "csgo"; // Assume CSGO for old configs
                }

                if (s1_game_type == "css") {
                    ui->s1_game_combo->setCurrentText("CSS");
                } else {
                    ui->s1_game_combo->setCurrentText("CSGO");
                }

                set_s1_folder(temp[4]);
                vmf_default_path = temp[5];
            }
        } else {
            if (temp.size() == 3) {
                set_cs2_folder(temp[1]);
                vmf_default_path = temp[2];
            } else if (temp.size() >= 4) {
                set_cs2_folder(temp[1]);
                set_s1_folder(temp[2]);
                vmf_default_path = temp[3];
            }
        }
    }
}

void Importer::go()
{
    ui->log_output->clear();

    if (cs2_basefolder.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "CS2 folder not selected.");
        return;
    }
    if (s1game_basefolder.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "CSGO folder not selected.");
        return;
    }
    if (bsp_file.isEmpty() && content_folder.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please select a VMF or BSP file.");
        return;
    }

    try {
        get_addon_name();
        if (addon_name.trimmed().isEmpty()) {
            addon_name = map_name;
        }

        save_to_cfg();

        QString log_dir_path = QDir(app_dir).filePath("log");
        QDir().mkpath(log_dir_path);
        QString log_filename = QString("%1_%2.log")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
            .arg(addon_name);
        QString log_file_path = QDir(log_dir_path).filePath(log_filename);

        if (log_stream) {
            delete log_stream;
            log_stream = nullptr;
        }
        if (log_file) {
            if (log_file->isOpen()) {
                log_file->close();
            }
            delete log_file;
            log_file = nullptr;
        }

        log_file = new QFile(log_file_path);
        if (log_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            log_stream = new QTextStream(log_file);
        } else {
            delete log_file;
            log_file = nullptr;
        }

        AppCore::move_vpk_signatures(cs2_basefolder.toStdString(), vpk_signatures_moved);

        ui->go_button->setEnabled(false);
        log("Starting AppCore thread...");

        AppCore::Options opts;
        opts.cs2_basefolder = cs2_basefolder.replace("/", "\\").toStdString();
        opts.s1game_basefolder = s1game_basefolder.toStdString();
        opts.s1_game_type = s1_game_type.toStdString();
        opts.content_folder = content_folder.toStdString();
        opts.map_name = map_name.toStdString();
        opts.bsp_file = bsp_file.toStdString();
        opts.app_dir = app_dir.toStdString();
        opts.addon_name = addon_name.toStdString();
        opts.usebsp = ui->usebsp_checkbox->isChecked() && !ui->usebsp_nomergeinstances_checkbox->isChecked();
        opts.usebsp_nomergeinstances = ui->usebsp_checkbox->isChecked() && ui->usebsp_nomergeinstances_checkbox->isChecked();
        opts.skipdeps = ui->skipdeps_checkbox->isChecked();

        opts.logger = [this](const std::string& msg) {
            QMetaObject::invokeMethod(this, "log", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(msg)));
        };

        std::thread([this, opts]() mutable {
            bool success = true;
            try {
                if (!opts.bsp_file.empty()) {
                    if (!AppCore::check_java()) {
                        throw std::runtime_error("Java is not installed. Cannot decompile BSP file.");
                    }
                    AppCore::process_bsp(opts);
                }

                MapImporter::Options mapOpts;
                std::string s1_subfolder = opts.s1_game_type == "css" ? "cstrike" : "csgo";

                std::string s1gamedir = opts.s1game_basefolder + "\\" + s1_subfolder;
                for (size_t i = 0; i < s1gamedir.length(); ++i) if (s1gamedir[i] == '/') s1gamedir[i] = '\\';
                mapOpts.s1gamedir = s1gamedir;

                mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";

                std::string contentdir = opts.content_folder;
                for (size_t i = 0; i < contentdir.length(); ++i) if (contentdir[i] == '/') contentdir[i] = '\\';
                mapOpts.s1contentdir = contentdir;

                mapOpts.s2addonname = opts.addon_name;
                mapOpts.s2contentdir = opts.cs2_basefolder + "\\content\\csgo_addons\\" + opts.addon_name;
                mapOpts.mapname = opts.map_name;
                mapOpts.usebsp = opts.usebsp;
                mapOpts.usebsp_nomergeinstances = opts.usebsp_nomergeinstances;
                mapOpts.skipdeps = opts.skipdeps;
                mapOpts.cs2_basefolder = opts.cs2_basefolder;

                MapImporter importer(mapOpts, opts.logger);
                success = importer.Run();

            } catch (const std::exception& e) {
                opts.logger(std::string("Error: ") + e.what());
                success = false;
            }

            QMetaObject::invokeMethod(this, [this, success]() {
                ui->go_button->setEnabled(true);
                if (success) {
                    log("MapImporter thread finished successfully.");
                } else {
                    log("MapImporter thread finished with errors.");
                }

                if (log_stream) {
                    delete log_stream;
                    log_stream = nullptr;
                }
                if (log_file) {
                    if (log_file->isOpen()) {
                        log_file->close();
                    }
                    delete log_file;
                    log_file = nullptr;
                }
            }, Qt::QueuedConnection);

        }).detach();

    } catch (const std::exception& e) {
        log(QString("Error: %1").arg(e.what()));
        QMessageBox::critical(this, "Error", e.what());
    }
}

void Importer::closeEvent(QCloseEvent *event)
{
    if (vpk_signatures_moved && !cs2_basefolder.isEmpty()) {
        AppCore::restore_vpk_signatures(cs2_basefolder.toStdString());
    }
    event->accept();
}
