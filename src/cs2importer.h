#ifndef CS2IMPORTER_H
#define CS2IMPORTER_H

#include <QMainWindow>
#include <QString>
#include <QProcess>

namespace Ui {
class MainWindow;
}

class Importer : public QMainWindow
{
    Q_OBJECT

public:
    explicit Importer(QWidget *parent = nullptr);
    ~Importer();

private slots:
    void select_cs2_folder();
    void select_csgo_folder();
    void select_vmf();
    void select_bsp();
    void validate_cs2();
    void validate_csgo();
    void on_usebsp_toggled(bool checked);
    void on_usebsp_nomergeinstances_toggled(bool checked);
    void get_addon();
    void get_launch_options();
    void go();

public slots:
    void log(const QString& message);

private:
    Ui::MainWindow *ui;

    QString app_dir;
    bool bspsrc_installed;
    bool java_installed;
    QString vmf_default_path;
    QString cs2_basefolder;
    QString s1game_basefolder;
    QString content_folder;
    QString content_folder_to_save;
    QString addon;
    QString map_name;
    bool vpk_signatures_moved;
    QString bsp_file;
    QString launch_options;
    bool check_java();
    bool check_bspsrc(const QString& base_path);

    void set_cs2_folder(const QString& path);
    void set_csgo_folder(const QString& path);

    void set_tooltips();
    void set_stylesheets();
    void load_from_cfg();
    void save_to_cfg();
    void fix_top_level_key(const QString& vmf_path);
    void move_vpk_signatures();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // CS2IMPORTER_H
