#ifndef UI_H
#define UI_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <memory>
#include "Miscellaneous.h"

class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString cs2Basefolder READ GetCs2Basefolder NOTIFY cs2BasefolderChanged)
    Q_PROPERTY(QString s1gameBasefolder READ GetS1gameBasefolder NOTIFY s1gameBasefolderChanged)
    Q_PROPERTY(QString s1GameType READ GetS1GameType NOTIFY s1GameTypeChanged)
    Q_PROPERTY(QString vmfDefaultPathUrl READ GetVmfDefaultPathUrl NOTIFY vmfDefaultPathUrlChanged)
    Q_PROPERTY(QString bspFile READ GetBspFile NOTIFY bspFileChanged)
    Q_PROPERTY(QString contentFolder READ GetContentFolder NOTIFY contentFolderChanged)
    Q_PROPERTY(QString addonName READ GetAddonName WRITE SetAddonName NOTIFY addonNameChanged)
    Q_PROPERTY(bool keepFuncDetailAsBrush READ GetKeepFuncDetailAsBrush WRITE SetKeepFuncDetailAsBrush NOTIFY keepFuncDetailAsBrushChanged)
    Q_PROPERTY(bool usebsp READ GetUsebsp WRITE SetUsebsp NOTIFY usebspChanged)
    Q_PROPERTY(bool usebspNomergeinstances READ GetUsebspNomergeinstances WRITE SetUsebspNomergeinstances NOTIFY usebspNomergeinstancesChanged)
    Q_PROPERTY(bool skipdeps READ GetSkipdeps WRITE SetSkipdeps NOTIFY skipdepsChanged)
    Q_PROPERTY(bool canGo READ GetCanGo NOTIFY canGoChanged)
    Q_PROPERTY(bool isGoing READ GetIsGoing NOTIFY isGoingChanged)
    Q_PROPERTY(QString currentVersion READ GetCurrentVersion CONSTANT)

    Q_PROPERTY(int activeTab READ GetActiveTab WRITE SetActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(QString mdlFile READ GetMdlFile NOTIFY mdlFileChanged)
    Q_PROPERTY(QStringList cs2AddonsList READ GetCs2AddonsList NOTIFY cs2AddonsListChanged)
    Q_PROPERTY(QString selectedMdlAddon READ GetSelectedMdlAddon WRITE SetSelectedMdlAddon NOTIFY selectedMdlAddonChanged)
    Q_PROPERTY(bool modelSkipAnimation READ GetModelSkipAnimation WRITE SetModelSkipAnimation NOTIFY modelSkipAnimationChanged)
    Q_PROPERTY(bool modelChangeBindpose READ GetModelChangeBindpose WRITE SetModelChangeBindpose NOTIFY modelChangeBindposeChanged)
    Q_PROPERTY(bool modelOverrideLean READ GetModelOverrideLean WRITE SetModelOverrideLean NOTIFY modelOverrideLeanChanged)
    Q_PROPERTY(bool modelHeaderHullBounds READ GetModelHeaderHullBounds WRITE SetModelHeaderHullBounds NOTIFY modelHeaderHullBoundsChanged)
    Q_PROPERTY(bool modelImportLods READ GetModelImportLods WRITE SetModelImportLods NOTIFY modelImportLodsChanged)
    Q_PROPERTY(bool modelWriteWeaponPrefab READ GetModelWriteWeaponPrefab WRITE SetModelWriteWeaponPrefab NOTIFY modelWriteWeaponPrefabChanged)

    Q_PROPERTY(QString pcfFile READ GetPcfFile NOTIFY pcfFileChanged)
    Q_PROPERTY(bool particleAllowDepthBlend READ GetParticleAllowDepthBlend WRITE SetParticleAllowDepthBlend NOTIFY particleAllowDepthBlendChanged)
    Q_PROPERTY(bool particleDisableDiffuse READ GetParticleDisableDiffuse WRITE SetParticleDisableDiffuse NOTIFY particleDisableDiffuseChanged)
    Q_PROPERTY(QString theme READ GetTheme WRITE SetTheme NOTIFY themeChanged)

public:
    enum TabIndex {
        TAB_MAP = 0,
        TAB_MODEL = 1,
        TAB_PARTICLE = 2
    };

    explicit Backend(QObject *parent = nullptr);

    int GetActiveTab() const { return activeTab; }
    void SetActiveTab(int val) {
        if (IsGoingWarn()) {
            emit activeTabChanged();
            return;
        }
        if(activeTab != val) { activeTab = val; emit activeTabChanged(); UpdateCanGo(); }
    }

    QString GetMdlFile() const { return mdlFile; }

    QStringList GetCs2AddonsList() const { return cs2AddonsList; }

    QString GetSelectedMdlAddon() const { return selectedMdlAddon; }
    void SetSelectedMdlAddon(const QString& addon) {
        if (IsGoingWarn()) {
            emit selectedMdlAddonChanged();
            return;
        }
        if(selectedMdlAddon != addon) { selectedMdlAddon = addon; emit selectedMdlAddonChanged(); UpdateCanGo(); }
    }

    bool GetModelSkipAnimation() const { return modelSkipAnimation; }
    void SetModelSkipAnimation(bool val) {
        if (IsGoingWarn()) {
            emit modelSkipAnimationChanged();
            return;
        }
        if(modelSkipAnimation != val) { modelSkipAnimation = val; emit modelSkipAnimationChanged(); SaveToCfg(); }
    }

    bool GetModelChangeBindpose() const { return modelChangeBindpose; }
    void SetModelChangeBindpose(bool val) {
        if (IsGoingWarn()) {
            emit modelChangeBindposeChanged();
            return;
        }
        if(modelChangeBindpose != val) { modelChangeBindpose = val; emit modelChangeBindposeChanged(); SaveToCfg(); }
    }

    bool GetModelOverrideLean() const { return modelOverrideLean; }
    void SetModelOverrideLean(bool val) {
        if (IsGoingWarn()) {
            emit modelOverrideLeanChanged();
            return;
        }
        if(modelOverrideLean != val) { modelOverrideLean = val; emit modelOverrideLeanChanged(); SaveToCfg(); }
    }

    bool GetModelHeaderHullBounds() const { return modelHeaderHullBounds; }
    void SetModelHeaderHullBounds(bool val) {
        if (IsGoingWarn()) {
            emit modelHeaderHullBoundsChanged();
            return;
        }
        if(modelHeaderHullBounds != val) { modelHeaderHullBounds = val; emit modelHeaderHullBoundsChanged(); SaveToCfg(); }
    }

    bool GetModelImportLods() const { return modelImportLods; }
    void SetModelImportLods(bool val) {
        if (IsGoingWarn()) {
            emit modelImportLodsChanged();
            return;
        }
        if(modelImportLods != val) { modelImportLods = val; emit modelImportLodsChanged(); SaveToCfg(); }
    }

    bool GetModelWriteWeaponPrefab() const { return modelWriteWeaponPrefab; }
    void SetModelWriteWeaponPrefab(bool val) {
        if (IsGoingWarn()) {
            emit modelWriteWeaponPrefabChanged();
            return;
        }
        if(modelWriteWeaponPrefab != val) { modelWriteWeaponPrefab = val; emit modelWriteWeaponPrefabChanged(); SaveToCfg(); }
    }

    QString GetPcfFile() const { return pcfFile; }

    bool GetParticleAllowDepthBlend() const { return particleAllowDepthBlend; }
    void SetParticleAllowDepthBlend(bool val) {
        if (IsGoingWarn()) {
            emit particleAllowDepthBlendChanged();
            return;
        }
        if(particleAllowDepthBlend != val) { particleAllowDepthBlend = val; emit particleAllowDepthBlendChanged(); SaveToCfg(); }
    }

    bool GetParticleDisableDiffuse() const { return particleDisableDiffuse; }
    void SetParticleDisableDiffuse(bool val) {
        if (IsGoingWarn()) {
            emit particleDisableDiffuseChanged();
            return;
        }
        if(particleDisableDiffuse != val) { particleDisableDiffuse = val; emit particleDisableDiffuseChanged(); SaveToCfg(); }
    }

    static Backend* instance();

    QString GetTheme() const { return theme; }
    void SetTheme(const QString& val);

    QString GetCs2Basefolder() const { return cs2Basefolder; }
    QString GetS1gameBasefolder() const {
        if (s1GameType == "css") return cssgamedir;
        if (s1GameType == "hl2") return hl2gamedir;
        if (s1GameType == "l4d") return l4dgamedir;
        if (s1GameType == "l4d2") return l4d2gamedir;
        if (s1GameType == "portal") return portalgamedir;
        if (s1GameType == "portal2") return portal2gamedir;
        if (s1GameType == "tf2") return tf2gamedir;
        if (s1GameType == "gmod") return gmodgamedir;
        if (s1GameType == "blackmesa") return blackmesagamedir;
        return csgogamedir;
    }
    QString GetS1GameType() const { return s1GameType; }
    QString GetVmfDefaultPathUrl() const { return QUrl::fromLocalFile(vmfDefaultPath).toString(); }
    QString GetBspFile() const { return bspFile; }
    QString GetContentFolder() const { return contentFolder; }
    QString GetAddonName() const { return addonName; }
    void SetAddonName(const QString& name) {
        if (IsGoingWarn()) {
            emit addonNameChanged();
            return;
        }
        if(addonName != name) { addonName = name; emit addonNameChanged(); UpdateCanGo(); }
    }

    bool GetKeepFuncDetailAsBrush() const { return keepFuncDetailAsBrush; }
    void SetKeepFuncDetailAsBrush(bool val) {
        if (IsGoingWarn()) {
            emit keepFuncDetailAsBrushChanged();
            return;
        }
        if(keepFuncDetailAsBrush != val) { keepFuncDetailAsBrush = val; emit keepFuncDetailAsBrushChanged(); SaveToCfg(); }
    }

    bool GetUsebsp() const { return usebsp; }
    void SetUsebsp(bool val) {
        if (IsGoingWarn()) {
            emit usebspChanged();
            return;
        }
        if(usebsp != val) { usebsp = val; emit usebspChanged(); if(!usebsp) SetUsebspNomergeinstances(false); GetLaunchOptions(); SaveToCfg(); }
    }

    bool GetUsebspNomergeinstances() const { return usebspNomergeinstances; }
    void SetUsebspNomergeinstances(bool val) {
        if (IsGoingWarn()) {
            emit usebspNomergeinstancesChanged();
            return;
        }
        if(usebspNomergeinstances != val) { usebspNomergeinstances = val; emit usebspNomergeinstancesChanged(); if(usebspNomergeinstances) SetUsebsp(true); GetLaunchOptions(); SaveToCfg(); }
    }

    bool GetSkipdeps() const { return skipdeps; }
    void SetSkipdeps(bool val) {
        if (IsGoingWarn()) {
            emit skipdepsChanged();
            return;
        }
        if(skipdeps != val) { skipdeps = val; emit skipdepsChanged(); GetLaunchOptions(); SaveToCfg(); }
    }

    bool GetCanGo() const {
        if (activeTab == TAB_MODEL) {
            return !cs2Basefolder.isEmpty() && !GetS1gameBasefolder().isEmpty() && !mdlFile.isEmpty() && !selectedMdlAddon.isEmpty() && !isGoing;
        }
        if (activeTab == TAB_MAP) {
            return !cs2Basefolder.isEmpty() && !GetS1gameBasefolder().isEmpty() && (!bspFile.isEmpty() || !contentFolder.isEmpty()) && !isGoing;
        }
        if (activeTab == TAB_PARTICLE) {
            return !cs2Basefolder.isEmpty() && !GetS1gameBasefolder().isEmpty() && !pcfFile.isEmpty() && !selectedMdlAddon.isEmpty() && !isGoing;
        }
        return false;
    }
    bool GetIsGoing() const { return isGoing; }
    QString GetCurrentVersion() const;

    void AppAboutToQuit();

public slots:
    void SelectCs2FolderDialog(const QUrl& url);
    void SelectS1FolderDialog(const QUrl& url);
    void SelectVmfDialog(const QUrl& url);
    void SelectBspDialog(const QUrl& url);
    void SelectMdlDialog(const QUrl& url);
    void SelectPcfDialog(const QUrl& url);
    void RefreshCs2AddonsList();
    void ValidateCs2();
    void ValidateS1();
    void SetS1GameType(const QString& type);
    void Start();
    void Stop();
    void CheckForUpdate();
    void AutoCheckForUpdate();
    bool requestConfirmation(const QString& title, const QString& msg);
    void setConfirmationResult(bool result);

signals:
    void cs2BasefolderChanged();
    void s1gameBasefolderChanged();
    void s1GameTypeChanged();
    void vmfDefaultPathUrlChanged();
    void bspFileChanged();
    void contentFolderChanged();
    void addonNameChanged();
    void keepFuncDetailAsBrushChanged();
    void usebspChanged();
    void usebspNomergeinstancesChanged();
    void skipdepsChanged();
    void canGoChanged();
    void isGoingChanged();

    void activeTabChanged();
    void mdlFileChanged();
    void cs2AddonsListChanged();
    void selectedMdlAddonChanged();
    void modelSkipAnimationChanged();
    void modelChangeBindposeChanged();
    void modelOverrideLeanChanged();
    void modelHeaderHullBoundsChanged();
    void modelImportLodsChanged();
    void modelWriteWeaponPrefabChanged();

    void pcfFileChanged();
    void particleAllowDepthBlendChanged();
    void particleDisableDiffuseChanged();
    void themeChanged();

    void logMessage(const QString& msg);
    void alertMessage(const QString& title, const QString& msg);
    void askQmlConfirmation(const QString& title, const QString& msg);
    void updateAvailable(const QString& version, const QString& notes, const QString& url);
    void noUpdateAvailable();

private:
    QString appDir;
    QString vmfDefaultPath;
    QString cs2Basefolder;
    QString csgogamedir;
    QString cssgamedir;
    QString hl2gamedir;
    QString l4dgamedir;
    QString l4d2gamedir;
    QString portalgamedir;
    QString portal2gamedir;
    QString tf2gamedir;
    QString gmodgamedir;
    QString blackmesagamedir;
    QString s1GameType; // "csgo", "css", etc.
    QString contentFolder;
    QString contentFolderToSave;
    QString addonName;
    QString mapName;
    bool vpkSignaturesMoved;
    QString bspFile;
    QString launchOptions;
    bool keepFuncDetailAsBrush = false;
    bool usebsp = true;
    bool usebspNomergeinstances = false;
    bool skipdeps = false;
    bool isGoing = false;

    int activeTab = TAB_MAP;
    QString mdlFile;
    QString pcfFile;
    QStringList cs2AddonsList;
    QString selectedMdlAddon;
    QString theme;
    bool modelSkipAnimation = false;
    bool modelChangeBindpose = false;
    bool modelOverrideLean = false;
    bool modelHeaderHullBounds = false;
    bool modelImportLods = false;
    bool modelWriteWeaponPrefab = false;

    bool particleAllowDepthBlend = false;
    bool particleDisableDiffuse = false;

    QNetworkAccessManager* networkManager;

    static Backend* s_instance;
    QMutex confirmMutex;
    QWaitCondition confirmCond;
    bool confirmResult = false;
    bool confirmInProgress = false;

    std::unique_ptr<QFile> logFile;
    std::unique_ptr<QTextStream> logStream;
    mutable QMutex logMutex;
    mutable QMutex vpkMutex;

    void SetCs2Folder(const QString& path);
    void SetS1Folder(const QString& path);

    void LoadFromCfg();
    void SaveToCfg();
    void GetLaunchOptions();
    void UpdateCanGo();

    bool IsGoingWarn();
    bool IsValidCs2(const QString& path);
    bool IsValidS1(const QString& path, const QString& type);
    void AutoDetectPaths();
    void CheckForUpdateInternal(bool isManual);
    void ApplyTheme(const QString& val);

    bool RunMapImportWorkflow(Miscellaneous::Options opts);
    bool RunModelImportWorkflow(Miscellaneous::Options opts, const QString& mdlPath);
    bool RunParticleImportWorkflow(Miscellaneous::Options opts, const QString& pcfPath);
};

#endif // UI_H
