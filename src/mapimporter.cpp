#include "mapimporter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <stdexcept>
#include <array>
#include <memory>
#include <cstdio>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

MapImporter::MapImporter(QObject *parent) : QObject(parent),
    usebsp(false), nomergeinstances(false), skipdeps(false)
{
}

void MapImporter::setPaths(const QString& s1game, const QString& s1content,
                           const QString& s2game, const QString& addon, const QString& map)
{
    s1gamecsgo = s1game.toStdString();
    s1contentcsgo = s1content.toStdString();
    s2gamecsgo = s2game.toStdString();
    s2addon = addon.toStdString();
    mapname = map.toStdString();

    std::string s2gameaddondir = "game\\csgo_addons\\" + s2addon;
    s2gameaddon = s2gamecsgo;

    size_t pos = s2gameaddon.find("game\\csgo");
    if (pos != std::string::npos) {
        s2gameaddon.replace(pos, 9, s2gameaddondir);
    } else {
        pos = s2gameaddon.find("game/csgo");
        if (pos != std::string::npos) {
            s2gameaddon.replace(pos, 9, s2gameaddondir);
        }
    }

    s2contentcsgo = s2gameaddon;
    pos = s2contentcsgo.find("game\\csgo_addons");
    if (pos != std::string::npos) {
        s2contentcsgo.replace(pos, 16, "content\\csgo_addons");
    } else {
        pos = s2contentcsgo.find("game/csgo_addons");
        if (pos != std::string::npos) {
            s2contentcsgo.replace(pos, 16, "content/csgo_addons");
        }
    }
    s2contentcsgoimported = s2contentcsgo;
}

void MapImporter::setOptions(bool bsp, bool nomerge, bool skip)
{
    usebsp = bsp;
    nomergeinstances = nomerge;
    skipdeps = skip;
}

void MapImporter::setBinPath(const QString& bp)
{
    binPath = bp.toStdString();
}

void MapImporter::emitLog(const std::string& msg)
{
    emit logMessage(QString::fromStdString(msg));
}

int MapImporter::runCommand(const std::string& program, const std::vector<std::string>& args)
{
    std::string cmd;

#ifdef _WIN32
    // Windows requires environment variable modification via cmd /c set PATH=... && command
    cmd = "cmd.exe /c \"set PATH=" + binPath + ";%PATH% && " + program;
#else
    cmd = "PATH=\"" + binPath + ":$PATH\" " + program;
#endif

    for (const auto& arg : args) {
        cmd += " " + arg;
    }

#ifdef _WIN32
    cmd += "\"";
#endif

    emitLog("> " + program + " " + cmd);

    // Using popen to read the output
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif

    if (!pipe) {
        emitLog("Failed to run command: " + program);
        return -1;
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string out(buffer.data());
        out.erase(out.find_last_not_of(" \n\r\t") + 1); // trim right
        if (!out.empty()) {
            emitLog(out);
        }
    }

#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

// Custom replace helper
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty()) return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void MapImporter::run()
{
    emitLog("Starting Map Import...");

    try {
        std::vector<std::string> mapImportArgs;
        mapImportArgs.push_back("-retail");
        mapImportArgs.push_back("-nop4");
        mapImportArgs.push_back("-nop4sync");
        if (usebsp) mapImportArgs.push_back("-usebsp");
        if (nomergeinstances) mapImportArgs.push_back("-usebsp_nomergeinstances");
        mapImportArgs.push_back("-src1gameinfodir");
        mapImportArgs.push_back("\"" + s1gamecsgo + "\"");
        mapImportArgs.push_back("-src1contentdir");
        mapImportArgs.push_back("\"" + s1contentcsgo + "\"");
        mapImportArgs.push_back("-s2addon");
        mapImportArgs.push_back("\"" + s2addon + "\"");
        mapImportArgs.push_back("-game");
        mapImportArgs.push_back("csgo");
        mapImportArgs.push_back("\"maps\\" + mapname + ".vmf\"");

        runCommand("source1import", mapImportArgs);

        std::string prefabMapname = mapname;
        replaceAll(prefabMapname, "instances", "prefabs");

        if (!skipdeps) {
            std::string prefabRefsPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_refs.txt";
            stripMDLsFromRefs(prefabRefsPath);

            std::string prefabMdlLstPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_mdl_lst.txt";
            importAndCompileMapMDLs(prefabMdlLstPath);

            std::string prefabNewRefsPath = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_new_refs.txt";
            importAndCompileMapRefs(prefabNewRefsPath);

            runCommand("source1import", mapImportArgs);
        }

        std::string srcVmap = s2contentcsgoimported + "\\maps\\" + prefabMapname + ".vmap";
        std::string finalVmapPath = s2contentcsgo + "\\maps\\" + prefabMapname + ".vmap";

        fs::path destDir = fs::path(s2contentcsgo) / "maps";
        if (!fs::exists(destDir)) {
            fs::create_directories(destDir);
        }

        if (!fs::exists(finalVmapPath)) {
            emitLog("Copying " + srcVmap + " to " + s2contentcsgo + "\\maps\\");
            fs::copy_file(srcVmap, finalVmapPath, fs::copy_options::overwrite_existing);
        }

        emitLog("Map Import Finished.");
        emit finished();

    } catch (const std::exception& e) {
        emit error(QString::fromStdString(e.what()));
    }
}

