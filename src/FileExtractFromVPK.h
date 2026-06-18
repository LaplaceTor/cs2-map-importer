#ifndef FILEEXTRACTFROMVPK_H
#define FILEEXTRACTFROMVPK_H

#include <QString>
#include "mapimporter.h"

class FileExtractFromVPK {
public:
    static void ExtractModelFromVPK(const QString& filepath, const MapImporter::Options& options);
    static void ExtractParticleFromVPK(const QString& filepath, const MapImporter::Options& options);
    static void ExtractSoundFromVPK(const QString& filepath, const MapImporter::Options& options);
};

#endif // FILEEXTRACTFROMVPK_H
