#ifndef MAPIMPORTER_H
#define MAPIMPORTER_H

#include <QString>
#include <QStringList>
#include <QSet>
#include <functional>

class MapImporter {
public:
    struct Options {
        QString s1gamedir;
        QString csgogamedir;
        QString s1gamename;
        QString s1contentdir;
        QString s2addonname;
        QString s2contentdir;
        QString mapname;
        bool usebsp;
        bool usebsp_nomergeinstances;
        bool skipdeps;

        QString cs2_basefolder; // to get the binaries
    };

    MapImporter(const Options& options, std::function<void(const QString&)> logCallback)
        : m_options(options), m_log(logCallback) {}

    bool Run();

private:
    Options m_options;
    std::function<void(const QString&)> m_log;

    void Log(const QString& msg);

    QStringList ReadTextFile(const QString& filepath);
    void EnsureFileWritable(const QString& filepath);

    void StripMDLsFromRefs(const QString& filename);
    void ForceUV2ForVMAT(const QString& mtlfile);
    bool Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials, QString& global2UVMaterialsFilepath);
    void ImportAndCompileMapMDLs(const QString& filename);
    void ImportAndCompileMapRefs(const QString& refsFile);
};

#endif // MAPIMPORTER_H
