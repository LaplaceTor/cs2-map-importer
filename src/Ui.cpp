#include "VmfBspProcess.h"
#include "Ui.h"
#include "MapImporter.h"
#include "Miscellaneous.h"

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
    javaInstalled(false),
    vmfDefaultPath("C:\\"),
    s1GameType("csgo"),
    contentFolderToSave("C:\\"),
    vpkSignaturesMoved(false),
    logFile(nullptr),
    logStream(nullptr)

{
    appDir = QCoreApplication::applicationDirPath();

    Miscellaneous::GlobaLLogger = [this](const QString& msg) {
        emit logMessage(msg);
        if (logStream) {
            *logStream << msg << "\n";
            logStream->flush();
        }
    };

    Miscellaneous::Log("Initializing CS2 Importer...");

    javaInstalled = Miscellaneous::CheckJava();

    GetLaunchOptions();

    if (!javaInstalled) {
        Miscellaneous::Log("Warning: Java is missing. BSP decompilation disabled.");
    }

    LoadFromCfg();

    Miscellaneous::Log("Initializing CS2 Importer... Finished");
}

Backend::~Backend()
{
    if (logStream) {
        delete logStream;
        logStream = nullptr;
    }
    if (logFile) {
        if (logFile->isOpen()) {
            logFile->close();
        }
        delete logFile;
        logFile = nullptr;
    }
}

void Backend::ValidateCs2()
{
    QDesktopServices::openUrl(QUrl("steam://validate/730"));
}

void Backend::ValidateS1()
{
    if (s1GameType == "css") {
        QDesktopServices::openUrl(QUrl("steam://validate/240"));
    } else if (s1GameType == "csgo") {
        QDesktopServices::openUrl(QUrl("steam://validate/4465480"));
    } else if (s1GameType == "hl2") {
        QDesktopServices::openUrl(QUrl("steam://validate/220"));
    } else if (s1GameType == "l4d") {
        QDesktopServices::openUrl(QUrl("steam://validate/500"));
    } else if (s1GameType == "l4d2") {
        QDesktopServices::openUrl(QUrl("steam://validate/550"));
    } else if (s1GameType == "portal") {
        QDesktopServices::openUrl(QUrl("steam://validate/400"));
    } else if (s1GameType == "portal2") {
        QDesktopServices::openUrl(QUrl("steam://validate/620"));
    } else if (s1GameType == "tf2") {
        QDesktopServices::openUrl(QUrl("steam://validate/440"));
    } else if (s1GameType == "gmod") {
        QDesktopServices::openUrl(QUrl("steam://validate/4000"));
    }
}

void Backend::SetS1GameType(const QString& type)
{
    if (s1GameType != type) {
        s1GameType = type;
        emit s1gameBasefolderChanged();
        emit s1GameTypeChanged();
        UpdateCanGo();
        SaveToCfg();
    }
}

bool Backend::IsValidCs2(const QString& path)
{
    if (path.isEmpty()) return false;

    QString gameinfoPath = QDir(path).filePath("game/csgo/gameinfo.gi");
    QFile file(gameinfoPath);
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
    return valid;
}

void Backend::SelectCs2FolderDialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    if (!IsValidCs2(path)) {
        emit alertMessage("Invalid CS2 Folder", "The selected folder is not a valid CS2 installation.\nPlease make sure to select a folder where game/csgo/gameinfo.gi contains 'game \"Counter-Strike 2\"'.");
        return;
    }

    SetCs2Folder(path);
}

void Backend::SetCs2Folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        cs2Basefolder = path;
        emit cs2BasefolderChanged();
        UpdateCanGo();
        SaveToCfg();
    }
}

bool Backend::IsValidS1(const QString& path, const QString& type)
{
    if (path.isEmpty()) return false;

    bool valid = false;

    auto checkGameinfo = [&](const QString& folder, const QString& gamename) {
        QString gameinfoPath = QDir(path).filePath(folder + "/gameinfo.txt");
        QFile file(gameinfoPath);

        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QRegularExpression regex("^\\s*game\\s+\"" + QRegularExpression::escape(gamename) + "\"");
            while (!in.atEnd()) {
                if (regex.match(in.readLine()).hasMatch()) {
                    valid = true;
                    break;
                }
            }
            file.close();
        }
    };

    if (type == "csgo") {
        checkGameinfo("csgo", "Counter-Strike: Global Offensive");
    } else if (type == "css") {
        checkGameinfo("cstrike", "Counter-Strike Source");
    } else if (type == "hl2") {
        checkGameinfo("hl2", "HALF-LIFE 2");
    } else if (type == "l4d") {
        checkGameinfo("left4dead", "Left 4 Dead");
    } else if (type == "l4d2") {
        checkGameinfo("left4dead2", "Left 4 Dead 2");
    } else if (type == "portal") {
        checkGameinfo("portal", "Portal");
    } else if (type == "portal2") {
        checkGameinfo("portal2", "PORTAL 2");
    } else if (type == "tf2") {
        checkGameinfo("tf", "Team Fortress 2");
    } else if (type == "gmod") {
        checkGameinfo("garrysmod", "Garry's Mod");
    }

    return valid;
}

