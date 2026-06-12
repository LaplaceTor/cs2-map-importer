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

    Q_PROPERTY(QString cs2_basefolder READ getCs2Basefolder NOTIFY cs2BasefolderChanged)
    Q_PROPERTY(QString s1game_basefolder READ getS1gameBasefolder NOTIFY s1gameBasefolderChanged)
    Q_PROPERTY(QString s1_game_type READ getS1GameType NOTIFY s1GameTypeChanged)
    Q_PROPERTY(QString vmf_default_path_url READ getVmfDefaultPathUrl NOTIFY vmfDefaultPathUrlChanged)
    Q_PROPERTY(QString bsp_file READ getBspFile NOTIFY bspFileChanged)
    Q_PROPERTY(QString content_folder READ getContentFolder NOTIFY contentFolderChanged)
    Q_PROPERTY(QString addon_name READ getAddonName WRITE setAddonName NOTIFY addonNameChanged)
    Q_PROPERTY(bool usebsp READ getUsebsp WRITE setUsebsp NOTIFY usebspChanged)
    Q_PROPERTY(bool usebsp_nomergeinstances READ getUsebspNomergeinstances WRITE setUsebspNomergeinstances NOTIFY usebspNomergeinstancesChanged)
    Q_PROPERTY(bool skipdeps READ getSkipdeps WRITE setSkipdeps NOTIFY skipdepsChanged)
    Q_PROPERTY(bool can_go READ getCanGo NOTIFY canGoChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend();

    QString getCs2Basefolder() const { return cs2_basefolder; }
    QString getS1gameBasefolder() const { return s1_game_type == "css" ? cssgamedir : csgogamedir; }
    QString getS1GameType() const { return s1_game_type; }
    QString getVmfDefaultPathUrl() const { return QUrl::fromLocalFile(vmf_default_path).toString(); }
    QString getBspFile() const { return bsp_file; }
    QString getContentFolder() const { return content_folder; }
    QString getAddonName() const { return addon_name; }
    void setAddonName(const QString& name) { if(addon_name != name) { addon_name = name; emit addonNameChanged(); updateCanGo(); } }

    bool getUsebsp() const { return usebsp; }
    void setUsebsp(bool val) { if(usebsp != val) { usebsp = val; emit usebspChanged(); if(!usebsp) setUsebspNomergeinstances(false); get_launch_options(); save_to_cfg(); } }

    bool getUsebspNomergeinstances() const { return usebsp_nomergeinstances; }
    void setUsebspNomergeinstances(bool val) { if(usebsp_nomergeinstances != val) { usebsp_nomergeinstances = val; emit usebspNomergeinstancesChanged(); if(usebsp_nomergeinstances) setUsebsp(true); get_launch_options(); save_to_cfg(); } }

    bool getSkipdeps() const { return skipdeps; }
    void setSkipdeps(bool val) { if(skipdeps != val) { skipdeps = val; emit skipdepsChanged(); get_launch_options(); save_to_cfg(); } }

    bool getCanGo() const { return !cs2_basefolder.isEmpty() && !getS1gameBasefolder().isEmpty() && (!bsp_file.isEmpty() || !content_folder.isEmpty()) && !is_going; }

    void appAboutToQuit();

public slots:
    void log(const QString& message);
    void select_cs2_folder_dialog(const QUrl& url);
    void select_s1_folder_dialog(const QUrl& url);
    void select_vmf_dialog(const QUrl& url);
    void select_bsp_dialog(const QUrl& url);
    void validate_cs2();
    void validate_s1();
    void set_s1_game_type(const QString& type);
    void go();

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

    void logMessage(const QString& msg);
    void alertMessage(const QString& title, const QString& msg);

private:
    QString app_dir;
    bool java_installed;
    QString vmf_default_path;
    QString cs2_basefolder;
    QString csgogamedir;
    QString cssgamedir;
    QString s1_game_type; // "csgo" or "css"
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

    void set_cs2_folder(const QString& path);
    void set_s1_folder(const QString& path);

    void load_from_cfg();
    void save_to_cfg();
    void get_launch_options();
    void updateCanGo();

    bool is_valid_cs2(const QString& path);
    bool is_valid_s1(const QString& path, const QString& type);
    void auto_detect_paths();
};

#endif // UI_H