void MapImporter::stripMDLsFromRefs(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::vector<std::string> mdls;
    std::vector<std::string> others;
    std::string line;

    while (std::getline(file, line)) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1); // trim
        if (line.empty()) continue;

        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        if (lowerLine.length() >= 4 && lowerLine.substr(lowerLine.length() - 4) == ".mdl") {
            mdls.push_back(line);
        } else {
            others.push_back(line);
        }
    }
    file.close();

    std::string mdlfilename = filename;
    replaceAll(mdlfilename, "_refs.txt", "_mdl_lst.txt");
    std::ofstream mdlFile(mdlfilename);
    if (mdlFile.is_open()) {
        for (const auto& mdl : mdls) {
            mdlFile << mdl << "\n";
        }
    }

    std::string refsfilename = filename;
    replaceAll(refsfilename, "_refs.txt", "_new_refs.txt");
    std::ofstream newRefsFile(refsfilename);
    if (newRefsFile.is_open()) {
        for (const auto& other : others) {
            newRefsFile << other << "\n";
        }
    }
}

void MapImporter::forceUV2ForVMAT(const std::string& mtlfile)
{
    std::string vmatfilename = s2contentcsgoimported + "\\" + mtlfile;
    replaceAll(vmatfilename, ".vmt", ".vmat");

    std::ifstream file(vmatfilename);
    if (!file.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    bool modified = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string txt = lines[i];
        txt.erase(0, txt.find_first_not_of(" \t"));
        txt.erase(txt.find_last_not_of(" \n\r\t") + 1);
        std::transform(txt.begin(), txt.end(), txt.begin(), ::tolower);

        if (txt.find("\"shader\"") == 0) {
            if (i + 1 < lines.size()) {
                std::string txtNext = lines[i + 1];
                replaceAll(txtNext, "\t", "");
                txtNext.erase(0, txtNext.find_first_not_of(" "));
                std::string lowerNext = txtNext;
                std::transform(lowerNext.begin(), lowerNext.end(), lowerNext.begin(), ::tolower);

                if (lowerNext.find("\"f_force_uv2\"") != 0) {
                    lines.insert(lines.begin() + i + 1, "\t\"F_FORCE_UV2\" \"1\"");
                    modified = true;
                    break;
                }
            }
        }
    }

    if (modified) {
        emitLog("Added F_FORCE_UV2 to " + vmatfilename);
        std::ofstream outFile(vmatfilename);
        if (outFile.is_open()) {
            for (const auto& l : lines) {
                outFile << l << "\n";
            }
        }
    }
}

