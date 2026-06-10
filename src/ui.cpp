#include <stdexcept>
#include "ui.h"
#include "mapimporter.h"
#include "appcore.h"

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
#include <QSettings>
#include <QThread>

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
        emit s1gameBasefolderChanged();
        emit s1GameTypeChanged();
        updateCanGo();
        save_to_cfg();
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
        save_to_cfg();
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
        if (s1_game_type == "css") {
            cssgamedir = path;
        } else {
            csgogamedir = path;
        }
        emit s1gameBasefolderChanged();
        updateCanGo();
        save_to_cfg();
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
    QSettings settings("cs2importer.cfg", QSettings::IniFormat);
    settings.setValue("usebsp", usebsp);
    settings.setValue("usebsp_nomergeinstances", usebsp_nomergeinstances);
    settings.setValue("skipdeps", skipdeps);
    settings.setValue("cs2_basefolder", cs2_basefolder);
    settings.setValue("csgogamedir", csgogamedir);
    settings.setValue("cssgamedir", cssgamedir);
    settings.setValue("content_folder_to_save", content_folder_to_save);
    settings.setValue("s1_game_type", s1_game_type);
}

void Backend::load_from_cfg()
{
    QSettings settings("cs2importer.cfg", QSettings::IniFormat);

    // Check if all necessary parameters are present
    if (!settings.contains("usebsp") ||
        !settings.contains("usebsp_nomergeinstances") ||
        !settings.contains("skipdeps") ||
        !settings.contains("cs2_basefolder") ||
        !settings.contains("csgogamedir") ||
        !settings.contains("cssgamedir") ||
        !settings.contains("content_folder_to_save") ||
        !settings.contains("s1_game_type")) {
        // Not all parameters found (could be missing file, or older format). Rewrite with defaults.
        save_to_cfg();
        return;
    }

    usebsp = settings.value("usebsp").toBool();
    usebsp_nomergeinstances = settings.value("usebsp_nomergeinstances").toBool();
    skipdeps = settings.value("skipdeps").toBool();
    cs2_basefolder = settings.value("cs2_basefolder").toString();
    csgogamedir = settings.value("csgogamedir").toString();
    cssgamedir = settings.value("cssgamedir").toString();
    content_folder_to_save = settings.value("content_folder_to_save").toString();
    vmf_default_path = content_folder_to_save;
    s1_game_type = settings.value("s1_game_type").toString();

    if (s1_game_type != "csgo" && s1_game_type != "css") {
        s1_game_type = "csgo";
    }

    emit cs2BasefolderChanged();
    emit s1gameBasefolderChanged();
    emit s1GameTypeChanged();
    emit vmfDefaultPathUrlChanged();
    emit usebspChanged();
    emit usebspNomergeinstancesChanged();
    emit skipdepsChanged();

    updateCanGo();
    get_launch_options();
}

void Backend::go()
{
    if (cs2_basefolder.isEmpty()) {
        emit alertMessage("Validation Error", "CS2 folder not selected.");
        return;
    }
    if (getS1gameBasefolder().isEmpty()) {
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

        AppCore::cancel_import = 0;
        AppCore::move_vpk_signatures(cs2_basefolder, vpk_signatures_moved);

        is_going = true;
        updateCanGo();

        log("Starting AppCore thread...");

        AppCore::Options opts;
        opts.cs2_basefolder = cs2_basefolder;
        opts.cs2_basefolder.replace("/", "\\");
        opts.s1game_basefolder = getS1gameBasefolder();
        opts.s1_game_type = s1_game_type;
        opts.content_folder = content_folder;
        opts.map_name = map_name;
        opts.bsp_file = bsp_file;
        opts.app_dir = app_dir;
        opts.addon_name = addon_name;
        opts.usebsp = usebsp && !usebsp_nomergeinstances;
        opts.usebsp_nomergeinstances = usebsp && usebsp_nomergeinstances;
        opts.skipdeps = skipdeps;

        opts.logger = [this](const QString& msg) {
            QMetaObject::invokeMethod(this, "log", Qt::QueuedConnection, Q_ARG(QString, msg));
        };

        QThread* workerThread = QThread::create([this, opts]() mutable {
            bool success = true;
            try {
                if (!opts.bsp_file.isEmpty()) {
                    if (!AppCore::check_java()) {
                        throw std::runtime_error("Java is not installed. Cannot decompile BSP file.");
                    }
                    AppCore::process_bsp(opts);
                }

                MapImporter::Options mapOpts;
                QString s1_subfolder = opts.s1_game_type == "css" ? "cstrike" : "csgo";

                QString s1gamedir = opts.s1game_basefolder + "\\" + s1_subfolder;
                s1gamedir.replace('/', '\\');
                mapOpts.s1gamedir = s1gamedir;

                mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";

                QString contentdir = opts.content_folder;
                contentdir.replace('/', '\\');
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
                opts.logger(QString("Error: ") + e.what());
                success = false;
            }

            QMetaObject::invokeMethod(this, [this, success]() {
                if (vpk_signatures_moved && !cs2_basefolder.isEmpty()) {
                    AppCore::restore_vpk_signatures(cs2_basefolder);
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
        });

        connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
        workerThread->start();

    } catch (const std::exception& e) {
        log(QString("Error: %1").arg(e.what()));
        emit alertMessage("Error", e.what());
    }
}

void Backend::appAboutToQuit()
{
    AppCore::cancel_all();
    if (vpk_signatures_moved && !cs2_basefolder.isEmpty()) {
        AppCore::restore_vpk_signatures(cs2_basefolder);
        vpk_signatures_moved = false;
    }
}
