#ifndef MAPIMPORTER_H
#define MAPIMPORTER_H

#include <string>
#include <vector>
#include <functional>
#include <set>

class MapImporter {
public:
    struct Options {
        std::string s1gamedir;
        std::string s1gamename;
        std::string s1contentdir;
        std::string s2addonname;
        std::string s2contentdir;
        std::string mapname;
        bool usebsp;
        bool usebsp_nomergeinstances;
        bool skipdeps;

        std::string cs2_basefolder; // to get the binaries
    };

    MapImporter(const Options& options, std::function<void(const std::string&)> logCallback)
        : m_options(options), m_log(logCallback) {}

    bool Run();

private:
    Options m_options;
    std::function<void(const std::string&)> m_log;

    void Log(const std::string& msg);
    int RunCommand(const std::string& cmd);

    std::vector<std::string> ReadTextFile(const std::string& filepath);
    void EnsureFileWritable(const std::string& filepath);

    void StripMDLsFromRefs(const std::string& filename);
    void ForceUV2ForVMAT(const std::string& mtlfile);
    bool Force2UVsIfRequired(const std::string& refsName, std::set<std::string>& global2UVMaterials, std::string& global2UVMaterialsFilepath);
    void ImportAndCompileMapMDLs(const std::string& filename);
    void ImportAndCompileMapRefs(const std::string& refsFile);
};

#endif // MAPIMPORTER_H
