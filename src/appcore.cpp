#include "appcore.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>
#include <memory>
#include <stdexcept>
#include <array>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace fs = std::filesystem;

bool AppCore::check_java() {
#ifdef _WIN32
    std::string cmd = "java -version 2>&1";
#else
    std::string cmd = "java -version 2>&1";
#endif
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(POPEN(cmd.c_str(), "r"), PCLOSE);
    if (!pipe) return false;

    char buffer[128];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        output += buffer;
    }
    return output.find("version") != std::string::npos;
}

void AppCore::move_vpk_signatures(const std::string& cs2_basefolder, bool& vpk_signatures_moved) {
    if (cs2_basefolder.empty()) return;

    fs::path bin_folder = fs::path(cs2_basefolder) / "game" / "bin" / "win64";
    fs::path vpk_path = bin_folder / "vpk.signatures";
    fs::path temp_folder = bin_folder / "temp";
    fs::path temp_vpk_path = temp_folder / "vpk.signatures";

    if (fs::exists(vpk_path)) {
        if (!fs::exists(temp_folder)) {
            fs::create_directories(temp_folder);
        }
        if (fs::exists(temp_vpk_path)) {
            fs::remove(temp_vpk_path);
        }
        fs::rename(vpk_path, temp_vpk_path);
        vpk_signatures_moved = true;
    }
}

void AppCore::restore_vpk_signatures(const std::string& cs2_basefolder) {
    if (cs2_basefolder.empty()) return;

    fs::path bin_folder = fs::path(cs2_basefolder) / "game" / "bin" / "win64";
    fs::path vpk_path = bin_folder / "vpk.signatures";
    fs::path temp_vpk_path = bin_folder / "temp" / "vpk.signatures";

    if (fs::exists(temp_vpk_path)) {
        if (fs::exists(vpk_path)) {
            fs::remove(vpk_path);
        }
        fs::rename(temp_vpk_path, vpk_path);
    }
}

