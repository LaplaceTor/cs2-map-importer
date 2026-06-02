#include "cs2importer.h"
#include "ui/ui_interface.h"
#include "mapimporter.h"

#include <QFileDialog>
#include <thread>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QProcess>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QTimer>
#include <QProcessEnvironment>
#include <QDebug>
#include <QDateTime>

Importer::Importer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    bspsrc_installed(false),
    java_installed(false),
    vmf_default_path("C:\\"),
    content_folder_to_save("C:\\"),
    vpk_signatures_moved(false)

{
    ui->setupUi(this);

    app_dir = QCoreApplication::applicationDirPath();


    log("Initializing CS2 Importer...");

    java_installed = check_java();
    bspsrc_installed = check_bspsrc(app_dir);

    set_tooltips();
    set_stylesheets();
    get_addon_name();
    get_launch_options();

    if (!java_installed || !bspsrc_installed) {
        ui->bsp_button->setToolTip("Java or bspsrc.jar is missing. BSP decompilation is disabled.");
        ui->bsp_button->setEnabled(false);
        log("Warning: Java or bspsrc.jar is missing. BSP decompilation disabled.");
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
}

Importer::~Importer()
{
    delete ui;
}

void Importer::log(const QString& message)
{
    ui->log_output->appendPlainText(message);
    // Auto-scroll to bottom
    ui->log_output->moveCursor(QTextCursor::End);
}

bool Importer::check_java()
{
    QProcess javaProc;
    javaProc.start("java", QStringList() << "-version");
    javaProc.waitForFinished(-1);
    // Usually prints to stderr
    QString output = QString::fromLocal8Bit(javaProc.readAllStandardError());
    if (output.contains("version") || javaProc.exitCode() == 0) {
        log("Java is installed.");
        return true;
    }
    log("Java is NOT installed or not in PATH.");
    return false;
}

bool Importer::check_bspsrc(const QString& base_path)
{
    QString bspsrc_path = QDir(base_path).filePath("bspsrc.jar");
    if (QFile::exists(bspsrc_path)) {
        log("bspsrc.jar found.");
        return true;
    }
    log("bspsrc.jar not found at " + bspsrc_path);
    return false;
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
    ui->config_checkbox->setToolTip("Auto-selects folders, auto-selects .VMF folder when you open the dialog, and auto-fills launch options for next time.");
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
    if (checked) {
        ui->usebsp_nomergeinstances_checkbox->setChecked(false);
    }
}

void Importer::on_usebsp_nomergeinstances_toggled(bool checked)
{
    if (checked) {
        ui->usebsp_checkbox->setChecked(false);
    }
}

void Importer::get_addon_name()
{
    addon_name = ui->addon_edit->text();
}

void Importer::get_launch_options()
{
    QStringList options;
    if (ui->usebsp_checkbox->isChecked()) options.append("-usebsp");
    if (ui->usebsp_nomergeinstances_checkbox->isChecked()) options.append("-usebsp_nomergeinstances");
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

void Importer::fix_top_level_key(const QString& vmf_path)
{
    QFile file(vmf_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    QString mapversion = "2";
    QRegularExpression mapversion_regex("^\\s*\"mapversion\"\\s+\"([^\"]+)\"");

    bool mapversion_found = false;
    for (const QString& line : lines) {
        QRegularExpressionMatch match = mapversion_regex.match(line);
        if (match.hasMatch()) {
            mapversion = match.captured(1);
            mapversion_found = true;
            break;
        }
    }

    if (!mapversion_found) {
        log("No mapversion found in VMF. Aborting fix.");
        return;
    }

    int visgroups_start_idx = -1;
    int visgroups_end_idx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == "visgroups") {
            visgroups_start_idx = i;
            break;
        }
    }

    QStringList visgroups_lines;
    if (visgroups_start_idx != -1) {
        int open_brackets = 0;
        bool found_first_bracket = false;
        for (int i = visgroups_start_idx; i < lines.size(); ++i) {
            open_brackets += lines[i].count('{');
            open_brackets -= lines[i].count('}');
            if (lines[i].contains('{')) {
                found_first_bracket = true;
            }

            if (found_first_bracket && open_brackets == 0) {
                visgroups_end_idx = i;
                break;
            }
        }

        if (visgroups_end_idx != -1) {
            for (int i = visgroups_start_idx; i <= visgroups_end_idx; ++i) {
                visgroups_lines.append(lines[i]);
            }
            // Remove the lines backwards to avoid shifting index issues
            for (int i = visgroups_end_idx; i >= visgroups_start_idx; --i) {
                lines.removeAt(i);
            }
        }
    } else {
        log("No visgroups block found in VMF. Aborting fix.");
        return;
    }

    QString versioninfo_block = QString("versioninfo\n{\n\t\"editorversion\" \"400\"\n\t\"editorbuild\" \"9999\"\n\t\"mapversion\" \"%1\"\n\t\"formatversion\" \"100\"\n\t\"prefab\" \"0\"\n}").arg(mapversion);

    lines.insert(0, versioninfo_block);

    if (!visgroups_lines.isEmpty()) {
        for (int i = 0; i < visgroups_lines.size(); ++i) {
            lines.insert(i + 1, visgroups_lines[i]);
        }
    }

    QString viewsettings_block = "viewsettings\n{\n\t\"bSnapToGrid\" \"1\"\n\t\"bShowGrid\" \"1\"\n\t\"bShowLogicalGrid\" \"0\"\n\t\"nGridSpacing\" \"64\"\n\t\"bShow3DGrid\" \"0\"\n}";

    int insert_idx = 1 + visgroups_lines.size();
    lines.insert(insert_idx, viewsettings_block);

    QString cordon_block = "cordon\n{\n\t\"mins\" \"(-1024 -1024 -1024)\"\n\t\"maxs\" \"(1024 1024 1024)\"\n\t\"active\" \"0\"\n}";
    lines.append(cordon_block);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setCodec("UTF-8");
        for (const QString& line : lines) {
            out << line << "\n";
        }
        file.close();
    }
}