void Backend::SelectS1FolderDialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    if (!IsValidS1(path, s1GameType)) {
        emit alertMessage("Invalid Source 1 Folder", "The selected folder is not a valid installation for the selected game.\nPlease make sure it is the correct directory containing the gameinfo.txt.");
        return;
    }

    SetS1Folder(path);
}

void Backend::AutoDetectPaths()
{
    QString steamPath;
    QSettings regSteam("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam", QSettings::NativeFormat);
    steamPath = regSteam.value("InstallPath").toString();
    if (steamPath.isEmpty()) {
        QSettings regSteam64("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        steamPath = regSteam64.value("InstallPath").toString();
    }

    if (steamPath.isEmpty()) return;

    QString library_vdf = QDir(steamPath).filePath("steamapps/libraryfolders.vdf");
    QFile vdf_file(library_vdf);
    if (!vdf_file.exists() || !vdf_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&vdf_file);
    QString content = in.readAll();
    vdf_file.close();

    struct LibraryData {
        QString path;
        QList<QString> apps;
    };
    QList<LibraryData> libraries;

    QString currentPath;
    QList<QString> currentApps;
    bool in_apps = false;

    QTextStream in2(&content);
    while (!in2.atEnd()) {
        QString line = in2.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith("\"path\"")) {
            QRegularExpression re("\"path\"\\s+\"([^\"]+)\"");
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                if (!currentPath.isEmpty()) {
                    libraries.append({currentPath, currentApps});
                    currentApps.clear();
                }
                currentPath = match.captured(1);
                currentPath.replace("\\\\", "/");
                currentPath.replace("\\", "/");
                in_apps = false;
            }
        } else if (line == "\"apps\"") {
            in_apps = true;
        } else if (in_apps && line == "}") {
            in_apps = false;
        } else if (in_apps && line.startsWith("\"")) {
            QRegularExpression re("\"(\\d+)\"");
            QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch()) {
                currentApps.append(match.captured(1));
            }
        }
    }
    if (!currentPath.isEmpty()) {
        libraries.append({currentPath, currentApps});
    }

    QString found_cs2_dir;
    QString found_csgo_legacy_dir;
    QString found_csgo_cs2_dir;
    QString found_css_dir;
    QString found_hl2_dir;
    QString found_l4d_dir;
    QString found_l4d2_dir;
    QString found_portal_dir;
    QString found_portal2_dir;
    QString found_tf2_dir;
    QString found_gmod_dir;

    for (const auto& lib : libraries) {
        QString base_lib = lib.path;
        QString common_dir = QDir(base_lib).filePath("steamapps/common");

        if (lib.apps.contains("730")) {
            QString cs2_candidate = QDir(common_dir).filePath("Counter-Strike Global Offensive");
            if (IsValidCs2(cs2_candidate)) {
                found_cs2_dir = cs2_candidate;
            }
            if (IsValidS1(cs2_candidate, "csgo")) {
                found_csgo_cs2_dir = cs2_candidate;
            }
        }

        if (lib.apps.contains("4465480")) {
            QString csgo_legacy_candidate = QDir(common_dir).filePath("csgo legacy");
            if (IsValidS1(csgo_legacy_candidate, "csgo")) {
                found_csgo_legacy_dir = csgo_legacy_candidate;
            }
        }

        if (lib.apps.contains("240")) {
            QString css_candidate = QDir(common_dir).filePath("Counter-Strike Source");
            if (IsValidS1(css_candidate, "css")) {
                found_css_dir = css_candidate;
            }
        }

        if (lib.apps.contains("220")) {
            QString hl2_candidate = QDir(common_dir).filePath("Half-Life 2");
            if (IsValidS1(hl2_candidate, "hl2")) {
                found_hl2_dir = hl2_candidate;
            }
        }

        if (lib.apps.contains("500")) {
            QString l4d_candidate = QDir(common_dir).filePath("Left 4 Dead");
            if (IsValidS1(l4d_candidate, "l4d")) {
                found_l4d_dir = l4d_candidate;
            }
        }

        if (lib.apps.contains("550")) {
            QString l4d2_candidate = QDir(common_dir).filePath("Left 4 Dead 2");
            if (IsValidS1(l4d2_candidate, "l4d2")) {
                found_l4d2_dir = l4d2_candidate;
            }
        }

        if (lib.apps.contains("400")) {
            QString portal_candidate = QDir(common_dir).filePath("Portal");
            if (IsValidS1(portal_candidate, "portal")) {
                found_portal_dir = portal_candidate;
            }
        }

        if (lib.apps.contains("620")) {
            QString portal2_candidate = QDir(common_dir).filePath("Portal 2");
            if (IsValidS1(portal2_candidate, "portal2")) {
                found_portal2_dir = portal2_candidate;
            }
        }

        if (lib.apps.contains("440")) {
            QString tf2_candidate = QDir(common_dir).filePath("Team Fortress 2");
            if (IsValidS1(tf2_candidate, "tf2")) {
                found_tf2_dir = tf2_candidate;
            }
        }

        if (lib.apps.contains("4000")) {
            QString gmod_candidate = QDir(common_dir).filePath("GarrysMod");
            if (IsValidS1(gmod_candidate, "gmod")) {
                found_gmod_dir = gmod_candidate;
            }
        }
    }

    bool updated = false;

    if (cs2Basefolder.isEmpty() && !found_cs2_dir.isEmpty()) {
        cs2Basefolder = found_cs2_dir;
        emit cs2BasefolderChanged();
        updated = true;
    }

    if (cssgamedir.isEmpty() && !found_css_dir.isEmpty()) {
        cssgamedir = found_css_dir;
        updated = true;
    }

    if (csgogamedir.isEmpty()) {
        if (!found_csgo_legacy_dir.isEmpty()) {
            csgogamedir = found_csgo_legacy_dir;
            updated = true;
        } else if (!found_csgo_cs2_dir.isEmpty()) {
            csgogamedir = found_csgo_cs2_dir;
            updated = true;
        }
    }

    if (hl2gamedir.isEmpty() && !found_hl2_dir.isEmpty()) {
        hl2gamedir = found_hl2_dir;
        updated = true;
    }

    if (l4dgamedir.isEmpty() && !found_l4d_dir.isEmpty()) {
        l4dgamedir = found_l4d_dir;
        updated = true;
    }

    if (l4d2gamedir.isEmpty() && !found_l4d2_dir.isEmpty()) {
        l4d2gamedir = found_l4d2_dir;
        updated = true;
    }

    if (portalgamedir.isEmpty() && !found_portal_dir.isEmpty()) {
        portalgamedir = found_portal_dir;
        updated = true;
    }

    if (portal2gamedir.isEmpty() && !found_portal2_dir.isEmpty()) {
        portal2gamedir = found_portal2_dir;
        updated = true;
    }

    if (tf2gamedir.isEmpty() && !found_tf2_dir.isEmpty()) {
        tf2gamedir = found_tf2_dir;
        updated = true;
    }

    if (gmodgamedir.isEmpty() && !found_gmod_dir.isEmpty()) {
        gmodgamedir = found_gmod_dir;
        updated = true;
    }

    if (updated) {
        UpdateCanGo();
        emit s1gameBasefolderChanged();
        SaveToCfg();
    }
}

