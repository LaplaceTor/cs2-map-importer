#ifndef MAPIMPORTER_H
#define MAPIMPORTER_H

#include <QString>
#include <QStringList>
#include <QSet>
#include <functional>

class MapImporter {
public:
    MapImporter() {}

    bool Run();

private:
    QStringList GetRefsList();
    void ImportAndCompileMapMDLs(const QString& filename);
    void ImportAndCompileMapMDLsMulti(const QString& filename);
    void ImportAndCompileMapRefs(const QStringList& missingMaterials);
    void ImportParticles();
    void ImportSounds();

    friend class SoundscapeImport;
};

#endif // MAPIMPORTER_H
