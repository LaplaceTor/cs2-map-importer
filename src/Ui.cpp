#include "VmfBspProcess.h"
#include "Ui.h"
#include "MapImporter.h"
#include "ModelImporter.h"
#include "ParticleImporter.h"
#include "Miscellaneous.h"

#include <QDir>
#include <QGuiApplication>
#include <QStyleHints>
#include <QDebug>
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
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

Backend* Backend::s_instance = nullptr;

Backend* Backend::instance()
{
    return s_instance;
}

Backend::Backend(QObject *parent) :
    QObject(parent),
    vmfDefaultPath("C:\\"),
    s1GameType("csgo"),
    contentFolderToSave("C:\\"),
    vpkSignaturesMoved(false),
    theme(""),
    networkManager(new QNetworkAccessManager(this)),
    logFile(nullptr),
    logStream(nullptr)

{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        if (theme == "system") {
            ApplyTheme("system");
        }
    });
#endif

    connect(networkManager, &QNetworkAccessManager::sslErrors, this, [](QNetworkReply* reply, const QList<QSslError>& errors) {
        Q_UNUSED(errors);
        reply->ignoreSslErrors();
    });

    appDir = QCoreApplication::applicationDirPath();

    Miscellaneous::GlobaLLogger = [this](const QString& msg) {
        emit logMessage(msg);
        QMutexLocker locker(&logMutex);
        if (logStream) {
            *logStream << msg << "\n";
            logStream->flush();
        }
    };

    s_instance = this;

    Miscellaneous::Log("Initializing CS2 Importer...");

    GetLaunchOptions();

    LoadFromCfg();

    Miscellaneous::Log("Initializing CS2 Importer... Finished");
}

bool Backend::requestConfirmation(const QString& title, const QString& msg)
{
    QMutexLocker locker(&confirmMutex);
    confirmResult = false;
    confirmInProgress = true;

    emit askQmlConfirmation(title, msg);

    while (confirmInProgress) {
        confirmCond.wait(&confirmMutex);
    }

    return confirmResult;
}

void Backend::setConfirmationResult(bool result)
{
    QMutexLocker locker(&confirmMutex);
    confirmResult = result;
    confirmInProgress = false;
    confirmCond.wakeAll();
}

bool Backend::ShowMessageBox(const QString& title, const QString& text, int iconType, bool showYesNo)
{
    Q_UNUSED(iconType);
    if (showYesNo) {
        if (s_instance) {
            return s_instance->requestConfirmation(title, text);
        }
        return false;
    } else {
        if (s_instance) {
            QMetaObject::invokeMethod(s_instance, [=]() {
                emit s_instance->alertMessage(title, text);
            });
            return true;
        }
        return false;
    }
}

void Backend::ValidateCs2()
{
    if (IsGoingWarn()) {
        return;
    }
    QDesktopServices::openUrl(QUrl("steam://validate/730"));
}

void Backend::ValidateS1()
{
    if (IsGoingWarn()) {
        return;
    }
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
    } else if (s1GameType == "blackmesa") {
        QDesktopServices::openUrl(QUrl("steam://validate/362890"));
    }
}

void Backend::SetS1GameType(const QString& type)
{
    if (IsGoingWarn()) {
        emit s1GameTypeChanged();
        return;
    }
    if (s1GameType != type) {
        s1GameType = type;
        emit s1gameBasefolderChanged();
        emit s1GameTypeChanged();
        UpdateCanGo();
        SaveToCfg();
    }
}

void Backend::SetTheme(const QString& val)
{
    if (theme != val) {
        theme = val;
        emit themeChanged();
        ApplyTheme(theme);
        SaveToCfg();
    }
}

void Backend::ApplyTheme(const QString& val)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (val == "light") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    } else if (val == "dark") {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    } else {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    }
#else
    Q_UNUSED(val);
#endif
}

bool Backend::IsGoingWarn()
{
    if (isGoing) {
        emit alertMessage("Process Running", "An import process is currently running. You cannot change any settings or options until you stop the process or it completes.");
        return true;
    }
    return false;
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
    if (IsGoingWarn()) {
        return;
    }
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
        RefreshCs2AddonsList();
        UpdateCanGo();
        SaveToCfg();
    }
}

bool Backend::IsValidS1(const QString& path, const QString& type)
{
    if (path.isEmpty()) return false;

    if (type == "other") {
        return path.endsWith("gameinfo.txt", Qt::CaseInsensitive) && QFileInfo(path).exists();
    }

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
    } else if (type == "blackmesa") {
        checkGameinfo("bms", "Black Mesa");
    }

    return valid;
}