void AppCore::fix_vmf_from_bsp(const std::string& vmf_path, LogCallback log) {
    if (!fs::exists(vmf_path)) return;

    std::ifstream infile(vmf_path);
    if (!infile.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    std::string mapversion = "2";
    std::regex mapversion_regex("^\\s*\"mapversion\"\\s+\"([^\"]+)\"");
    bool mapversion_found = false;

    while (std::getline(infile, line)) {
        lines.push_back(line);
        std::smatch match;
        if (!mapversion_found && std::regex_search(line, match, mapversion_regex)) {
            mapversion = match[1].str();
            mapversion_found = true;
        }
    }
    infile.close();

    if (!mapversion_found) {
        log("No mapversion found in VMF. Aborting fix.");
        return;
    }

    int visgroups_start_idx = -1;
    int visgroups_end_idx = -1;
    bool has_versioninfo = false;
    bool has_viewsettings = false;
    bool has_cordon = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmed = lines[i];
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        if (!trimmed.empty()) {
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
        }

        if (trimmed == "visgroups" && visgroups_start_idx == -1) {
            visgroups_start_idx = i;
        }
        if (trimmed == "versioninfo") {
            has_versioninfo = true;
        }
        if (trimmed == "viewsettings") {
            has_viewsettings = true;
        }
        if (trimmed == "cordon") {
            has_cordon = true;
        }
    }

    std::vector<std::string> visgroups_lines;
    if (visgroups_start_idx != -1) {
        int open_brackets = 0;
        bool found_first_bracket = false;
        for (size_t i = visgroups_start_idx; i < lines.size(); ++i) {
            open_brackets += std::count(lines[i].begin(), lines[i].end(), '{');
            open_brackets -= std::count(lines[i].begin(), lines[i].end(), '}');
            if (lines[i].find('{') != std::string::npos) {
                found_first_bracket = true;
            }

            if (found_first_bracket && open_brackets == 0) {
                visgroups_end_idx = i;
                break;
            }
        }

        if (visgroups_end_idx != -1) {
            for (int i = visgroups_start_idx; i <= visgroups_end_idx; ++i) {
                visgroups_lines.push_back(lines[i]);
            }
            lines.erase(lines.begin() + visgroups_start_idx, lines.begin() + visgroups_end_idx + 1);
        }
    } else {
        log("No visgroups block found in VMF. Aborting fix.");
        return;
    }

    if (!has_versioninfo) {
        std::string versioninfo_block = "versioninfo\n{\n\t\"editorversion\" \"400\"\n\t\"editorbuild\" \"9999\"\n\t\"mapversion\" \"" + mapversion + "\"\n\t\"formatversion\" \"100\"\n\t\"prefab\" \"0\"\n}";
        lines.insert(lines.begin(), versioninfo_block);
    }

    if (!visgroups_lines.empty()) {
        int insert_idx = has_versioninfo ? 0 : 1;
        lines.insert(lines.begin() + insert_idx, visgroups_lines.begin(), visgroups_lines.end());
    }

    if (!has_viewsettings) {
        std::string viewsettings_block = "viewsettings\n{\n\t\"bSnapToGrid\" \"1\"\n\t\"bShowGrid\" \"1\"\n\t\"bShowLogicalGrid\" \"0\"\n\t\"nGridSpacing\" \"64\"\n\t\"bShow3DGrid\" \"0\"\n}";
        int insert_idx = (has_versioninfo ? 0 : 1) + visgroups_lines.size();
        lines.insert(lines.begin() + insert_idx, viewsettings_block);
    }

    if (!has_cordon) {
        std::string cordon_block = "cordon\n{\n\t\"mins\" \"(-1024 -1024 -1024)\"\n\t\"maxs\" \"(1024 1024 1024)\"\n\t\"active\" \"0\"\n}";
        lines.push_back(cordon_block);
    }

    std::vector<std::string> out_lines;
    bool in_dispinfo = false;
    int open_brackets_disp = 0;
    bool in_dispinfo_bracket = false;
    bool has_offsets = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string l = lines[i];
        std::string trimmed = l;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        if (!trimmed.empty()) {
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
        }

        if (trimmed == "dispinfo") {
            in_dispinfo = true;
            open_brackets_disp = 0;
            in_dispinfo_bracket = false;
            has_offsets = false;
        }

        if (in_dispinfo) {
            open_brackets_disp += std::count(l.begin(), l.end(), '{');
            open_brackets_disp -= std::count(l.begin(), l.end(), '}');
            if (!in_dispinfo_bracket && l.find('{') != std::string::npos) {
                in_dispinfo_bracket = true;
            }

            if (trimmed == "offsets" || trimmed == "offset_normals") {
                has_offsets = true;
            }

            if (in_dispinfo_bracket && open_brackets_disp == 0) {
                in_dispinfo = false;
            }
        }

        if (in_dispinfo && trimmed == "alphas" && !has_offsets) {
            size_t first_non_space = l.find_first_not_of(" \t");
            std::string indent = "";
            if (first_non_space != std::string::npos) {
                indent = l.substr(0, first_non_space);
            }

            std::string offsets_block =
                indent + "offsets\n" +
                indent + "{\n" +
                indent + "\t\"row0\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row1\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row2\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row3\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row4\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row5\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row6\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row7\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "\t\"row8\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                indent + "}\n" +
                indent + "offset_normals\n" +
                indent + "{\n" +
                indent + "\t\"row0\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row1\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row2\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row3\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row4\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row5\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row6\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row7\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "\t\"row8\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                indent + "}";
            out_lines.push_back(offsets_block);
        }

        out_lines.push_back(l);
    }

    std::ofstream outfile(vmf_path);
    if (outfile.is_open()) {
        for (const auto& l : out_lines) {
            outfile << l << "\n";
        }
    }
}

int AppCore::run_command_sync(const std::string& cmd, LogCallback logger) {
    if (logger) logger(cmd);
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        if (logger) logger("Failed to create pipe.");
        return -1;
    }
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        if (logger) logger("Failed to set handle information.");
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return -1;
    }
    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    STARTUPINFOA siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    // Use CREATE_NO_WINDOW to hide the console window
    std::string writableCmd = cmd;
    BOOL bSuccess = CreateProcessA(
        NULL,
        writableCmd.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &siStartInfo,
        &piProcInfo
    );
    if (!bSuccess) {
        if (logger) logger("Failed to run command.");
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return -1;
    }
    CloseHandle(hChildStd_OUT_Wr);
    DWORD dwRead;
    CHAR chBuf[4096];
    std::string outputBuffer = "";
    while (true) {
        bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break;
        chBuf[dwRead] = '\0';
        outputBuffer += chBuf;
        size_t pos;
        while ((pos = outputBuffer.find('\n')) != std::string::npos) {
            std::string line_out = outputBuffer.substr(0, pos);
            if (!line_out.empty() && line_out.back() == '\r') line_out.pop_back();
            if (logger) logger(line_out);
            outputBuffer.erase(0, pos + 1);
        }
    }
    if (!outputBuffer.empty()) {
        if (!outputBuffer.empty() && outputBuffer.back() == '\r') outputBuffer.pop_back();
        if (logger) logger(outputBuffer);
    }
    CloseHandle(hChildStd_OUT_Rd);
    DWORD exitCode;
    WaitForSingleObject(piProcInfo.hProcess, INFINITE);
    GetExitCodeProcess(piProcInfo.hProcess, &exitCode);

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return exitCode;
#else
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(POPEN(cmd.c_str(), "r"), PCLOSE);
    if (!pipe) {
        if (logger) logger("Failed to run command.");
        return -1;
    }
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string output = buffer.data();
        if (!output.empty() && output.back() == '\n') output.pop_back();
        if (!output.empty() && output.back() == '\r') output.pop_back();
        if (!output.empty()) {
             if (logger) logger(output);
        }
    }
    return PCLOSE(pipe.release());
