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

    MapImporter(const Options& options)
        : m_options(options) {}

    bool Run();

private:
    Options m_options;


    QStringList ReadTextFile(const QString& filepath);
    void EnsureFileWritable(const QString& filepath);

    void StripMDLsFromRefs(const QString& filename);
    void ExtractModelFromVPK(const QString& filepath);
    void ExtractParticleFromVPK(const QString& filepath);
    void ExtractSoundFromVPK(const QString& filepath);
    void ForceUV2ForVMAT(const QString& mtlfile);
    bool Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials, QString& global2UVMaterialsFilepath);
    void ImportAndCompileMapMDLs(const QString& filename);
    void ImportAndCompileMapRefs(const QString& refsFile);
    void ImportParticles();
    void ImportSounds(const QString& target_mapname);
};

#endif // MAPIMPORTER_H