void Backend::SelectS1FolderDialog(const QUrl& url)
{
    if (IsGoingWarn()) {
        return;
    }
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    if (!IsValidS1(path, s1GameType)) {
        if (s1GameType == "other") {
            emit alertMessage("Invalid gameinfo.txt", "The selected file is not a valid gameinfo.txt file.");
        } else {
            emit alertMessage("Invalid Source 1 Folder", "The selected folder is not a valid installation for the selected game.\nPlease make sure it is the correct directory containing the gameinfo.txt.");
        }
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
        QStringList apps;
    };
    QList<LibraryData> libraries;

    QString currentPath;
    QStringList currentApps;
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
    QString found_blackmesa_dir;

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

        if (lib.apps.contains("362890")) {
            QString blackmesa_candidate = QDir(common_dir).filePath("Black Mesa");
            if (IsValidS1(blackmesa_candidate, "blackmesa")) {
                found_blackmesa_dir = blackmesa_candidate;
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

    if (blackmesagamedir.isEmpty() && !found_blackmesa_dir.isEmpty()) {
        blackmesagamedir = found_blackmesa_dir;
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
        } else if (s1GameType == "blackmesa") {
            blackmesagamedir = path;
        } else if (s1GameType == "other") {
            othergameinfo = path;
            othergamedir = QFileInfo(path).absolutePath();
            otherbasefolder = Miscellaneous::GetBaseFolderFromGameInfo(path);
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
    if (IsGoingWarn()) {
        return;
    }
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

void Backend::SelectPcfDialog(const QUrl& url)
{
    if (IsGoingWarn()) {
        return;
    }
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    pcfFile = path;
    emit pcfFileChanged();

    UpdateCanGo();
}

void Backend::SelectMdlDialog(const QUrl& url)
{
    if (IsGoingWarn()) {
        return;
    }
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

    mdlFile = path;
    emit mdlFileChanged();

    UpdateCanGo();
}

void Backend::RefreshCs2AddonsList()
{
    if (isGoing) return;
    QStringList list;
    if (!cs2Basefolder.isEmpty()) {
        QDir addonsDir(QDir(cs2Basefolder).filePath("content/csgo_addons"));
        if (addonsDir.exists()) {
            list = addonsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        }
    }
    if (list.isEmpty()) {
        list.append("others");
    }

    if (cs2AddonsList != list) {
        cs2AddonsList = list;
        emit cs2AddonsListChanged();
    }

    // Default to the first addon or "others"
    if (!cs2AddonsList.isEmpty()) {
        if (selectedMdlAddon.isEmpty() || !cs2AddonsList.contains(selectedMdlAddon)) {
            SetSelectedMdlAddon(cs2AddonsList.first());
        }
    } else {
        SetSelectedMdlAddon("");
    }
}

void Backend::SelectBspDialog(const QUrl& url)
{
    if (IsGoingWarn()) {
        return;
    }
    QString path = url.toLocalFile();
    if (path.isEmpty()) return;

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
    settings.setValue("theme", theme);
    settings.setValue("cmdLogOut", cmdLogOut);
    settings.setValue("keepFuncDetailAsBrush", keepFuncDetailAsBrush);
    settings.setValue("usebsp", usebsp);
    settings.setValue("usebsp_nomergeinstances", usebspNomergeinstances);
    settings.setValue("skipdeps", skipdeps);
    settings.setValue("cs2_basefolder", cs2Basefolder);

    settings.setValue("modelSkipAnimation", modelSkipAnimation);
    settings.setValue("modelChangeBindpose", modelChangeBindpose);
    settings.setValue("modelOverrideLean", modelOverrideLean);
    settings.setValue("modelHeaderHullBounds", modelHeaderHullBounds);
    settings.setValue("modelImportLods", modelImportLods);
    settings.setValue("modelWriteWeaponPrefab", modelWriteWeaponPrefab);

    settings.setValue("particleAllowDepthBlend", particleAllowDepthBlend);
    settings.setValue("particleDisableDiffuse", particleDisableDiffuse);

    settings.setValue("csgogamedir", csgogamedir);
    settings.setValue("cssgamedir", cssgamedir);
    settings.setValue("hl2gamedir", hl2gamedir);
    settings.setValue("l4dgamedir", l4dgamedir);
    settings.setValue("l4d2gamedir", l4d2gamedir);
    settings.setValue("portalgamedir", portalgamedir);
    settings.setValue("portal2gamedir", portal2gamedir);
    settings.setValue("tf2gamedir", tf2gamedir);
    settings.setValue("gmodgamedir", gmodgamedir);
    settings.setValue("blackmesagamedir", blackmesagamedir);
    settings.setValue("othergameinfo", othergameinfo);
    settings.setValue("othergamedir", othergamedir);
    settings.setValue("otherbasefolder", otherbasefolder);
    settings.setValue("content_folder_to_save", contentFolderToSave);
    settings.setValue("s1_game_type", s1GameType);
}

void Backend::LoadFromCfg()
{
    QSettings settings("cs2importer.cfg", QSettings::IniFormat);

    theme = settings.value("theme", "system").toString();
    ApplyTheme(theme);

    cmdLogOut = settings.value("cmdLogOut", false).toBool();
    keepFuncDetailAsBrush = settings.value("keepFuncDetailAsBrush", false).toBool();
    usebsp = settings.value("usebsp", true).toBool();
    usebspNomergeinstances = settings.value("usebsp_nomergeinstances", false).toBool();
    skipdeps = settings.value("skipdeps", false).toBool();
    cs2Basefolder = settings.value("cs2_basefolder", "").toString();

    modelSkipAnimation = settings.value("modelSkipAnimation", false).toBool();
    modelChangeBindpose = settings.value("modelChangeBindpose", false).toBool();
    modelOverrideLean = settings.value("modelOverrideLean", false).toBool();
    modelHeaderHullBounds = settings.value("modelHeaderHullBounds", false).toBool();
    modelImportLods = settings.value("modelImportLods", false).toBool();
    modelWriteWeaponPrefab = settings.value("modelWriteWeaponPrefab", false).toBool();

    particleAllowDepthBlend = settings.value("particleAllowDepthBlend", false).toBool();
    particleDisableDiffuse = settings.value("particleDisableDiffuse", false).toBool();

    csgogamedir = settings.value("csgogamedir", "").toString();
    cssgamedir = settings.value("cssgamedir", "").toString();
    hl2gamedir = settings.value("hl2gamedir", "").toString();
    l4dgamedir = settings.value("l4dgamedir", "").toString();
    l4d2gamedir = settings.value("l4d2gamedir", "").toString();
    portalgamedir = settings.value("portalgamedir", "").toString();
    portal2gamedir = settings.value("portal2gamedir", "").toString();
    tf2gamedir = settings.value("tf2gamedir", "").toString();
    gmodgamedir = settings.value("gmodgamedir", "").toString();
    blackmesagamedir = settings.value("blackmesagamedir", "").toString();
    othergameinfo = settings.value("othergameinfo", "").toString();
    othergamedir = settings.value("othergamedir", "").toString();
    otherbasefolder = settings.value("otherbasefolder", "").toString();
    contentFolderToSave = settings.value("content_folder_to_save", "C:\\").toString();
    vmfDefaultPath = contentFolderToSave;
    s1GameType = settings.value("s1_game_type", "csgo").toString();

    QStringList valid_games = {"csgo", "css", "hl2", "l4d", "l4d2", "portal", "portal2", "tf2", "gmod", "blackmesa", "other"};
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
    if (!blackmesagamedir.isEmpty() && !IsValidS1(blackmesagamedir, "blackmesa")) {
        blackmesagamedir = "";
    }
    if (!othergameinfo.isEmpty() && !IsValidS1(othergameinfo, "other")) {
        othergameinfo = "";
    }
    if (!othergameinfo.isEmpty()) {
        othergamedir = QFileInfo(othergameinfo).absolutePath();
        otherbasefolder = Miscellaneous::GetBaseFolderFromGameInfo(othergameinfo);
    } else {
        othergamedir = "";
        otherbasefolder = "";
    }

    AutoDetectPaths();

    emit cs2BasefolderChanged();
    emit s1gameBasefolderChanged();
    emit s1GameTypeChanged();
    emit vmfDefaultPathUrlChanged();
    emit usebspChanged();
    emit cmdLogOutChanged();
    emit usebspNomergeinstancesChanged();
    emit skipdepsChanged();

    emit modelSkipAnimationChanged();
    emit modelChangeBindposeChanged();
    emit modelOverrideLeanChanged();
    emit modelHeaderHullBoundsChanged();
    emit modelImportLodsChanged();
    emit modelWriteWeaponPrefabChanged();

    emit particleAllowDepthBlendChanged();
    emit particleDisableDiffuseChanged();

    RefreshCs2AddonsList();

    UpdateCanGo();
    GetLaunchOptions();
}

void Backend::Start()
{
    if (IsGoingWarn()) {
        return;
    }
    if (cs2Basefolder.isEmpty()) {
        emit alertMessage("Validation Error", "CS2 folder not selected.");
        return;
    }
    if (GetS1gameBasefolder().isEmpty()) {
        emit alertMessage("Validation Error", "CSGO/CSS folder not selected.");
        return;
    }
    if (activeTab == TAB_MODEL) {
        if (mdlFile.isEmpty()) {
            emit alertMessage("Validation Error", "Please select an MDL file.");
            return;
        }
        if (selectedMdlAddon.isEmpty()) {
            emit alertMessage("Validation Error", "Please select a CS2 addon.");
            return;
        }
    } else if (activeTab == TAB_PARTICLE) {
        if (pcfFile.isEmpty()) {
            emit alertMessage("Validation Error", "Please select a PCF file.");
            return;
        }
        if (selectedMdlAddon.isEmpty()) {
            emit alertMessage("Validation Error", "Please select a CS2 addon.");
            return;
        }
    } else {
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
    }

    if (activeTab == TAB_MAP) {
        if (appDir.contains(' ')) {
            emit alertMessage("Error", "The path to this application contains spaces:\n" + appDir + "\n\nValve binaries do not support importing when the application path contains spaces. Please move this application to a path without spaces (e.g., 'C:\\cs2importer').");
            return;
        }

        bool filenameHasSpace = false;
        QString problemFilename;
        if (!bspFile.isEmpty()) {
            QString bspFilename = QFileInfo(bspFile).fileName();
            if (bspFilename.contains(' ')) {
                filenameHasSpace = true;
                problemFilename = bspFilename;
            }
        } else {
            if (mapName.contains(' ')) {
                filenameHasSpace = true;
                problemFilename = mapName + ".vmf";
            }
        }

        if (filenameHasSpace) {
            emit alertMessage("Error", "The selected map filename contains spaces:\n\"" + problemFilename + "\"\n\nValve binaries do not support importing map files with spaces in their names. Please rename the map file to remove any spaces (e.g., use underscores: 'custom_map.vmf').");
            return;
        }
    }

    try {
        QString currentAddonName = (activeTab == TAB_MODEL || activeTab == TAB_PARTICLE) ? selectedMdlAddon : addonName;
        if (activeTab == TAB_MAP && currentAddonName.trimmed().isEmpty()) {
            currentAddonName = mapName;
            addonName = mapName;
            emit addonNameChanged();
        }

        SaveToCfg();

        QString log_dir_path = QDir(appDir).filePath("logs");
        QDir().mkpath(log_dir_path);
        QString log_filename;
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        if (activeTab == TAB_MODEL) {
            QString base = QFileInfo(mdlFile).baseName();
            if (base.isEmpty()) base = currentAddonName;
            log_filename = QString("%1_mdl_%2.log").arg(timestamp).arg(base);
        } else if (activeTab == TAB_PARTICLE) {
            QString base = QFileInfo(pcfFile).baseName();
            if (base.isEmpty()) base = currentAddonName;
            log_filename = QString("%1_pcf_%2.log").arg(timestamp).arg(base);
        } else {
            log_filename = QString("%1_%2.log").arg(timestamp).arg(currentAddonName);
        }
        QString log_file_path = QDir(log_dir_path).filePath(log_filename);

        {
            QMutexLocker locker(&logMutex);
            logStream.reset();
            logFile.reset();

            auto tempLogFile = std::make_unique<QFile>(log_file_path);
            if (tempLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                logFile = std::move(tempLogFile);
                logStream = std::make_unique<QTextStream>(logFile.get());
            }
        }

        Miscellaneous::CanceLImport = 0;
        {
            QMutexLocker locker(&vpkMutex);
            Miscellaneous::MoveVpkSignatures(cs2Basefolder, vpkSignaturesMoved);
        }

        isGoing = true;
        UpdateCanGo();

        Miscellaneous::Log("Starting Miscellaneous thread...");

        QList<Miscellaneous::SearchTarget> searchTargets;
        if (!skipdeps) {
            QString gameinfoFile;
            if (s1GameType == "other") {
                gameinfoFile = othergameinfo;
            } else {
                QString s1Subfolder = "csgo";
                if (s1GameType == "css") s1Subfolder = "cstrike";
                else if (s1GameType == "hl2") s1Subfolder = "hl2";
                else if (s1GameType == "l4d") s1Subfolder = "left4dead";
                else if (s1GameType == "l4d2") s1Subfolder = "left4dead2";
                else if (s1GameType == "portal") s1Subfolder = "portal";
                else if (s1GameType == "portal2") s1Subfolder = "portal2";
                else if (s1GameType == "tf2") s1Subfolder = "tf";
                else if (s1GameType == "gmod") s1Subfolder = "garrysmod";
                else if (s1GameType == "blackmesa") s1Subfolder = "bms";

                gameinfoFile = QDir(GetS1gameBasefolder()).filePath(s1Subfolder + "/gameinfo.txt");
            }

            if (!gameinfoFile.isEmpty() && QFile::exists(gameinfoFile)) {
                Miscellaneous::Log("Parsing gameinfo from: " + gameinfoFile);
                Miscellaneous::ParseGameInfo(gameinfoFile, searchTargets);
                Miscellaneous::Log(QString("Found %1 search targets from gameinfo.").arg(searchTargets.size()));
            } else {
                Miscellaneous::Log("Warning: gameinfo.txt not found at: " + gameinfoFile);
            }
        }

        Miscellaneous::Options opts;
        opts.cs2Basefolder = QDir::toNativeSeparators(cs2Basefolder);
        opts.s1gameBasefolder = GetS1gameBasefolder();
        opts.csgogamedir = csgogamedir;
        opts.s1GameType = s1GameType;
        opts.othergamedir = othergamedir;
        opts.appDir = appDir;
        opts.searchTargets = searchTargets;

        if (activeTab == TAB_PARTICLE) {
            opts.addonName = selectedMdlAddon;
            opts.cmdLogOut = true; // Always true for non-map imports to preserve default logging
            opts.particleAllowDepthBlend = particleAllowDepthBlend;
            opts.particleDisableDiffuse = particleDisableDiffuse;
            opts.keepFuncDetailAsBrush = false;
            opts.usebsp = false;
            opts.usebspNomergeinstances = false;
            opts.skipdeps = false;
            opts.modelSkipAnimation = false;
            opts.modelChangeBindpose = false;
            opts.modelOverrideLean = false;
            opts.modelHeaderHullBounds = false;
            opts.modelImportLods = false;
            opts.modelWriteWeaponPrefab = false;
        } else if (activeTab == TAB_MODEL) {
            opts.addonName = selectedMdlAddon;
            opts.cmdLogOut = true; // Always true for non-map imports to preserve default logging
            opts.modelSkipAnimation = modelSkipAnimation;
            opts.modelChangeBindpose = modelChangeBindpose;
            opts.modelOverrideLean = modelOverrideLean;
            opts.modelHeaderHullBounds = modelHeaderHullBounds;
            opts.modelImportLods = modelImportLods;
            opts.modelWriteWeaponPrefab = modelWriteWeaponPrefab;
            opts.particleAllowDepthBlend = false;
            opts.particleDisableDiffuse = false;
            opts.keepFuncDetailAsBrush = false;
            opts.usebsp = false;
            opts.usebspNomergeinstances = false;
            opts.skipdeps = false;
        } else {
            opts.contentFolder = contentFolder;
            opts.cmdLogOut = cmdLogOut;
            opts.particleAllowDepthBlend = false;
            opts.particleDisableDiffuse = false;
            opts.mapName = mapName;
            opts.bspFile = bspFile;
            opts.addonName = addonName;
            opts.keepFuncDetailAsBrush = keepFuncDetailAsBrush;
            opts.usebsp = usebsp && !usebspNomergeinstances;
            opts.usebspNomergeinstances = usebsp && usebspNomergeinstances;
            opts.skipdeps = skipdeps;
            opts.modelSkipAnimation = false;
            opts.modelChangeBindpose = false;
            opts.modelOverrideLean = false;
            opts.modelHeaderHullBounds = false;
            opts.modelImportLods = false;
            opts.modelWriteWeaponPrefab = false;
        }

        QThread* workerThread = QThread::create([this, opts, activeTabCopy = activeTab, mdlFileCopy = mdlFile, pcfFileCopy = pcfFile]() mutable {
            bool success = true;
            try {
                if (activeTabCopy == TAB_MODEL) {
                    success = RunModelImportWorkflow(opts, mdlFileCopy);
                } else if (activeTabCopy == TAB_PARTICLE) {
                    success = RunParticleImportWorkflow(opts, pcfFileCopy);
                } else if (activeTabCopy == TAB_MAP) {
                    success = RunMapImportWorkflow(opts);
                }
            } catch (const AppException& e) {
                Miscellaneous::Log(QString("Error: ") + e.message());
                success = false;
                QString errMsg = e.message();
                QMetaObject::invokeMethod(this, [this, errMsg]() {
                    emit alertMessage("Error", errMsg);
                }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this, success, activeTabCopy]() {
                {
                    QMutexLocker locker(&vpkMutex);
                    if (vpkSignaturesMoved && !cs2Basefolder.isEmpty()) {
                        Miscellaneous::RestoreVpkSignatures(cs2Basefolder);
                        vpkSignaturesMoved = false;
                    }
                }

                isGoing = false;
                UpdateCanGo();

                if (success) {
                    if (activeTabCopy == TAB_MODEL) {
                        Miscellaneous::Log("ModelImporter thread finished successfully.");
                    } else if (activeTabCopy == TAB_PARTICLE) {
                        Miscellaneous::Log("ParticleImporter thread finished successfully.");
                    } else {
                        Miscellaneous::Log("MapImporter thread finished successfully.");
                    }
                } else {
                    if (activeTabCopy == TAB_MODEL) {
                        Miscellaneous::Log("ModelImporter thread finished with errors.");
                    } else if (activeTabCopy == TAB_PARTICLE) {
                        Miscellaneous::Log("ParticleImporter thread finished with errors.");
                    } else {
                        Miscellaneous::Log("MapImporter thread finished with errors.");
                    }
                }

                {
                    QMutexLocker locker(&logMutex);
                    logStream.reset();
                    logFile.reset();
                }
            }, Qt::QueuedConnection);
        });

        connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
        workerThread->start();

    } catch (const AppException& e) {
        Miscellaneous::Log(QString("Error: %1").arg(e.message()));
        emit alertMessage("Error", e.message());
        isGoing = false;
        UpdateCanGo();
        {
            QMutexLocker locker(&logMutex);
            logStream.reset();
            logFile.reset();
        }
    } catch (...) {
        emit alertMessage("Error", "An unexpected error occurred.");
        isGoing = false;
        UpdateCanGo();
        {
            QMutexLocker locker(&logMutex);
            logStream.reset();
            logFile.reset();
        }
    }
}

bool Backend::RunMapImportWorkflow(Miscellaneous::Options opts)
{
    if (Miscellaneous::CanceLImport) return false;

    Miscellaneous::SetOptions(opts);

    if (!opts.bspFile.isEmpty()) {
        VmfBspProcess::ProcessBsp();
    }

    if (Miscellaneous::CanceLImport) return false;

    // Get updated opts (like updated contentFolder after ProcessBsp)
    Miscellaneous::Options currentOpts = Miscellaneous::GetOptions();

    QString target_vmf_path = QDir(currentOpts.appDir).filePath("maps/" + currentOpts.mapName + "/maps/" + currentOpts.mapName + ".vmf");
    VmfBspProcess::FixEntities(target_vmf_path);

    if (Miscellaneous::CanceLImport) return false;

    QString s1gamedir;
    if (currentOpts.s1GameType == "other") {
        s1gamedir = QDir::toNativeSeparators(currentOpts.othergamedir);
    } else {
        QString s1Subfolder = "csgo";
        if (currentOpts.s1GameType == "css") s1Subfolder = "cstrike";
        else if (currentOpts.s1GameType == "hl2") s1Subfolder = "hl2";
        else if (currentOpts.s1GameType == "l4d") s1Subfolder = "left4dead";
        else if (currentOpts.s1GameType == "l4d2") s1Subfolder = "left4dead2";
        else if (currentOpts.s1GameType == "portal") s1Subfolder = "portal";
        else if (currentOpts.s1GameType == "portal2") s1Subfolder = "portal2";
        else if (currentOpts.s1GameType == "tf2") s1Subfolder = "tf";
        else if (currentOpts.s1GameType == "gmod") s1Subfolder = "garrysmod";
        else if (currentOpts.s1GameType == "blackmesa") s1Subfolder = "bms";
        s1gamedir = QDir::toNativeSeparators(currentOpts.s1gameBasefolder + "/" + s1Subfolder);
    }
    currentOpts.s1gamedir = s1gamedir;

    QString csgogamedir_path = QDir::toNativeSeparators(currentOpts.csgogamedir + "/csgo");
    currentOpts.csgogamedir = csgogamedir_path;

    QString contentdir = QDir::toNativeSeparators(currentOpts.contentFolder);
    currentOpts.s1contentdir = contentdir;

    currentOpts.s2contentdir = QDir::toNativeSeparators(currentOpts.cs2Basefolder + "/content/csgo_addons/" + currentOpts.addonName);

    Miscellaneous::SetOptions(currentOpts);

    MapImporter importer;
    return importer.Run();
}

bool Backend::RunModelImportWorkflow(Miscellaneous::Options opts, const QString& mdlPath)
{
    QString s1gamedir;
    if (opts.s1GameType == "other") {
        s1gamedir = QDir::toNativeSeparators(opts.othergamedir);
    } else {
        QString s1Subfolder = "csgo";
        if (opts.s1GameType == "css") s1Subfolder = "cstrike";
        else if (opts.s1GameType == "hl2") s1Subfolder = "hl2";
        else if (opts.s1GameType == "l4d") s1Subfolder = "left4dead";
        else if (opts.s1GameType == "l4d2") s1Subfolder = "left4dead2";
        else if (opts.s1GameType == "portal") s1Subfolder = "portal";
        else if (opts.s1GameType == "portal2") s1Subfolder = "portal2";
        else if (opts.s1GameType == "tf2") s1Subfolder = "tf";
        else if (opts.s1GameType == "gmod") s1Subfolder = "garrysmod";
        else if (opts.s1GameType == "blackmesa") s1Subfolder = "bms";
        s1gamedir = QDir::toNativeSeparators(opts.s1gameBasefolder + "/" + s1Subfolder);
    }
    opts.s1gamedir = s1gamedir;

    QString csgogamedir_path = QDir::toNativeSeparators(opts.csgogamedir + "/csgo");
    opts.csgogamedir = csgogamedir_path;

    QString modelsTempDir = QDir::toNativeSeparators(opts.appDir + "/models_temp");
    QDir().mkpath(modelsTempDir);
    opts.s1contentdir = modelsTempDir;

    opts.s2contentdir = QDir::toNativeSeparators(opts.cs2Basefolder + "/content/csgo_addons/" + opts.addonName);

    Miscellaneous::SetOptions(opts);

    ModelImporter importer;
    return importer.Run(mdlPath);
}

bool Backend::RunParticleImportWorkflow(Miscellaneous::Options opts, const QString& pcfPath)
{
    QString s1gamedir;
    if (opts.s1GameType == "other") {
        s1gamedir = QDir::toNativeSeparators(opts.othergamedir);
    } else {
        QString s1Subfolder = "csgo";
        if (opts.s1GameType == "css") s1Subfolder = "cstrike";
        else if (opts.s1GameType == "hl2") s1Subfolder = "hl2";
        else if (opts.s1GameType == "l4d") s1Subfolder = "left4dead";
        else if (opts.s1GameType == "l4d2") s1Subfolder = "left4dead2";
        else if (opts.s1GameType == "portal") s1Subfolder = "portal";
        else if (opts.s1GameType == "portal2") s1Subfolder = "portal2";
        else if (opts.s1GameType == "tf2") s1Subfolder = "tf";
        else if (opts.s1GameType == "gmod") s1Subfolder = "garrysmod";
        else if (opts.s1GameType == "blackmesa") s1Subfolder = "bms";
        s1gamedir = QDir::toNativeSeparators(opts.s1gameBasefolder + "/" + s1Subfolder);
    }
    opts.s1gamedir = s1gamedir;

    QString csgogamedir_path = QDir::toNativeSeparators(opts.csgogamedir + "/csgo");
    opts.csgogamedir = csgogamedir_path;

    opts.s2contentdir = QDir::toNativeSeparators(opts.cs2Basefolder + "/content/csgo_addons/" + opts.addonName);

    Miscellaneous::SetOptions(opts);

    ParticleImporter importer;
    return importer.Run(pcfPath);
}

void Backend::Stop()
{
    if (isGoing) {
        Miscellaneous::Log("Cancelling import...");
        Miscellaneous::CancelAll();
        {
            QMutexLocker locker(&vpkMutex);
            if (vpkSignaturesMoved && !cs2Basefolder.isEmpty()) {
                Miscellaneous::RestoreVpkSignatures(cs2Basefolder);
                vpkSignaturesMoved = false;
            }
        }
    }
}

QString Backend::GetCurrentVersion() const
{
    return QString(APP_VERSION);
}

void Backend::CheckForUpdate()
{
    if (IsGoingWarn()) {
        return;
    }
    CheckForUpdateInternal(true);
}

void Backend::AutoCheckForUpdate()
{
    CheckForUpdateInternal(false);
}

void Backend::CheckForUpdateInternal(bool isManual)
{
    qDebug() << "[UpdateCheck] Checking for updates... Manual:" << isManual;
    Miscellaneous::Log("Checking for updates...");

    auto performFallback = [this, isManual]() {
        qDebug() << "[UpdateCheck] Performing fallback update check using github.com...";
        Miscellaneous::Log("Retrying update check using fallback...");

        QUrl fallbackUrl("https://github.com/LaplaceTor/cs2-map-importer/releases/latest");
        QNetworkRequest request(fallbackUrl);
        request.setTransferTimeout(10000);
        request.setRawHeader("User-Agent", "CS2-Map-Importer");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply* fallbackReply = networkManager->get(request);
        fallbackReply->setProperty("finalUrl", fallbackUrl);

        connect(fallbackReply, &QNetworkReply::redirected, this, [fallbackReply](const QUrl &redirectUrl) {
            fallbackReply->setProperty("finalUrl", redirectUrl);
        });

        connect(fallbackReply, &QNetworkReply::finished, this, [this, fallbackReply, isManual]() {
            fallbackReply->deleteLater();

            if (fallbackReply->error() != QNetworkReply::NoError) {
                qDebug() << "[UpdateCheck] Fallback failed:" << fallbackReply->errorString();
                Miscellaneous::Log("Fallback failed to check for updates: " + fallbackReply->errorString());
                if (isManual) {
                    emit noUpdateAvailable();
                }
                return;
            }

            QUrl finalUrl = fallbackReply->property("finalUrl").toUrl();
            QString path = finalUrl.path();
            int index = path.indexOf("/releases/tag/");
            QString tagName;
            if (index != -1) {
                tagName = path.mid(index + QString("/releases/tag/").size());
            }

            if (tagName.startsWith("v")) {
                tagName = tagName.mid(1);
            }

            qDebug() << "[UpdateCheck] Fallback final URL:" << finalUrl.toString() << "Latest version:" << tagName << "Current version:" << APP_VERSION;

            if (!tagName.isEmpty() && tagName != QString(APP_VERSION)) {
                qDebug() << "[UpdateCheck] Update available (via fallback)!";
                Miscellaneous::Log("Update available: " + tagName);
                emit updateAvailable(tagName, "", finalUrl.toString());
            } else {
                qDebug() << "[UpdateCheck] No updates available (via fallback).";
                Miscellaneous::Log("No updates available.");
                if (isManual) {
                    emit noUpdateAvailable();
                }
            }
        });
    };

    QUrl url("https://api.github.com/repos/LaplaceTor/cs2-map-importer/releases/latest");
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    request.setRawHeader("User-Agent", "CS2-Map-Importer");

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, isManual, performFallback]() {
        reply->deleteLater();

        bool success = false;
        QString tagName;
        QString body;
        QString htmlUrl;

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            if (jsonDoc.isObject()) {
                QJsonObject jsonObj = jsonDoc.object();
                tagName = jsonObj["tag_name"].toString();
                body = jsonObj["body"].toString();
                htmlUrl = jsonObj["html_url"].toString();
                success = true;
            } else {
                qDebug() << "[UpdateCheck] Failed to parse update response.";
                Miscellaneous::Log("Failed to parse update response.");
            }
        } else {
            qDebug() << "[UpdateCheck] Failed to check for updates:" << reply->errorString();
            Miscellaneous::Log("Failed to check for updates: " + reply->errorString());
        }

        if (!success) {
            performFallback();
            return;
        }

        if (tagName.startsWith("v")) {
            tagName = tagName.mid(1);
        }

        qDebug() << "[UpdateCheck] Latest version:" << tagName << "Current version:" << APP_VERSION;

        // Very simple version comparison: if strings differ, assume update if tagName not empty
        if (!tagName.isEmpty() && tagName != QString(APP_VERSION)) {
            qDebug() << "[UpdateCheck] Update available!";
            Miscellaneous::Log("Update available: " + tagName);
            emit updateAvailable(tagName, body, htmlUrl);
        } else {
            qDebug() << "[UpdateCheck] No updates available.";
            Miscellaneous::Log("No updates available.");
            if (isManual) {
                emit noUpdateAvailable();
            }
        }
    });
}

void Backend::AppAboutToQuit()
{
    Miscellaneous::CancelAll();
    {
        QMutexLocker locker(&vpkMutex);
        if (vpkSignaturesMoved && !cs2Basefolder.isEmpty()) {
            Miscellaneous::RestoreVpkSignatures(cs2Basefolder);
            vpkSignaturesMoved = false;
        }
    }
}
