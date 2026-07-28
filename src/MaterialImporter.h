#ifndef MATERIALIMPORTER_H
#define MATERIALIMPORTER_H

#include <QString>

class MaterialImporter {
public:
    MaterialImporter() {}

    static bool ProcessImage(const QString& imagePath, const QString& appDir, QString& outPreviewPath);
};

#endif // MATERIALIMPORTER_H