void Backend::SetS1Folder(const QString& path)
{
    if (!path.isEmpty() && path != "None") {
        if (s1GameType == "css") {
            cssgamedir = path;
        } else if (s1GameType == "hl2") {
            hl2gamedir = path;
        } else if (s1GameType == "l4d") {
            l4dgamedir = path;
        } else if (s1GameType == "l4d2") {
            l4d2gamedir = path;
        } else if (s1GameType == "portal") {
            portalgamedir = path;
        } else if (s1GameType == "portal2") {
            portal2gamedir = path;
        } else if (s1GameType == "tf2") {
            tf2gamedir = path;
        } else if (s1GameType == "gmod") {
            gmodgamedir = path;
        } else {
            csgogamedir = path;
        }
        emit s1gameBasefolderChanged();
        UpdateCanGo();
        SaveToCfg();
    }
}

void Backend::SelectVmfDialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    bspFile.clear();
    emit bspFileChanged();

    QFileInfo fileInfo(path);
    mapName = fileInfo.baseName();
    contentFolder = fileInfo.absolutePath();

    QString target_maps_dir = QDir(appDir).filePath(QString("maps/%1/maps").arg(mapName));
    QDir().mkpath(target_maps_dir);

    QString target_vmf_path = QDir(target_maps_dir).filePath(fileInfo.fileName());

    if (fileInfo.absoluteFilePath() != target_vmf_path) {
        if (QFile::exists(target_vmf_path)) {
            QFile::remove(target_vmf_path);
        }
        QFile::copy(fileInfo.absoluteFilePath(), target_vmf_path);
    }

    contentFolderToSave = contentFolder;
    contentFolder = QDir(appDir).filePath(QString("maps/%1").arg(mapName));
    vmfDefaultPath = contentFolderToSave;
    emit vmfDefaultPathUrlChanged();

    emit contentFolderChanged();

    addonName = "";
    emit addonNameChanged();

    Miscellaneous::Log("VMF set up at: " + target_vmf_path);

    UpdateCanGo();
}

