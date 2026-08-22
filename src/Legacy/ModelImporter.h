#ifndef MODELIMPORTER_H
#define MODELIMPORTER_H

#include <QString>
#include <QStringList>
#include <QSet>

class ModelImporter {
public:
    ModelImporter() {}

    bool Run(const QString& mdlPath);

private:
    void FixModelMaterials(const QStringList& vmatFiles);
};

#endif // MODELIMPORTER_H
