#ifndef UI_H
#define UI_H

#include <QMainWindow>
#include <QString>
#include <QFile>
#include <QTextStream>

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
    void select_s1_folder();
    void select_vmf();
    void select_bsp();
    void validate_cs2();
    void validate_s1();
    void on_usebsp_toggled(bool checked);
    void on_usebsp_nomergeinstances_toggled(bool checked);
    void get_addon_name();
    void get_launch_options();
    void go();

public slots:
    void log(const QString& message);

private:
    Ui::MainWindow *ui;

    QString app_dir;
    bool java_installed;
    QString vmf_default_path;
    QString cs2_basefolder;
    QString s1game_basefolder;
    QString s1_game_type; // "csgo" or "css"
    QString content_folder;
    QString content_folder_to_save;
    QString addon_name;
    QString map_name;
    bool vpk_signatures_moved;
    QString bsp_file;
    QString launch_options;

    QFile* log_file;
    QTextStream* log_stream;

    void set_cs2_folder(const QString& path);
    void set_s1_folder(const QString& path);

    void set_tooltips();
    void set_stylesheets();
    void load_from_cfg();
    void save_to_cfg();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // UI_H