void Backend::SelectBspDialog(const QUrl& url)
{
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    if (!javaInstalled) {
        emit alertMessage("Java Missing", "Java is not installed. Cannot decompile BSP file.");
        return;
    }

    bspFile = path;
    emit bspFileChanged();
    QFileInfo fileInfo(path);
    mapName = fileInfo.baseName();
    contentFolder.clear();
    contentFolderToSave = fileInfo.absolutePath();
    vmfDefaultPath = contentFolderToSave;
    emit vmfDefaultPathUrlChanged();
    emit contentFolderChanged();

    addonName = "";
    emit addonNameChanged();

    UpdateCanGo();
}

void Backend::GetLaunchOptions()
{
    QStringList options;
    if (usebsp) {
        if (usebspNomergeinstances) {
            options.append("-usebsp_nomergeinstances");
        } else {
            options.append("-usebsp");
        }
    }
    if (skipdeps) options.append("-skipdeps");
    launchOptions = options.join(" ");
}

void Backend::UpdateCanGo()
{
    emit canGoChanged();
    emit isGoingChanged();
}

void Backend::SaveToCfg()
{
    QSettings settings("cs2importer.cfg", QSettings::IniFormat);
    settings.setValue("usebsp", usebsp);
    settings.setValue("usebsp_nomergeinstances", usebspNomergeinstances);
    settings.setValue("skipdeps", skipdeps);
    settings.setValue("cs2_basefolder", cs2Basefolder);
    settings.setValue("csgogamedir", csgogamedir);
    settings.setValue("cssgamedir", cssgamedir);
    settings.setValue("hl2gamedir", hl2gamedir);
    settings.setValue("l4dgamedir", l4dgamedir);
    settings.setValue("l4d2gamedir", l4d2gamedir);
    settings.setValue("portalgamedir", portalgamedir);
    settings.setValue("portal2gamedir", portal2gamedir);
    settings.setValue("tf2gamedir", tf2gamedir);
    settings.setValue("gmodgamedir", gmodgamedir);
    settings.setValue("content_folder_to_save", contentFolderToSave);
    settings.setValue("s1_game_type", s1GameType);
}

