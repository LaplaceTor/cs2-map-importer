#include "mapimporter.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <array>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace fs = std::filesystem;

void MapImporter::Log(const std::string& msg) {
    if (m_log) m_log(msg);
}

int MapImporter::RunCommand(const std::string& cmd) {
    Log(cmd);

    std::string fullCmd = "cmd.exe /c \"";
    // Setup environment path to include cs2 bin dir
    std::string binPath = m_options.cs2_basefolder + "\\game\\bin\\win64";
    fullCmd += "set PATH=" + binPath + ";%PATH% && " + cmd + "\"";

    std::unique_ptr<FILE, int(*)(FILE*)> pipe(POPEN(fullCmd.c_str(), "r"), PCLOSE);
    if (!pipe) {
        Log("Failed to run command.");
        return -1;
    }

    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::string output = buffer.data();
        if (!output.empty() && output.back() == '\n') output.pop_back();
        if (!output.empty() && output.back() == '\r') output.pop_back();
        if (!output.empty()) {
             Log(output);
        }
    }

    return 0; // simplified
}

std::vector<std::string> MapImporter::ReadTextFile(const std::string& filepath) {
    std::vector<std::string> lines;
    std::ifstream file(filepath);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }
    return lines;
}

void MapImporter::EnsureFileWritable(const std::string& filepath) {
    fs::path p(filepath);
    if (fs::exists(p)) {
        fs::permissions(p, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::add);
    } else {
        if (p.has_parent_path() && !fs::exists(p.parent_path())) {
            fs::create_directories(p.parent_path());
        }
    }
}

void MapImporter::StripMDLsFromRefs(const std::string& filename) {
    auto refs = ReadTextFile(filename);
    std::vector<std::string> mdls;
    std::vector<std::string> others;

    for (const auto& ref : refs) {
        if (ref.empty()) continue;
        std::string lowerRef = ref;
        std::transform(lowerRef.begin(), lowerRef.end(), lowerRef.begin(), ::tolower);
        if (lowerRef.find(".mdl") != std::string::npos) {
            mdls.push_back(ref);
        } else {
            others.push_back(ref);
        }
    }

    std::string mdlfilename = filename;
    size_t pos = mdlfilename.rfind("_refs.txt");
    if (pos != std::string::npos) mdlfilename.replace(pos, 9, "_mdl_lst.txt");

    EnsureFileWritable(mdlfilename);
    std::ofstream mdlFile(mdlfilename);
    for (const auto& m : mdls) mdlFile << m << "\n";

    std::string refsfilename = filename;
    pos = refsfilename.rfind("_refs.txt");
    if (pos != std::string::npos) refsfilename.replace(pos, 9, "_new_refs.txt");

    EnsureFileWritable(refsfilename);
    std::ofstream refFile(refsfilename);
    for (const auto& o : others) refFile << o << "\n";
}

void MapImporter::ForceUV2ForVMAT(const std::string& mtlfile) {
    std::string vmat = mtlfile;
    size_t pos = vmat.rfind(".vmt");
    if (pos != std::string::npos) vmat.replace(pos, 4, ".vmat");

    std::string vmatfilename = m_options.s2contentdir + "\\" + vmat;
    if (!fs::exists(vmatfilename)) return;

    auto lines = ReadTextFile(vmatfilename);
    EnsureFileWritable(vmatfilename);

    bool added = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string txt = lines[i];
        std::string lowerTxt = txt;
        std::transform(lowerTxt.begin(), lowerTxt.end(), lowerTxt.begin(), ::tolower);

        size_t start = lowerTxt.find_first_not_of(" \t");
        if (start != std::string::npos && lowerTxt.substr(start).find("\"shader\"") == 0) {
            if (i + 1 < lines.size()) {
                std::string txtNext = lines[i+1];
                std::string lowerNext = txtNext;
                std::transform(lowerNext.begin(), lowerNext.end(), lowerNext.begin(), ::tolower);

                size_t startNext = lowerNext.find_first_not_of(" \t");
                if (startNext == std::string::npos || lowerNext.substr(startNext).find("\"f_force_uv2\"") != 0) {
                    lines.insert(lines.begin() + i + 1, "\t\"F_FORCE_UV2\" \"1\"");
                    added = true;
                    break;
                }
            }
        }
    }

    if (added) {
        Log("Added F_FORCE_UV2 to " + vmatfilename);
        std::ofstream file(vmatfilename);
        for (const auto& l : lines) file << l << "\n";
    }
}

