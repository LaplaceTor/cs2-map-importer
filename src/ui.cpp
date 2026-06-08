#include "ui.h"
#include "mapimporter.h"
#include "appcore.h"

#include <thread>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>

Backend::Backend(QObject *parent) :
    QObject(parent),
    java_installed(false),
    vmf_default_path("C:\\"),
    s1_game_type("csgo"),
    content_folder_to_save("C:\\"),
    vpk_signatures_moved(false),
    log_file(nullptr),
    log_stream(nullptr)

{
    app_dir = QCoreApplication::applicationDirPath();

    log("Initializing CS2 Importer...");

    java_installed = AppCore::check_java();

    get_launch_options();

    if (!java_installed) {
        log("Warning: Java is missing. BSP decompilation disabled.");
    }

    load_from_cfg();

    log("Initializing CS2 Importer... Finished");
}

Backend::~Backend()
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
}

void Backend::log(const QString& message)
{
    emit logMessage(message);

    if (log_stream && log_file && log_file->isOpen()) {
        *log_stream << message << "\n";
        log_stream->flush();
    }
}

void Backend::validate_cs2()
{
    QDesktopServices::openUrl(QUrl("steam://validate/730"));
}

void Backend::validate_s1()
{
    if (s1_game_type == "css") {
        QDesktopServices::openUrl(QUrl("steam://validate/240"));
    } else if (s1_game_type == "csgo") {
        QDesktopServices::openUrl(QUrl("steam://validate/4465480"));
    }
}

void Backend::set_s1_game_type(const QString& type)
{
    if (s1_game_type != type) {
        s1_game_type = type;
        s1game_basefolder.clear();
        emit s1gameBasefolderChanged();
        emit s1GameTypeChanged();
        updateCanGo();
    }
}

void Backend::select_cs2_folder_dialog(const QUrl& url)
{
    QString path = url.toLocalFile();
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
        emit alertMessage("Invalid CS2 Folder", "The selected folder is not a valid CS2 installation.\nPlease make sure to select a folder where game/csgo/gameinfo.gi contains 'game \"Counter-Strike 2\"'.");
        return;
    }

    set_cs2_folder(path);
}

void Backend::set_cs2_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        cs2_basefolder = path;
        emit cs2BasefolderChanged();
        updateCanGo();
    }
}

void Backend::select_s1_folder_dialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    bool valid = false;

    if (s1_game_type == "csgo") {
        QString gameinfo_path_csgo = QDir(path).filePath("csgo/gameinfo.txt");
        QFile file_csgo(gameinfo_path_csgo);

        if (file_csgo.exists() && file_csgo.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file_csgo);
            QRegularExpression regex("^\\s*game\\s+\"Counter-Strike: Global Offensive\"\\s*$");
            while (!in.atEnd()) {
                if (regex.match(in.readLine()).hasMatch()) {
                    valid = true;
                    break;
                }
            }
            file_csgo.close();
        }

    } else if (s1_game_type == "css") {
        QString gameinfo_path_css = QDir(path).filePath("cstrike/gameinfo.txt");
        QFile file_css(gameinfo_path_css);

        if (file_css.exists() && file_css.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file_css);
            QRegularExpression regex("^\\s*game\\s+\"Counter-Strike Source\"\\s*$");
            while (!in.atEnd()) {
                if (regex.match(in.readLine()).hasMatch()) {
                    valid = true;
                    break;
                }
            }
            file_css.close();
        }
    }

    if (!valid) {
        emit alertMessage("Invalid Source 1 Folder", "The selected folder is not a valid CSGO/CSS installation.\nPlease make sure it is a \"csgo legacy\" or \"Counter-Strike Source\" directory containing the gameinfo.txt.");
        return;
    }

    set_s1_folder(path);
}

void Backend::set_s1_folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        s1game_basefolder = path;
        emit s1gameBasefolderChanged();
        updateCanGo();
    }
}

