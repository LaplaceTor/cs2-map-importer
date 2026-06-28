#ifndef UI_H
#define UI_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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
    Q_PROPERTY(QString materialFolder READ GetMaterialFolder NOTIFY materialFolderChanged)
    Q_PROPERTY(QString materialFileList READ GetMaterialFileList WRITE SetMaterialFileList NOTIFY materialFileListChanged)
    Q_PROPERTY(QString materialAddonName READ GetMaterialAddonName WRITE SetMaterialAddonName NOTIFY materialAddonNameChanged)
    Q_PROPERTY(bool usebsp READ GetUsebsp WRITE SetUsebsp NOTIFY usebspChanged)
    Q_PROPERTY(bool usebspNomergeinstances READ GetUsebspNomergeinstances WRITE SetUsebspNomergeinstances NOTIFY usebspNomergeinstancesChanged)
    Q_PROPERTY(bool skipdeps READ GetSkipdeps WRITE SetSkipdeps NOTIFY skipdepsChanged)
    Q_PROPERTY(bool canGo READ GetCanGo NOTIFY canGoChanged)
    Q_PROPERTY(bool isGoing READ GetIsGoing NOTIFY isGoingChanged)
    Q_PROPERTY(QString currentVersion READ GetCurrentVersion CONSTANT)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

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
        return csgogamedir;
    }
    QString GetS1GameType() const { return s1GameType; }
    QString GetVmfDefaultPathUrl() const { return QUrl::fromLocalFile(vmfDefaultPath).toString(); }
    QString GetBspFile() const { return bspFile; }
    QString GetContentFolder() const { return contentFolder; }
    QString GetAddonName() const { return addonName; }
    void SetAddonName(const QString& name) { if(addonName != name) { addonName = name; emit addonNameChanged(); UpdateCanGo(); } }

    QString GetMaterialFolder() const { return materialFolder; }
    QString GetMaterialFileList() const { return materialFileList; }
    void SetMaterialFileList(const QString& list) { if(materialFileList != list) { materialFileList = list; emit materialFileListChanged(); UpdateCanGo(); } }
    QString GetMaterialAddonName() const { return materialAddonName; }
    void SetMaterialAddonName(const QString& name) { if(materialAddonName != name) { materialAddonName = name; emit materialAddonNameChanged(); UpdateCanGo(); } }

    bool GetUsebsp() const { return usebsp; }
    void SetUsebsp(bool val) { if(usebsp != val) { usebsp = val; emit usebspChanged(); if(!usebsp) SetUsebspNomergeinstances(false); GetLaunchOptions(); SaveToCfg(); } }

    bool GetUsebspNomergeinstances() const { return usebspNomergeinstances; }
    void SetUsebspNomergeinstances(bool val) { if(usebspNomergeinstances != val) { usebspNomergeinstances = val; emit usebspNomergeinstancesChanged(); if(usebspNomergeinstances) SetUsebsp(true); GetLaunchOptions(); SaveToCfg(); } }

    bool GetSkipdeps() const { return skipdeps; }
    void SetSkipdeps(bool val) { if(skipdeps != val) { skipdeps = val; emit skipdepsChanged(); GetLaunchOptions(); SaveToCfg(); } }

    bool GetCanGo() const { return !cs2Basefolder.isEmpty() && !GetS1gameBasefolder().isEmpty() && (!bspFile.isEmpty() || !contentFolder.isEmpty()) && !isGoing; }
    bool GetIsGoing() const { return isGoing; }
    QString GetCurrentVersion() const;

    void AppAboutToQuit();

public slots:
    void SelectCs2FolderDialog(const QUrl& url);
    void SelectS1FolderDialog(const QUrl& url);
    void SelectVmfDialog(const QUrl& url);
    void SelectBspDialog(const QUrl& url);
    void SelectMaterialFolderDialog(const QUrl& url);
    void SelectMaterialAddonDialog(const QUrl& url);
    void ValidateCs2();
    void ValidateS1();
    void SetS1GameType(const QString& type);
    void Start();
    void StartMaterialImport();
    void Stop();
    void CheckForUpdate();

signals:
    void cs2BasefolderChanged();
    void s1gameBasefolderChanged();
    void s1GameTypeChanged();
    void vmfDefaultPathUrlChanged();
    void bspFileChanged();
    void contentFolderChanged();
    void addonNameChanged();
    void materialFolderChanged();
    void materialFileListChanged();
    void materialAddonNameChanged();
    void usebspChanged();
    void usebspNomergeinstancesChanged();
    void skipdepsChanged();
    void canGoChanged();
    void isGoingChanged();

    void logMessage(const QString& msg);
    void alertMessage(const QString& title, const QString& msg);
    void updateAvailable(const QString& version, const QString& notes, const QString& url);
    void noUpdateAvailable();

private:
    QString appDir;
    bool javaInstalled;
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
    QString s1GameType; // "csgo", "css", etc.
    QString contentFolder;
    QString contentFolderToSave;
    QString addonName;
    QString materialFolder;
    QString materialFileList;
    QString materialAddonName;
    QString mapName;
    bool vpkSignaturesMoved;
    QString bspFile;
    QString launchOptions;
    bool usebsp = true;
    bool usebspNomergeinstances = false;
    bool skipdeps = false;
    bool isGoing = false;

    QNetworkAccessManager* networkManager;

    QFile* logFile;
    QTextStream* logStream;

    void SetCs2Folder(const QString& path);
    void SetS1Folder(const QString& path);

    void LoadFromCfg();
    void SaveToCfg();
    void GetLaunchOptions();
    void UpdateCanGo();

    bool IsValidCs2(const QString& path);
    bool IsValidS1(const QString& path, const QString& type);
    void AutoDetectPaths();
};

#endif // UI_H
