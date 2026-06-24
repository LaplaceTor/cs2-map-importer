#ifndef SOUNDSCAPEIMPORT_H
#define SOUNDSCAPEIMPORT_H

#include "MapImporter.h"
#include <QString>
#include <QSet>

class SoundscapeImport {
public:
    static void ImportSoundscapes(MapImporter* importer, QSet<QString>& uniqueSounds);
};

#endif // SOUNDSCAPEIMPORT_H
