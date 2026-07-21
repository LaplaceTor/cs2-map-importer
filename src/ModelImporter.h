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
    QStringList ReadTextFile(const QString& filepath);
    void EnsureFileWritable(const QString& filepath);
};

#endif // MODELIMPORTER_H