void Importer::move_vpk_signatures()
{
    if (cs2_basefolder.isEmpty()) return;

    QDir bin_folder(QDir(cs2_basefolder).filePath("game/bin/win64"));
    QString vpk_path = bin_folder.filePath("vpk.signatures");
    QDir temp_folder(bin_folder.filePath("temp"));
    QString temp_vpk_path = temp_folder.filePath("vpk.signatures");

    if (QFile::exists(vpk_path)) {
        if (!temp_folder.exists()) {
            bin_folder.mkpath("temp");
        }
        if (QFile::exists(temp_vpk_path)) {
            QFile::remove(temp_vpk_path);
        }
        QFile::rename(vpk_path, temp_vpk_path);
        vpk_signatures_moved = true;
    }
}

void Importer::go()
{
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
        if (ui->config_checkbox->isChecked()) {
            save_to_cfg();
        }

        move_vpk_signatures();

        if (!bsp_file.isEmpty()) {
            if (!java_installed) {
                throw std::runtime_error("Java is not installed. Cannot decompile BSP file.");
            }

            QString maps_dir = QDir(app_dir).filePath("maps");
            QDir().mkpath(maps_dir);

            QString vmf_dest = QDir(maps_dir).filePath(map_name + ".vmf");
            QString bspsrc_jar = QDir(app_dir).filePath("bspsrc.jar");

            if (!QFile::exists(bspsrc_jar)) {
                throw std::runtime_error("Could not find bspsrc.jar at " + bspsrc_jar.toStdString());
            }

            log("Decompiling BSP: " + bsp_file);

            QProcess decompProc;
            decompProc.setProcessChannelMode(QProcess::MergedChannels);
            QStringList args;
            args << "-jar" << bspsrc_jar << bsp_file << "-o" << vmf_dest << "--unpack_embedded";
            decompProc.start("java", args);
            decompProc.waitForFinished(-1);
            log(QString::fromLocal8Bit(decompProc.readAll()));

            if (decompProc.exitCode() != 0) {
                throw std::runtime_error("BSP Decompilation failed.");
            }

            QString unpacked_dir;
            QStringList possible_locations = {
                QDir::current().filePath(map_name),
                QDir(app_dir).filePath(map_name),
                QFileInfo(bsp_file).absoluteDir().filePath(map_name),
                QDir(maps_dir).filePath(map_name)
            };

            for (const QString& loc : possible_locations) {
                if (QDir(loc).exists()) {
                    unpacked_dir = loc;
                    break;
                }
            }

            if (!unpacked_dir.isEmpty()) {
                log("Found unpacked files at " + unpacked_dir);

                if (!s1game_basefolder.isEmpty()) {
                    QStringList foldersToCopy = {"materials", "models"};
                    QString s1_subfolder = s1_game_type == "css" ? "cstrike/" : "csgo/";
                    for (const QString& folder_name : foldersToCopy) {
                        QDir src_folder(QDir(unpacked_dir).filePath(folder_name));
                        if (src_folder.exists()) {
                            QDir dest_folder(QDir(s1game_basefolder).filePath(s1_subfolder + folder_name));
                            log("Copying " + src_folder.absolutePath() + " to " + dest_folder.absolutePath());
                            // Recursive copy function would be needed here, or call system xcopy/cp
                            // For simplicity, calling system xcopy on Windows
                            QString srcPathStr = src_folder.absolutePath().replace("/", "\\");
                            QString destPathStr = dest_folder.absolutePath().replace("/", "\\");
                            QProcess xcopyProc;
                            xcopyProc.start("xcopy", QStringList() << srcPathStr << destPathStr << "/E" << "/I" << "/Y");
                            xcopyProc.waitForFinished(-1);
                        }
                    }
                }

                QString target_unpacked_dir = QDir(maps_dir).filePath(map_name);
                if (unpacked_dir != target_unpacked_dir) {
                    // QDir::rename works if target doesn't exist.
                    // If moving fails (e.g., across drives), one needs full copy/delete.
                    // Using system move for reliability on Windows if needed, or simple rename.
                    if (QDir(target_unpacked_dir).exists()) {
                        QDir(target_unpacked_dir).removeRecursively();
                    }
                    if (QDir().rename(unpacked_dir, target_unpacked_dir)) {
                         log("Moved unpacked directory to " + target_unpacked_dir);
                    } else {
                         log("Failed to rename unpacked directory to " + target_unpacked_dir + ". Attempting move command...");
                         QProcess moveProc;
                         moveProc.start("cmd.exe", QStringList() << "/c" << "move" << "/y" << unpacked_dir.replace("/", "\\") << target_unpacked_dir.replace("/", "\\"));
                         moveProc.waitForFinished(-1);
                    }
                }
            } else {
                log("Could not find unpacked embedded files directory '" + map_name + "'");
            }

            fix_top_level_key(vmf_dest);

            // Move vmf to maps/<map_name>/maps/
            QString target_maps_dir = QDir(app_dir).filePath(QString("maps/%1/maps").arg(map_name));
            QDir().mkpath(target_maps_dir);
            QString final_vmf_dest = QDir(target_maps_dir).filePath(map_name + ".vmf");

            if (QFile::exists(final_vmf_dest)) {
                QFile::remove(final_vmf_dest);
            }
            if (QFile::rename(vmf_dest, final_vmf_dest)) {
                 log("Moved VMF to: " + final_vmf_dest);
            } else {
                 log("Failed to move VMF to: " + final_vmf_dest);
            }

            content_folder = QDir(app_dir).filePath(QString("maps/%1").arg(map_name));
            log("Decompiled and prepared at: " + final_vmf_dest);
        }

        ui->go_button->setEnabled(false);
        log("Starting MapImporter thread...");

        MapImporter::Options opts;
        QString s1_subfolder = s1_game_type == "css" ? "cstrike" : "csgo";
        opts.s1gamedir = QDir(s1game_basefolder).filePath(s1_subfolder).replace("/", "\\").toStdString();
        opts.s1gamename = s1_game_type == "css" ? "css" : "csgo";
        opts.s1contentdir = content_folder.replace("/", "\\").toStdString();
        opts.s2addonname = addon_name.toStdString();
        opts.s2contentdir = QDir(cs2_basefolder).filePath("content/csgo_addons/" + addon_name).replace("/", "\\").toStdString();
        opts.mapname = map_name.toStdString();
        opts.usebsp = ui->usebsp_checkbox->isChecked();
        opts.usebsp_nomergeinstances = ui->usebsp_nomergeinstances_checkbox->isChecked();
        opts.skipdeps = ui->skipdeps_checkbox->isChecked();
        opts.cs2_basefolder = cs2_basefolder.replace("/", "\\").toStdString();

        std::thread([this, opts]() {
            MapImporter importer(opts, [this](const std::string& msg) {
                QMetaObject::invokeMethod(this, "log", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(msg)));
            });

            bool success = importer.Run();

            QMetaObject::invokeMethod(this, [this, success]() {
                ui->go_button->setEnabled(true);
                if (success) {
                    log("MapImporter thread finished successfully.");
                } else {
                    log("MapImporter thread finished with errors.");
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
        QDir bin_folder(QDir(cs2_basefolder).filePath("game/bin/win64"));
        QString vpk_path = bin_folder.filePath("vpk.signatures");
        QString temp_vpk_path = bin_folder.filePath("temp/vpk.signatures");
        if (QFile::exists(temp_vpk_path)) {
            if (QFile::exists(vpk_path)) {
                QFile::remove(vpk_path);
            }
            QFile::rename(temp_vpk_path, vpk_path);
        }
    }
    event->accept();
}
