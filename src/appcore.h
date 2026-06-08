#pragma once

#include <QString>
#include <QStringList>
#include <functional>
#include <QAtomicInt>

class AppCore {
public:
    using LogCallback = std::function<void(const QString&)>;

    struct Options {
        QString cs2_basefolder;
        QString s1game_basefolder;
        QString s1_game_type; // "css" or "csgo"
        QString content_folder;
        QString map_name;
        QString bsp_file;
        QString app_dir;
        QString addon_name;

        bool usebsp;
        bool usebsp_nomergeinstances;
        bool skipdeps;

        LogCallback logger;
    };

    static bool check_java();
    static void fix_vmf_from_bsp(const QString& vmf_path, LogCallback log);
    static void move_vpk_signatures(const QString& cs2_basefolder, bool& vpk_signatures_moved);
    static void restore_vpk_signatures(const QString& cs2_basefolder);

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void process_bsp(Options& options);

    static int run_command_sync(const QString& cmd, LogCallback logger);
    static void cancel_all();

    static QAtomicInt cancel_import;

private:
    static QString parse_mapversion(const QStringList& lines, bool& found);
    static QStringList extract_visgroups(const QStringList& lines, QStringList& remaining_lines);
    static QStringList insert_required_blocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups);
    static QStringList patch_dispinfo(const QStringList& lines);
};