bool MapImporter::Force2UVsIfRequired(const std::string& refsName, std::set<std::string>& global2UVMaterials, std::string& global2UVMaterialsFilepath) {
    std::set<std::string> uvsUpdated;
    std::string meshinfofilename = refsName;
    size_t pos = meshinfofilename.rfind("_refs.txt");
    if (pos != std::string::npos) meshinfofilename.replace(pos, 9, "_refs/mesh/meshinfo.txt");

    std::replace(meshinfofilename.begin(), meshinfofilename.end(), '/', '\\');

    if (!fs::exists(meshinfofilename)) return false;

    auto meshinfo = ReadTextFile(meshinfofilename);
    std::string meshstring;
    for (const auto& l : meshinfo) meshstring += l;

    bool b2UV = false;
    if (!fs::exists(refsName)) return false;

    auto refsList = ReadTextFile(refsName);
    int numuvs = 1; // Simplistic parsing
    if (meshstring.find("'numuvs': 2") != std::string::npos || meshstring.find("\"numuvs\": 2") != std::string::npos) {
        numuvs = 2;
    }

    for (const auto& mtlfile : refsList) {
        if (mtlfile.empty()) continue;
        if (uvsUpdated.count(mtlfile)) continue;

        if (global2UVMaterials.count(mtlfile)) {
            b2UV = true;
            uvsUpdated.insert(mtlfile);
        } else {
            if (numuvs == 2) {
                b2UV = true;
                Log("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                uvsUpdated.insert(mtlfile);

                global2UVMaterials.insert(mtlfile);
                std::ofstream ofs(global2UVMaterialsFilepath, std::ios::app);
                ofs << mtlfile << "\n";

                ForceUV2ForVMAT(mtlfile);
            }
        }
    }
    return b2UV;
}

void MapImporter::ImportAndCompileMapMDLs(const std::string& filename) {
    auto mdlfiles = ReadTextFile(filename);
    if (mdlfiles.empty()) {
        Log("No MDLs to import");
        return;
    }

    Log("Importing models");
    Log("--------------------------------");
    for (const auto& x : mdlfiles) {
        if (x.empty() || x[0] == '-') continue;
        Log(x);
    }
    Log("--------------------------------");

    std::vector<std::string> force2UVList;
    std::set<std::string> mdlmtls;
    std::string extraoptions = "";

    for (const auto& m : mdlfiles) {
        if (m.empty()) continue;
        if (m[0] == '-') {
            if (m == "-" || m == "-nooptions") extraoptions = "";
            else extraoptions = m;
        } else {
            std::string mdlfile = m;
            std::replace(mdlfile.begin(), mdlfile.end(), '/', '\\');

            std::string infile = mdlfile;
            std::string outName = m_options.s2contentdir + "\\" + mdlfile;
            size_t pos = outName.rfind(".mdl");
            if (pos != std::string::npos) outName.replace(pos, 4, ".vmdl");

            std::string refsName = m_options.s2contentdir + "\\" + mdlfile;
            pos = refsName.rfind(".mdl");
            if (pos != std::string::npos) refsName.replace(pos, 4, "_refs.txt");

            std::string importCmd = "cs_mdl_import.exe -nop4 " + extraoptions + " -i \"" + m_options.s1gamedir + "\" -o \"" + m_options.s2contentdir + "\" \"" + infile + "\"";
            RunCommand(importCmd);

            if (fs::exists(refsName)) {
                auto refs = ReadTextFile(refsName);
                for (const auto& ref : refs) {
                    if (!ref.empty()) mdlmtls.insert(ref);
                }
                force2UVList.push_back(refsName);
            }
        }
    }

    std::string temp_refs = filename;
    size_t pos = temp_refs.rfind("mdl_lst");
    if (pos != std::string::npos) temp_refs.replace(pos, 7, "mtl_lst");

    EnsureFileWritable(temp_refs);
    std::ofstream fw(temp_refs);
    for (const auto& mtl : mdlmtls) fw << mtl << "\n";
    fw.close();

    std::string importRefsCmd = "source1import.exe -retail -nop4 -nop4sync -src1gameinfodir \"" + m_options.s1gamedir + "\" -s2addon " + m_options.s2addonname + " -game " + m_options.s1gamename + " -usefilelist \"" + temp_refs + "\"";
    RunCommand(importRefsCmd);

    std::set<std::string> global2UVMaterials;
    std::string global2UVMaterialFilepath = "source1import_2uvmateriallist.txt";
    if (fs::exists(global2UVMaterialFilepath)) {
        auto force2UVListFile = ReadTextFile(global2UVMaterialFilepath);
        for (const auto& mtl : force2UVListFile) {
            global2UVMaterials.insert(mtl);
            ForceUV2ForVMAT(mtl);
        }
    }
    EnsureFileWritable(global2UVMaterialFilepath);

    for (const auto& mtlfile : mdlmtls) {
        if (mtlfile.empty() || mtlfile[0] == '-') continue;
        std::string mtl = mtlfile;
        std::replace(mtl.begin(), mtl.end(), '/', '\\');
        std::string outName = m_options.s2contentdir + "\\" + mtl;
        pos = outName.rfind(".vmt");
        if (pos != std::string::npos) outName.replace(pos, 4, ".vmat");

        std::string resCompCmd = "resourcecompiler.exe -retail -nop4 -game " + m_options.s1gamename + " \"" + outName + "\"";
        RunCommand(resCompCmd);
    }

    for (const auto& m : mdlfiles) {
        if (m.empty() || m[0] == '-') continue;
        std::string mdlfile = m;
        std::replace(mdlfile.begin(), mdlfile.end(), '/', '\\');

        std::string outName = m_options.s2contentdir + "\\" + mdlfile;
        pos = outName.rfind(".mdl");
        if (pos != std::string::npos) outName.replace(pos, 4, ".vmdl");

        if (!fs::exists(outName)) continue;

        std::string refsName = m_options.s2contentdir + "\\" + mdlfile;
        pos = refsName.rfind(".mdl");
        if (pos != std::string::npos) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = Force2UVsIfRequired(refsName, global2UVMaterials, global2UVMaterialFilepath);

        std::string resCompCmd;
        if (bForceCompile) {
            resCompCmd = "resourcecompiler.exe -retail -nop4 -f -game " + m_options.s1gamename + " \"" + outName + "\"";
        } else {
            resCompCmd = "resourcecompiler.exe -retail -nop4 -game " + m_options.s1gamename + " \"" + outName + "\"";
        }
        RunCommand(resCompCmd);
    }
}

void MapImporter::ImportAndCompileMapRefs(const std::string& refsFile) {
    std::string importcmd = "source1import.exe -retail -nop4 -nop4sync -src1gameinfodir \"" + m_options.s1gamedir + "\" -s2addon " + m_options.s2addonname + " -game " + m_options.s1gamename + " -usefilelist \"" + refsFile + "\"";
    RunCommand(importcmd);

    auto refs = ReadTextFile(refsFile);
    std::string newList = "";

    for (const auto& line : refs) {
        if (!line.empty()) {
            std::string modLine = line;
            size_t pos = modLine.rfind(".vmt");
            if (pos != std::string::npos) modLine.replace(pos, 4, ".vmat");
            std::replace(modLine.begin(), modLine.end(), ' ', '_');
            std::replace(modLine.begin(), modLine.end(), '/', '\\');
            newList += m_options.s2contentdir + "\\" + modLine + "\n";
        }
    }

    std::string tmpFile = m_options.s2contentdir + "\\maps\\" + m_options.mapname + "_compile_new_refs.txt";
    EnsureFileWritable(tmpFile);
    std::ofstream writeFile(tmpFile);
    writeFile << newList;
    writeFile.close();

    std::string compilercmd = "resourcecompiler.exe -retail -nop4 -game " + m_options.s1gamename + " -f -filelist \"" + tmpFile + "\"";
    RunCommand(compilercmd);
}

bool MapImporter::Run() {
    Log("Starting Map Import process via C++.");

    std::string usebspStr = m_options.usebsp ? "-usebsp" : "";
    std::string nomergeinstancesStr = m_options.usebsp_nomergeinstances ? "-usebsp_nomergeinstances" : "";

    std::string mapImportCmd = "source1import.exe -retail -nop4 -nop4sync " + usebspStr;
    if (!nomergeinstancesStr.empty()) mapImportCmd += " " + nomergeinstancesStr;
    mapImportCmd += " -src1gameinfodir \"" + m_options.s1gamedir + "\" -src1contentdir \"" + m_options.s1contentdir + "\" -s2addon \"" + m_options.s2addonname + "\" -game " + m_options.s1gamename + " maps\\" + m_options.mapname + ".vmf";

    RunCommand(mapImportCmd);

    std::string m_mapname = m_options.mapname;
    size_t pos = m_mapname.find("instances");
    if (pos != std::string::npos) {
        m_mapname.replace(pos, 9, "prefabs");
    }

    if (!m_options.skipdeps) {
        StripMDLsFromRefs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_refs.txt");
        ImportAndCompileMapMDLs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_mdl_lst.txt");
        ImportAndCompileMapRefs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_new_refs.txt");

        RunCommand(mapImportCmd);
    }

    Log("Import process complete.");
    return true;
}
