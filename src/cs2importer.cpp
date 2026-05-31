#include "cs2importer.h"
#include "ui/ui_interface.h"

#include <QFileDialog>
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
    vmf_folder_to_save("C:\\"),
    vpk_signatures_moved(false),
    process(new QProcess(this))
{
    ui->setupUi(this);

    app_dir = QCoreApplication::applicationDirPath();

    connect(process, &QProcess::readyReadStandardOutput, this, &Importer::appendLogOutput);
    connect(process, &QProcess::readyReadStandardError, this, &Importer::appendLogError);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Importer::processFinished);

    log("Initializing CS2 Importer...");

    check_colorama();
    java_installed = check_java();
    bspsrc_installed = check_bspsrc(app_dir);

    set_tooltips();
    set_stylesheets();
    get_addon();
    get_launch_options();

    if (!java_installed || !bspsrc_installed) {
        ui->bsp_button->setToolTip("Java or bspsrc.jar is missing. BSP decompilation is disabled.");
        ui->bsp_button->setEnabled(false);
        log("Warning: Java or bspsrc.jar is missing. BSP decompilation disabled.");
    }

    load_from_cfg();

    connect(ui->cs2_button, &QPushButton::clicked, this, &Importer::select_cs2_folder);
    connect(ui->csgo_button, &QPushButton::clicked, this, &Importer::select_csgo_folder);
    connect(ui->vmf_button, &QPushButton::clicked, this, &Importer::select_vmf);
    connect(ui->bsp_button, &QPushButton::clicked, this, &Importer::select_bsp);
    connect(ui->validate_cs2_button, &QPushButton::clicked, this, &Importer::validate_cs2);
    connect(ui->validate_csgo_button, &QPushButton::clicked, this, &Importer::validate_csgo);
    connect(ui->addon_edit, &QLineEdit::textChanged, this, &Importer::get_addon);
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

void Importer::appendLogOutput()
{
    QByteArray data = process->readAllStandardOutput();
    QString text = QString::fromLocal8Bit(data);
    python_output += text;
    log(text.trimmed());
}

void Importer::appendLogError()
{
    QByteArray data = process->readAllStandardError();
    QString text = QString::fromLocal8Bit(data);
    python_output += text;
    log(text.trimmed());
}

void Importer::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        log("Process crashed!");
    } else {
        log(QString("Process finished with exit code %1").arg(exitCode));
    }

    if (!python_output.isEmpty()) {
        QDir log_dir(app_dir);
        if (!log_dir.exists("log")) {
            log_dir.mkdir("log");
        }
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString log_filename = QString("log/%1_%2.log").arg(timestamp, addon);
        QFile log_file(log_dir.filePath(log_filename));
        if (log_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&log_file);
            out << python_output;
            log_file.close();
            log("Saved python output to " + log_filename);
        } else {
            log("Failed to save python output to " + log_filename);
        }
    }
}

void Importer::check_colorama()
{
    log("Checking for Python colorama...");
    QProcess chkProc;
    chkProc.start("python", QStringList() << "-c" << "import colorama");
    chkProc.waitForFinished(-1);
    if (chkProc.exitCode() != 0) {
        log("colorama not found in system python. Installing...");
        QProcess instProc;
        instProc.setProcessChannelMode(QProcess::MergedChannels);
        instProc.start("python", QStringList() << "-m" << "pip" << "install" << "colorama");
        instProc.waitForFinished(-1);
        log(QString::fromLocal8Bit(instProc.readAll()));
    } else {
        log("colorama found.");
    }
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

void Importer::validate_csgo()
{
    QDesktopServices::openUrl(QUrl("steam://validate/4465480"));
}

void Importer::set_stylesheets()
{
    ui->cs2_label->setStyleSheet("background-color:rgb(255, 0, 0)");
    ui->csgo_label->setStyleSheet("background-color:rgb(255, 0, 0)");
    ui->vmf_label->setStyleSheet("background-color:rgb(255, 0, 0)");
}

void Importer::set_tooltips()
{
    ui->cs2_button->setToolTip("Use \"Counter-Strike Global Offensive\" folder or any folder inside it.");
    ui->csgo_button->setToolTip("Use \"csgo legacy\" folder or any folder inside it.");
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

    QStringList parts = path.split("/Counter-Strike Global Offensive");

    QString newPath = parts[0] + "/Counter-Strike Global Offensive";
    set_cs2_folder(newPath);
}

void Importer::set_cs2_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        cs2_basefolder = path;
        ui->cs2_label->setText(path);
        ui->cs2_label->setStyleSheet("background-color:rgb(0, 255, 0)");
    }
}

void Importer::select_csgo_folder()
{
    QString path = QFileDialog::getExistingDirectory(this, "Select a folder:", "C:\\", QFileDialog::ShowDirsOnly);
    if (path.isEmpty()) return;

    QStringList parts = path.split("/csgo legacy");

    QString newPath = parts[0] + "/csgo legacy";
    set_csgo_folder(newPath);
}

void Importer::set_csgo_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        csgo_basefolder = path;
        ui->csgo_label->setText(path);
        ui->csgo_label->setStyleSheet("background-color:rgb(0, 255, 0)");
    }
}

