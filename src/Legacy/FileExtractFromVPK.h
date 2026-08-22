#ifndef FILEEXTRACTFROMVPK_H
#define FILEEXTRACTFROMVPK_H

#include <QString>
class FileExtractFromVPK {
public:
    static bool ExtractModel(const QString& filepath);
    static bool ExtractMaterial(const QString& filepath);
    static bool ExtractParticle(const QString& filepath);
    static bool ExtractSound(const QString& filepath);
};

#endif // FILEEXTRACTFROMVPK_H
