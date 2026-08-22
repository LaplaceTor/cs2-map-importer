#ifndef SOUNDSCAPEIMPORT_H
#define SOUNDSCAPEIMPORT_H

#include <QString>
#include <QSet>

class SoundscapeImport {
public:
    static void ImportSoundscapes(QSet<QString>& uniqueSounds);
    static void ProcessVmfConnections(const QString& vmfPath, QSet<QString>& uniqueSounds);
};

#endif // SOUNDSCAPEIMPORT_H