void Importer::select_vmf()
{
    QString path = QFileDialog::getOpenFileName(this, "Select a VMF", vmf_default_path, "VMF files (*.vmf)");
    if (path.isEmpty()) return;

    bsp_file.clear();
    QFileInfo fileInfo(path);
    map_name = fileInfo.baseName();
    vmf_folder = fileInfo.absolutePath();

    QString target_maps_dir = QDir(app_dir).filePath("maps");
    QDir().mkpath(target_maps_dir);

    QString target_vmf_path = QDir(target_maps_dir).filePath(fileInfo.fileName());

    if (fileInfo.absoluteFilePath() != target_vmf_path) {
        if (QFile::exists(target_vmf_path)) {
            QFile::remove(target_vmf_path);
        }
        QFile::copy(fileInfo.absoluteFilePath(), target_vmf_path);
    }

    vmf_folder_to_save = vmf_folder;
    vmf_folder = app_dir;
    log("VMF set up at: " + target_vmf_path);

    ui->vmf_label->setText(path);
    ui->vmf_label->setStyleSheet("background-color:rgb(0, 255, 0)");
}

void Importer::select_bsp()
{
    QString path = QFileDialog::getOpenFileName(this, "Select a BSP", vmf_default_path, "BSP files (*.bsp)");
    if (path.isEmpty()) return;

    bsp_file = path;
    QFileInfo fileInfo(path);
    map_name = fileInfo.baseName();
    vmf_folder_to_save = fileInfo.absolutePath();

    ui->vmf_label->setText(path);
    ui->vmf_label->setStyleSheet("background-color:rgb(0, 255, 0)");
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

void Importer::get_addon()
{
    addon = ui->addon_edit->text();
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

    QString temp = QString("%1\n%2\n%3\n%4\n%5\n%6")
        .arg(usebsp_state)
        .arg(nomerge_state)
        .arg(skipdeps_state)
        .arg(cs2_basefolder)
        .arg(csgo_basefolder)
        .arg(vmf_folder_to_save);

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
                set_csgo_folder(temp[4]);
                vmf_default_path = temp[5];
            }
        } else {
            if (temp.size() == 3) {
                set_cs2_folder(temp[1]);
                vmf_default_path = temp[2];
            } else if (temp.size() >= 4) {
                set_cs2_folder(temp[1]);
                set_csgo_folder(temp[2]);
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

void Importer::fix_import_script()
{
    if (cs2_basefolder.isEmpty()) return;

    QString script_path = QDir(cs2_basefolder).filePath("game/csgo/import_scripts/import_map_community.py");
    QFile file(script_path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&file);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    bool modified = false;

    int start_idx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains("Enter to Continue, Esc to Quit")) {
            start_idx = i;
            break;
        }
    }

    if (start_idx != -1 && start_idx + 13 < lines.size()) {
        QString indent = lines[start_idx].left(lines[start_idx].indexOf("utl.print_color"));
        // Remove 14 lines (from start_idx to start_idx + 13)
        for (int j = 0; j < 14; ++j) {
            lines.removeAt(start_idx);
        }
        // Insert bRunImport = True
        lines.insert(start_idx, indent + "bRunImport = True");
        modified = true;
    }

    if (modified) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out.setCodec("UTF-8");
            for (const QString& line : lines) {
                out << line << "\n";
            }
            file.close();
        }
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
    if (csgo_basefolder.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "CSGO folder not selected.");
        return;
    }
    if (bsp_file.isEmpty() && vmf_folder.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please select a VMF or BSP file.");
        return;
    }

    try {
        get_addon();
        if (addon.trimmed().isEmpty()) {
            addon = map_name;
        }
        if (ui->config_checkbox->isChecked()) {
            save_to_cfg();
        }

        fix_import_script();
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

                if (!csgo_basefolder.isEmpty()) {
                    QStringList foldersToCopy = {"materials", "models"};
                    for (const QString& folder_name : foldersToCopy) {
                        QDir src_folder(QDir(unpacked_dir).filePath(folder_name));
                        if (src_folder.exists()) {
                            QDir dest_folder(QDir(csgo_basefolder).filePath("csgo/" + folder_name));
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
            vmf_folder = app_dir;
            log("Decompiled to: " + vmf_dest);
        }

        QString cd = QDir(cs2_basefolder).filePath("game/csgo/import_scripts");

        QStringList args;
        args << "import_map_community.py"
             << QDir(csgo_basefolder).filePath("csgo")
             << vmf_folder
             << QDir(cs2_basefolder).filePath("game/csgo")
             << addon
             << map_name;

        if (ui->usebsp_checkbox->isChecked()) args << "-usebsp";
        if (ui->usebsp_nomergeinstances_checkbox->isChecked()) args << "-usebsp_nomergeinstances";
        if (ui->skipdeps_checkbox->isChecked()) args << "-skipdeps";

        log("Executing Python script: python " + args.join(" "));

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString bin_path = QDir(cs2_basefolder).filePath("game/bin/win64").replace("/", "\\");
        QString currentPath = env.value("PATH");
        env.insert("PATH", bin_path + ";" + currentPath);

        process->setProcessEnvironment(env);
        process->setWorkingDirectory(cd);
        python_output.clear();
        process->start("python", args);

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
