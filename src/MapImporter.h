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
        bool usebspNomergeinstances;
        bool skipdeps;

        QString cs2Basefolder; // to get the binaries
            };

    MapImporter(const Options& options)
        : mOptions(options) {}

    bool Run();

private:
    Options mOptions;


    QStringList ReadTextFile(const QString& filepath);
    void EnsureFileWritable(const QString& filepath);

    void StripMDLsFromRefs(const QString& filename);
    bool Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials);
    void ImportAndCompileMapMDLs(const QString& filename);
    void ImportAndCompileMapRefs(const QString& refsFile);
    void ImportParticles();
    void ImportSounds();

    friend class SoundscapeImport;
};

#endif // MAPIMPORTER_H