void Backend::select_vmf_dialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    bsp_file.clear();
    emit bspFileChanged();

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
    vmf_default_path = content_folder_to_save;
    emit vmfDefaultPathUrlChanged();

    emit contentFolderChanged();

    log("VMF set up at: " + target_vmf_path);

    updateCanGo();
}

void Backend::select_bsp_dialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    if (!java_installed) {
        emit alertMessage("Java Missing", "Java is not installed. Cannot decompile BSP file.");
        return;
    }

    bsp_file = path;
    emit bspFileChanged();
    QFileInfo fileInfo(path);
    map_name = fileInfo.baseName();
    content_folder.clear();
    content_folder_to_save = fileInfo.absolutePath();
    vmf_default_path = content_folder_to_save;
    emit vmfDefaultPathUrlChanged();
    emit contentFolderChanged();

    updateCanGo();
}

void Backend::get_launch_options()
{
    QStringList options;
    if (usebsp) {
        if (usebsp_nomergeinstances) {
            options.append("-usebsp_nomergeinstances");
        } else {
            options.append("-usebsp");
        }
    }
    if (skipdeps) options.append("-skipdeps");
    launch_options = options.join(" ");
}

void Backend::updateCanGo()
{
    emit canGoChanged();
}

void Backend::save_to_cfg()
{
    QString usebsp_state = usebsp ? "True" : "False";
    QString nomerge_state = usebsp_nomergeinstances ? "True" : "False";
    QString skipdeps_state = skipdeps ? "True" : "False";

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

void Backend::load_from_cfg()
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
                setUsebsp(temp[0] == "True");
                setUsebspNomergeinstances(temp[1] == "True");
                setSkipdeps(temp[2] == "True");
                set_cs2_folder(temp[3]);

                if (temp.size() >= 7) {
                    set_s1_game_type(temp[6]);
                } else {
                    set_s1_game_type("csgo");
                }

                set_s1_folder(temp[4]);
                vmf_default_path = temp[5];
                emit vmfDefaultPathUrlChanged();
            }
        } else {
            if (temp.size() == 3) {
                set_cs2_folder(temp[1]);
                vmf_default_path = temp[2];
                emit vmfDefaultPathUrlChanged();
            } else if (temp.size() >= 4) {
                set_cs2_folder(temp[1]);
                set_s1_folder(temp[2]);
                vmf_default_path = temp[3];
                emit vmfDefaultPathUrlChanged();
            }
        }
    }
}

void Backend::go()
{
    if (cs2_basefolder.isEmpty()) {
        emit alertMessage("Validation Error", "CS2 folder not selected.");
        return;
    }
    if (s1game_basefolder.isEmpty()) {
        emit alertMessage("Validation Error", "CSGO/CSS folder not selected.");
        return;
    }
    if (bsp_file.isEmpty() && content_folder.isEmpty()) {
        emit alertMessage("Validation Error", "Please select a VMF or BSP file.");
        return;
    }

    try {
        if (addon_name.trimmed().isEmpty()) {
            addon_name = map_name;
            emit addonNameChanged();
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

        AppCore::cancel_import = false;
        AppCore::move_vpk_signatures(cs2_basefolder.toStdString(), vpk_signatures_moved);

        is_going = true;
        updateCanGo();

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
        opts.usebsp = usebsp && !usebsp_nomergeinstances;
        opts.usebsp_nomergeinstances = usebsp && usebsp_nomergeinstances;
        opts.skipdeps = skipdeps;

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
                if (vpk_signatures_moved && !cs2_basefolder.isEmpty()) {
                    AppCore::restore_vpk_signatures(cs2_basefolder.toStdString());
                    vpk_signatures_moved = false;
                }

                is_going = false;
                updateCanGo();

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
        emit alertMessage("Error", e.what());
    }
}

void Backend::appAboutToQuit()
{
    AppCore::cancel_all();
    if (vpk_signatures_moved && !cs2_basefolder.isEmpty()) {
        AppCore::restore_vpk_signatures(cs2_basefolder.toStdString());
        vpk_signatures_moved = false;
    }
}
