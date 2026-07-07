#ifndef FILEEXTRACTFROMVPK_H
#define FILEEXTRACTFROMVPK_H

#include <QString>
class FileExtractFromVPK {
public:
    static void ExtractModel(const QString& filepath);
    static void ExtractMaterial(const QString& filepath);
    static void ExtractParticle(const QString& filepath);
    static void ExtractSound(const QString& filepath);
};

#endif // FILEEXTRACTFROMVPK_H
