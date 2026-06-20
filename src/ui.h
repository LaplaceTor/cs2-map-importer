#ifndef UI_H
#define UI_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QUrl>

class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString cs2_basefolder READ GetCs2Basefolder NOTIFY cs2BasefolderChanged)
    Q_PROPERTY(QString s1game_basefolder READ GetS1gameBasefolder NOTIFY s1gameBasefolderChanged)
    Q_PROPERTY(QString s1_game_type READ GetS1GameType NOTIFY s1GameTypeChanged)
    Q_PROPERTY(QString vmf_default_path_url READ GetVmfDefaultPathUrl NOTIFY vmfDefaultPathUrlChanged)
    Q_PROPERTY(QString bsp_file READ GetBspFile NOTIFY bspFileChanged)
    Q_PROPERTY(QString content_folder READ GetContentFolder NOTIFY contentFolderChanged)
    Q_PROPERTY(QString addon_name READ GetAddonName WRITE SetAddonName NOTIFY addonNameChanged)
    Q_PROPERTY(bool usebsp READ GetUsebsp WRITE SetUsebsp NOTIFY usebspChanged)
    Q_PROPERTY(bool usebsp_nomergeinstances READ GetUsebspNomergeinstances WRITE SetUsebspNomergeinstances NOTIFY usebspNomergeinstancesChanged)
    Q_PROPERTY(bool skipdeps READ GetSkipdeps WRITE SetSkipdeps NOTIFY skipdepsChanged)
    Q_PROPERTY(bool can_go READ GetCanGo NOTIFY canGoChanged)
    Q_PROPERTY(bool is_going READ GetIsGoing NOTIFY isGoingChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

    QString GetCs2Basefolder() const { return cs2_basefolder; }
    QString GetS1gameBasefolder() const {
        if (s1_game_type == "css") return cssgamedir;
        if (s1_game_type == "hl2") return hl2gamedir;
        if (s1_game_type == "l4d") return l4dgamedir;
        if (s1_game_type == "l4d2") return l4d2gamedir;
        if (s1_game_type == "portal") return portalgamedir;
        if (s1_game_type == "portal2") return portal2gamedir;
        if (s1_game_type == "tf2") return tf2gamedir;
        if (s1_game_type == "gmod") return gmodgamedir;
        return csgogamedir;
    }
    QString GetS1GameType() const { return s1_game_type; }
    QString GetVmfDefaultPathUrl() const { return QUrl::fromLocalFile(vmf_default_path).toString(); }
    QString GetBspFile() const { return bsp_file; }
    QString GetContentFolder() const { return content_folder; }
    QString GetAddonName() const { return addon_name; }
    void SetAddonName(const QString& name) { if(addon_name != name) { addon_name = name; emit addonNameChanged(); UpdateCanGo(); } }

    bool GetUsebsp() const { return usebsp; }
    void SetUsebsp(bool val) { if(usebsp != val) { usebsp = val; emit usebspChanged(); if(!usebsp) SetUsebspNomergeinstances(false); GetLaunchOptions(); SaveToCfg(); } }

    bool GetUsebspNomergeinstances() const { return usebsp_nomergeinstances; }
    void SetUsebspNomergeinstances(bool val) { if(usebsp_nomergeinstances != val) { usebsp_nomergeinstances = val; emit usebspNomergeinstancesChanged(); if(usebsp_nomergeinstances) SetUsebsp(true); GetLaunchOptions(); SaveToCfg(); } }

    bool GetSkipdeps() const { return skipdeps; }
    void SetSkipdeps(bool val) { if(skipdeps != val) { skipdeps = val; emit skipdepsChanged(); GetLaunchOptions(); SaveToCfg(); } }

    bool GetCanGo() const { return !cs2_basefolder.isEmpty() && !GetS1gameBasefolder().isEmpty() && (!bsp_file.isEmpty() || !content_folder.isEmpty()) && !is_going; }
    bool GetIsGoing() const { return is_going; }

    void AppAboutToQuit();

public slots:
    void SelectCs2FolderDialog(const QUrl& url);
    void SelectS1FolderDialog(const QUrl& url);
    void SelectVmfDialog(const QUrl& url);
    void SelectBspDialog(const QUrl& url);
    void ValidateCs2();
    void ValidateS1();
    void SetS1GameType(const QString& type);
    void Start();
    void Stop();

signals:
    void cs2BasefolderChanged();
    void s1gameBasefolderChanged();
    void s1GameTypeChanged();
    void vmfDefaultPathUrlChanged();
    void bspFileChanged();
    void contentFolderChanged();
    void addonNameChanged();
    void usebspChanged();
    void usebspNomergeinstancesChanged();
    void skipdepsChanged();
    void canGoChanged();
    void isGoingChanged();

    void logMessage(const QString& msg);
    void alertMessage(const QString& title, const QString& msg);

private:
    QString app_dir;
    bool java_installed;
    QString vmf_default_path;
    QString cs2_basefolder;
    QString csgogamedir;
    QString cssgamedir;
    QString hl2gamedir;
    QString l4dgamedir;
    QString l4d2gamedir;
    QString portalgamedir;
    QString portal2gamedir;
    QString tf2gamedir;
    QString gmodgamedir;
    QString s1_game_type; // "csgo", "css", etc.
    QString content_folder;
    QString content_folder_to_save;
    QString addon_name;
    QString map_name;
    bool vpk_signatures_moved;
    QString bsp_file;
    QString launch_options;
    bool usebsp = true;
    bool usebsp_nomergeinstances = false;
    bool skipdeps = false;
    bool is_going = false;

    QFile* log_file;
    QTextStream* log_stream;

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
