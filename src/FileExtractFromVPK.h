#ifndef FILEEXTRACTFROMVPK_H
#define FILEEXTRACTFROMVPK_H

#include <QString>
#include "MapImporter.h"

class FileExtractFromVPK {
public:
    static void ExtractModel(const QString& filepath);
    static void ExtractParticle(const QString& filepath);
    static void ExtractSound(const QString& filepath);
};

#endif // FILEEXTRACTFROMVPK_H