#endif
}

void AppCore::process_bsp(Options& options) {
    fs::path app_dir = options.app_dir;
    fs::path maps_dir = app_dir / "maps";
    fs::create_directories(maps_dir);

    fs::path vmf_dest = maps_dir / (options.map_name + ".vmf");
    fs::path bspsrc_jar = app_dir / "bspsrc.jar";

    if (!fs::exists(bspsrc_jar)) {
        throw std::runtime_error("Could not find bspsrc.jar at " + bspsrc_jar.string());
    }

    options.logger("Decompiling BSP: " + options.bsp_file);

    std::string decomp_cmd = "java -jar \"" + bspsrc_jar.string() + "\" \"" + options.bsp_file + "\" -o \"" + vmf_dest.string() + "\" --unpack_embedded";
    int ret = run_command_sync(decomp_cmd, options.logger);
    if (ret != 0) {
        throw std::runtime_error("BSP Decompilation failed.");
    }

    fs::path unpacked_dir;
    std::vector<fs::path> possible_locations = {
        fs::current_path() / options.map_name,
        app_dir / options.map_name,
        fs::path(options.bsp_file).parent_path() / options.map_name,
        maps_dir / options.map_name
    };

    for (const auto& loc : possible_locations) {
        if (fs::exists(loc) && fs::is_directory(loc)) {
            unpacked_dir = loc;
            break;
        }
    }

    fs::path target_unpacked_dir = maps_dir / options.map_name;
    if (!unpacked_dir.empty()) {
        options.logger("Found unpacked files at " + unpacked_dir.string());

        if (unpacked_dir != target_unpacked_dir) {
            if (fs::exists(target_unpacked_dir)) {
                fs::remove_all(target_unpacked_dir);
            }
            std::error_code ec;
            fs::rename(unpacked_dir, target_unpacked_dir, ec);
            if (!ec) {
                options.logger("Moved unpacked directory to " + target_unpacked_dir.string());
            } else {
                options.logger("Failed to rename unpacked directory to " + target_unpacked_dir.string() + ". Attempting recursive copy...");
                std::error_code copy_ec;
                fs::copy(unpacked_dir, target_unpacked_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing, copy_ec);
                if (!copy_ec) {
                    std::error_code remove_ec;
                    fs::remove_all(unpacked_dir, remove_ec);
                    if (!remove_ec) {
                        options.logger("Successfully copied and removed original unpacked directory.");
                    } else {
                        options.logger("Successfully copied but failed to remove original unpacked directory: " + unpacked_dir.string());
                    }
                } else {
                    options.logger("Failed to copy unpacked directory: " + copy_ec.message());
                }
            }
        }
    } else {
        options.logger("Could not find unpacked embedded files directory '" + options.map_name + "'");
    }

    fix_vmf_from_bsp(vmf_dest.string(), options.logger);

    fs::path target_maps_dir = app_dir / "maps" / options.map_name / "maps";
    fs::create_directories(target_maps_dir);
    fs::path final_vmf_dest = target_maps_dir / (options.map_name + ".vmf");

    if (fs::exists(final_vmf_dest)) {
        fs::remove(final_vmf_dest);
    }

    std::error_code ec;
    fs::rename(vmf_dest, final_vmf_dest, ec);
    if (!ec) {
        options.logger("Moved VMF to: " + final_vmf_dest.string());
    } else {
        options.logger("Failed to move VMF to: " + final_vmf_dest.string());
    }

    options.content_folder = (app_dir / "maps" / options.map_name).string();
    options.logger("Decompiled and prepared at: " + final_vmf_dest.string());

    // Copy materials and models to s1gamedir
    std::string s1_subfolder = (options.s1_game_type == "css") ? "cstrike" : "csgo";
    fs::path s1gamedir = fs::path(options.s1game_basefolder) / s1_subfolder;

    if (fs::exists(target_unpacked_dir)) {
        fs::path src_materials = target_unpacked_dir / "materials";
        fs::path dest_materials = s1gamedir / "materials";
        if (fs::exists(src_materials) && fs::is_directory(src_materials)) {
            options.logger("Copying materials to " + dest_materials.string());
            fs::copy(src_materials, dest_materials, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }

        fs::path src_models = target_unpacked_dir / "models";
        fs::path dest_models = s1gamedir / "models";
        if (fs::exists(src_models) && fs::is_directory(src_models)) {
            options.logger("Copying models to " + dest_models.string());
            fs::copy(src_models, dest_models, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }
    }
}