void Backend::LoadFromCfg()
{
    QSettings settings("cs2importer.cfg", QSettings::IniFormat);

    usebsp = settings.value("usebsp", true).toBool();
    usebspNomergeinstances = settings.value("usebsp_nomergeinstances", false).toBool();
    skipdeps = settings.value("skipdeps", false).toBool();
    cs2Basefolder = settings.value("cs2_basefolder", "").toString();
    csgogamedir = settings.value("csgogamedir", "").toString();
    cssgamedir = settings.value("cssgamedir", "").toString();
    hl2gamedir = settings.value("hl2gamedir", "").toString();
    l4dgamedir = settings.value("l4dgamedir", "").toString();
    l4d2gamedir = settings.value("l4d2gamedir", "").toString();
    portalgamedir = settings.value("portalgamedir", "").toString();
    portal2gamedir = settings.value("portal2gamedir", "").toString();
    tf2gamedir = settings.value("tf2gamedir", "").toString();
    gmodgamedir = settings.value("gmodgamedir", "").toString();
    contentFolderToSave = settings.value("content_folder_to_save", "C:\\").toString();
    vmfDefaultPath = contentFolderToSave;
    s1GameType = settings.value("s1_game_type", "csgo").toString();

    QStringList valid_games = {"csgo", "css", "hl2", "l4d", "l4d2", "portal", "portal2", "tf2", "gmod"};
    if (!valid_games.contains(s1GameType)) {
        s1GameType = "csgo";
    }

    if (!cs2Basefolder.isEmpty() && !IsValidCs2(cs2Basefolder)) {
        cs2Basefolder = "";
    }
    if (!csgogamedir.isEmpty() && !IsValidS1(csgogamedir, "csgo")) {
        csgogamedir = "";
    }
    if (!cssgamedir.isEmpty() && !IsValidS1(cssgamedir, "css")) {
        cssgamedir = "";
    }
    if (!hl2gamedir.isEmpty() && !IsValidS1(hl2gamedir, "hl2")) {
        hl2gamedir = "";
    }
    if (!l4dgamedir.isEmpty() && !IsValidS1(l4dgamedir, "l4d")) {
        l4dgamedir = "";
    }
    if (!l4d2gamedir.isEmpty() && !IsValidS1(l4d2gamedir, "l4d2")) {
        l4d2gamedir = "";
    }
    if (!portalgamedir.isEmpty() && !IsValidS1(portalgamedir, "portal")) {
        portalgamedir = "";
    }
    if (!portal2gamedir.isEmpty() && !IsValidS1(portal2gamedir, "portal2")) {
        portal2gamedir = "";
    }
    if (!tf2gamedir.isEmpty() && !IsValidS1(tf2gamedir, "tf2")) {
        tf2gamedir = "";
    }
    if (!gmodgamedir.isEmpty() && !IsValidS1(gmodgamedir, "gmod")) {
        gmodgamedir = "";
    }

    AutoDetectPaths();

    emit cs2BasefolderChanged();
    emit s1gameBasefolderChanged();
    emit s1GameTypeChanged();
    emit vmfDefaultPathUrlChanged();
    emit usebspChanged();
    emit usebspNomergeinstancesChanged();
    emit skipdepsChanged();

    UpdateCanGo();
    GetLaunchOptions();
}