bool MapImporter::force2UVsIfRequired(const std::string& refsName, std::set<std::string>& global2UVMaterials, const std::string& global2UVMaterialsFile)
{
    std::string meshinfofilename = refsName;
    replaceAll(meshinfofilename, "_refs.txt", "_refs\\mesh\\meshinfo.txt");
    replaceAll(meshinfofilename, "/", "\\");

    if (!fs::exists(meshinfofilename)) return false;

    std::ifstream meshFile(meshinfofilename);
    if (!meshFile.is_open()) return false;
    std::stringstream buffer;
    buffer << meshFile.rdbuf();
    std::string meshinfoStr = buffer.str();
    meshFile.close();

    bool is2UV = false;
    std::regex rx("'numuvs'\\s*:\\s*2");
    if (std::regex_search(meshinfoStr, rx)) {
        is2UV = true;
    }

    bool b2UV = false;
    std::ifstream refsFile(refsName);
    if (!refsFile.is_open()) return false;

    std::vector<std::string> refsList;
    std::string line;
    while (std::getline(refsFile, line)) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) {
            refsList.push_back(line);
        }
    }
    refsFile.close();

    std::set<std::string> uvsUpdated;

    for (const auto& mtlfile : refsList) {
        if (uvsUpdated.find(mtlfile) != uvsUpdated.end()) continue;

        if (global2UVMaterials.find(mtlfile) != global2UVMaterials.end()) {
            b2UV = true;
            uvsUpdated.insert(mtlfile);
        } else {
            if (is2UV) {
                b2UV = true;
                emitLog("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                uvsUpdated.insert(mtlfile);

                if (global2UVMaterials.find(mtlfile) == global2UVMaterials.end()) {
                    std::ofstream outList(global2UVMaterialsFile, std::ios_base::app);
                    outList << mtlfile << "\n";
                    global2UVMaterials.insert(mtlfile);
                }

                forceUV2ForVMAT(mtlfile);
            }
        }
    }

    return b2UV;
}

void MapImporter::importAndCompileMapMDLs(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        emitLog("No MDLs to import (file not found)");
        return;
    }

    std::vector<std::string> mdlfiles;
    std::string line;
    while (std::getline(file, line)) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) {
            mdlfiles.push_back(line);
        }
    }
    file.close();

    if (mdlfiles.empty()) {
        emitLog("No MDLs to import");
        return;
    }

    emitLog("Importing models");
    emitLog("--------------------------------");
    for (const auto& x : mdlfiles) {
        if (x.find("-") != 0) {
            emitLog(x);
        }
    }
    emitLog("--------------------------------");

    std::vector<std::string> force2UVListFiles;
    std::set<std::string> mdlmtls;
    std::string extraoptions = "";

    for (std::string mdlfile : mdlfiles) {
        if (mdlfile.find("-") == 0) {
            if (mdlfile == "-" || mdlfile == "-nooptions") {
                extraoptions = "";
            } else {
                extraoptions = mdlfile;
            }
        } else {
            replaceAll(mdlfile, "/", "\\");
            std::string infile = mdlfile;
            std::string outName = s2contentcsgoimported + "\\" + mdlfile;
            replaceAll(outName, ".mdl", ".vmdl");
            std::string refsName = s2contentcsgoimported + "\\" + mdlfile;
            replaceAll(refsName, ".mdl", "_refs.txt");

            std::vector<std::string> importArgs;
            importArgs.push_back("-nop4");
            if (!extraoptions.empty()) {
                importArgs.push_back(extraoptions);
            }
            importArgs.push_back("-i");
            importArgs.push_back("\"" + s1gamecsgo + "\"");
            importArgs.push_back("-o");
            importArgs.push_back("\"" + s2contentcsgoimported + "\"");
            importArgs.push_back("\"" + infile + "\"");
            runCommand("cs_mdl_import", importArgs);

            if (fs::exists(refsName)) {
                std::ifstream refFile(refsName);
                if (refFile.is_open()) {
                    std::string refLine;
                    while (std::getline(refFile, refLine)) {
                        refLine.erase(refLine.find_last_not_of(" \n\r\t") + 1);
                        if (!refLine.empty()) {
                            mdlmtls.insert(refLine);
                        }
                    }
                    refFile.close();
                }
                force2UVListFiles.push_back(refsName);
            }
        }
    }

    std::string temp_refs = filename;
    replaceAll(temp_refs, "mdl_lst", "mtl_lst");
    std::ofstream tempRefsFile(temp_refs);
    if (tempRefsFile.is_open()) {
        for (const auto& mtl : mdlmtls) {
            tempRefsFile << mtl << "\n";
        }
        tempRefsFile.close();
    }

    std::vector<std::string> importRefsArgs;
    importRefsArgs.push_back("-retail");
    importRefsArgs.push_back("-nop4");
    importRefsArgs.push_back("-nop4sync");
    importRefsArgs.push_back("-src1gameinfodir");
    importRefsArgs.push_back("\"" + s1gamecsgo + "\"");
    importRefsArgs.push_back("-s2addon");
    importRefsArgs.push_back("\"" + s2addon + "\"");
    importRefsArgs.push_back("-game");
    importRefsArgs.push_back("csgo");
    importRefsArgs.push_back("-usefilelist");
    importRefsArgs.push_back("\"" + temp_refs + "\"");
    runCommand("source1import", importRefsArgs);

    std::set<std::string> global2UVMaterials;
    std::string listFileName = "source1import_2uvmateriallist.txt";
    std::ifstream listFileIn(listFileName);
    if (listFileIn.is_open()) {
        std::string mtl;
        while (std::getline(listFileIn, mtl)) {
            mtl.erase(mtl.find_last_not_of(" \n\r\t") + 1);
            if (!mtl.empty()) {
                global2UVMaterials.insert(mtl);
                forceUV2ForVMAT(mtl);
            }
        }
        listFileIn.close();
    }

    // compile materials
    for (std::string mtlfile : mdlmtls) {
        if (mtlfile.find("-") == 0 || mtlfile.empty()) continue;
        replaceAll(mtlfile, "/", "\\");
        std::string outName = s2contentcsgoimported + "\\" + mtlfile;
        replaceAll(outName, ".vmt", ".vmat");

        std::vector<std::string> resCompArgs;
        resCompArgs.push_back("-retail");
        resCompArgs.push_back("-nop4");
        resCompArgs.push_back("-game");
        resCompArgs.push_back("csgo");
        resCompArgs.push_back("\"" + outName + "\"");
        runCommand("resourcecompiler", resCompArgs);
    }

    // compile models
    for (std::string mdlfile : mdlfiles) {
        if (mdlfile.find("-") == 0) continue;
        replaceAll(mdlfile, "/", "\\");
        std::string outName = s2contentcsgoimported + "\\" + mdlfile;
        replaceAll(outName, ".mdl", ".vmdl");

        if (!fs::exists(outName)) continue;

        std::string refsName = s2contentcsgoimported + "\\" + mdlfile;
        replaceAll(refsName, ".mdl", "_refs.txt");

        bool bForceCompile = force2UVsIfRequired(refsName, global2UVMaterials, listFileName);

        std::vector<std::string> resCompArgs;
        resCompArgs.push_back("-retail");
        resCompArgs.push_back("-nop4");
        if (bForceCompile) {
            resCompArgs.push_back("-f");
        }
        resCompArgs.push_back("-game");
        resCompArgs.push_back("csgo");
        resCompArgs.push_back("\"" + outName + "\"");
        runCommand("resourcecompiler", resCompArgs);
    }
}

