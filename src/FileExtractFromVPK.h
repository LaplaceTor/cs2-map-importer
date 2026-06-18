#ifndef FILEEXTRACTFROMVPK_H
#define FILEEXTRACTFROMVPK_H

#include <QString>
#include "mapimporter.h"

class FileExtractFromVPK {
public:
    static void ExtractModel(const QString& filepath, const MapImporter::Options& options);
    static void ExtractParticle(const QString& filepath, const MapImporter::Options& options);
    static void ExtractSound(const QString& filepath, const MapImporter::Options& options);
};

#endif // FILEEXTRACTFROMVPK_H
