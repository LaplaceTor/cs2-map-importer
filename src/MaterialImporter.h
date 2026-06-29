#ifndef MATERIALIMPORTER_H
#define MATERIALIMPORTER_H

#include <QString>
#include <QStringList>

class MaterialImporter {
public:
    MaterialImporter() {}

    bool Run(const QString& refsFile);

private:
    QStringList ReadTextFile(const QString& filepath);
    void EnsureFileWritable(const QString& filepath);
};

#endif // MATERIALIMPORTER_H