void MapImporter::importAndCompileMapRefs(const std::string& refsFile)
{
    std::vector<std::string> importCmdArgs;
    importCmdArgs.push_back("-retail");
    importCmdArgs.push_back("-nop4");
    importCmdArgs.push_back("-nop4sync");
    importCmdArgs.push_back("-src1gameinfodir");
    importCmdArgs.push_back("\"" + s1gamecsgo + "\"");
    importCmdArgs.push_back("-s2addon");
    importCmdArgs.push_back("\"" + s2addon + "\"");
    importCmdArgs.push_back("-game");
    importCmdArgs.push_back("csgo");
    importCmdArgs.push_back("-usefilelist");
    importCmdArgs.push_back("\"" + refsFile + "\"");
    runCommand("source1import", importCmdArgs);

    std::ifstream file(refsFile);
    if (!file.is_open()) return;

    std::string newList = "";
    std::string line;
    while (std::getline(file, line)) {
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (!line.empty()) {
            replaceAll(line, ".vmt", ".vmat");
            replaceAll(line, " ", "_");
            replaceAll(line, "/", "\\");
            newList += s2contentcsgoimported + "\\" + line + "\n";
        }
    }
    file.close();

    std::string prefabMapname = mapname;
    replaceAll(prefabMapname, "instances", "prefabs");

    std::string tmpFile = s2contentcsgoimported + "\\maps\\" + prefabMapname + "_prefab_compile_new_refs.txt";
    std::ofstream tempFile(tmpFile);
    if (tempFile.is_open()) {
        tempFile << newList;
        tempFile.close();
    }

    std::vector<std::string> compArgs;
    compArgs.push_back("-retail");
    compArgs.push_back("-nop4");
    compArgs.push_back("-game");
    compArgs.push_back("csgo");
    compArgs.push_back("-f");
    compArgs.push_back("-filelist");
    compArgs.push_back("\"" + tmpFile + "\"");
    runCommand("resourcecompiler", compArgs);
}
