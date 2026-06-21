#ifndef SOUNDSCAPEIMPORT_H
#define SOUNDSCAPEIMPORT_H

#include "MapImporter.h"
#include <QString>
#include <QSet>

class SoundscapeImport {
public:
    static void ImportSoundscapes(MapImporter* importer, const MapImporter::Options& options, QSet<QString>& uniqueSounds);
};

#endif // SOUNDSCAPEIMPORT_H
