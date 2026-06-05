#pragma once

#include <string>
#include <functional>
#include <vector>

class AppCore {
public:
    using LogCallback = std::function<void(const std::string&)>;

    struct Options {
        std::string cs2_basefolder;
        std::string s1game_basefolder;
        std::string s1_game_type; // "css" or "csgo"
        std::string content_folder;
        std::string map_name;
        std::string bsp_file;
        std::string app_dir;
        std::string addon_name;

        bool usebsp;
        bool usebsp_nomergeinstances;
        bool skipdeps;

        LogCallback logger;
    };

    static bool check_java();
    static void fix_vmf_from_bsp(const std::string& vmf_path, LogCallback log);
    static void move_vpk_signatures(const std::string& cs2_basefolder, bool& vpk_signatures_moved);
    static void restore_vpk_signatures(const std::string& cs2_basefolder);

    // Decompiles the BSP, moves unpacked files, and moves materials/models folders into s1gamedir
    static void process_bsp(Options& options);

    static int run_command_sync(const std::string& cmd, LogCallback logger);
};