void Backend::Start()
{
    if (cs2Basefolder.isEmpty()) {
        emit alertMessage("Validation Error", "CS2 folder not selected.");
        return;
    }
    if (GetS1gameBasefolder().isEmpty()) {
        emit alertMessage("Validation Error", "CSGO/CSS folder not selected.");
        return;
    }
    if (bspFile.isEmpty() && contentFolder.isEmpty()) {
        emit alertMessage("Validation Error", "Please select a VMF or BSP file.");
        return;
    }
    if (usebsp || usebspNomergeinstances) {
        if (csgogamedir.isEmpty() || !IsValidS1(csgogamedir, "csgo")) {
            emit alertMessage("Error", "You need to install CSGO for map geo import!");
            return;
        }
    }

    try {
        if (addonName.trimmed().isEmpty()) {
            addonName = mapName;
            emit addonNameChanged();
        }

        SaveToCfg();

        QString log_dir_path = QDir(appDir).filePath("logs");
        QDir().mkpath(log_dir_path);
        QString log_filename = QString("%1_%2.log")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
            .arg(addonName);
        QString log_file_path = QDir(log_dir_path).filePath(log_filename);

        if (logStream) {
            delete logStream;
            logStream = nullptr;
        }
        if (logFile) {
            if (logFile->isOpen()) {
                logFile->close();
            }
            delete logFile;
            logFile = nullptr;
        }

        logFile = new QFile(log_file_path);
        if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            logStream = new QTextStream(logFile);
        } else {
            delete logFile;
            logFile = nullptr;
        }

        Miscellaneous::CanceLImport = 0;
        Miscellaneous::MoveVpkSignatures(cs2Basefolder, vpkSignaturesMoved);

        isGoing = true;
        UpdateCanGo();

        Miscellaneous::Log("Starting Miscellaneous thread...");

        Miscellaneous::Options opts;
        opts.cs2Basefolder = cs2Basefolder;
        opts.cs2Basefolder.replace("/", "\\");
        opts.s1gameBasefolder = GetS1gameBasefolder();
        opts.csgogamedir = csgogamedir;
        opts.s1GameType = s1GameType;
        opts.contentFolder = contentFolder;
        opts.mapName = mapName;
        opts.bspFile = bspFile;
        opts.appDir = appDir;
        opts.addonName = addonName;
        opts.usebsp = usebsp && !usebspNomergeinstances;
        opts.usebspNomergeinstances = usebsp && usebspNomergeinstances;
        opts.skipdeps = skipdeps;

        QThread* workerThread = QThread::create([this, opts]() mutable {
            bool success = true;
            try {
                if (!opts.bspFile.isEmpty()) {
                    if (!Miscellaneous::CheckJava()) {
                        throw AppException("Java is not installed. Cannot decompile BSP file.");
                    }
                    VmfBspProcess::ProcessBsp(opts);
                }

                QString target_vmf_path = QDir(opts.appDir).filePath("maps/" + opts.mapName + "/maps/" + opts.mapName + ".vmf");
                VmfBspProcess::FixSpecialTargetnames(target_vmf_path);

                MapImporter::Options mapOpts;
                QString s1Subfolder = "csgo";
                if (opts.s1GameType == "css") s1Subfolder = "cstrike";
                else if (opts.s1GameType == "hl2") s1Subfolder = "hl2";
                else if (opts.s1GameType == "l4d") s1Subfolder = "left4dead";
                else if (opts.s1GameType == "l4d2") s1Subfolder = "left4dead2";
                else if (opts.s1GameType == "portal") s1Subfolder = "portal";
                else if (opts.s1GameType == "portal2") s1Subfolder = "portal2";
                else if (opts.s1GameType == "tf2") s1Subfolder = "tf";
                else if (opts.s1GameType == "gmod") s1Subfolder = "garrysmod";

                QString s1gamedir = opts.s1gameBasefolder + "\\" + s1Subfolder;
                s1gamedir.replace('/', '\\');
                mapOpts.s1gamedir = s1gamedir;

                QString csgogamedir_path = opts.csgogamedir + "\\csgo";
                csgogamedir_path.replace('/', '\\');
                mapOpts.csgogamedir = csgogamedir_path;

                mapOpts.s1gamename = opts.s1GameType;

                QString contentdir = opts.contentFolder;
                contentdir.replace('/', '\\');
                mapOpts.s1contentdir = contentdir;

                mapOpts.s2addonname = opts.addonName;
                mapOpts.s2contentdir = opts.cs2Basefolder + "\\content\\csgo_addons\\" + opts.addonName;
                mapOpts.mapname = opts.mapName;
                mapOpts.usebsp = opts.usebsp;
                mapOpts.usebspNomergeinstances = opts.usebspNomergeinstances;
                mapOpts.skipdeps = opts.skipdeps;
                mapOpts.cs2Basefolder = opts.cs2Basefolder;

                MapImporter importer(mapOpts);
                success = importer.Run();

            } catch (const AppException& e) {
                Miscellaneous::Log(QString("Error: ") + e.message());
                success = false;
                QString errMsg = e.message();
                QMetaObject::invokeMethod(this, [this, errMsg]() {
                    emit alertMessage("Error", errMsg);
                }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this, success]() {
                if (vpkSignaturesMoved && !cs2Basefolder.isEmpty()) {
                    Miscellaneous::RestoreVpkSignatures(cs2Basefolder);
                    vpkSignaturesMoved = false;
                }

                isGoing = false;
                UpdateCanGo();

                if (success) {
                    Miscellaneous::Log("MapImporter thread finished successfully.");
                } else {
                    Miscellaneous::Log("MapImporter thread finished with errors.");
                }

                if (logStream) {
                    delete logStream;
                    logStream = nullptr;
                }
                if (logFile) {
                    if (logFile->isOpen()) {
                        logFile->close();
                    }
                    delete logFile;
                    logFile = nullptr;
                }
            }, Qt::QueuedConnection);
        });

        connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
        workerThread->start();

    } catch (const AppException& e) {
        Miscellaneous::Log(QString("Error: %1").arg(e.message()));
        emit alertMessage("Error", e.message());
    }
}

void Backend::Stop()
{
    if (isGoing) {
        Miscellaneous::Log("Cancelling import...");
        Miscellaneous::CancelAll();
    }
}

void Backend::AppAboutToQuit()
{
    Miscellaneous::CancelAll();
    if (vpkSignaturesMoved && !cs2Basefolder.isEmpty()) {
        Miscellaneous::RestoreVpkSignatures(cs2Basefolder);
        vpkSignaturesMoved = false;
    }
}
