#pragma once

#include <string>
#include <functional>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

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
  
private:
    static std::string parse_mapversion(const std::vector<std::string>& lines, bool& found);
    static std::vector<std::string> extract_visgroups(const std::vector<std::string>& lines, std::vector<std::string>& remaining_lines);
    static std::vector<std::string> insert_required_blocks(const std::vector<std::string>& lines, const std::string& mapversion, const std::vector<std::string>& visgroups);
    static std::vector<std::string> patch_dispinfo(const std::vector<std::string>& lines);
};
