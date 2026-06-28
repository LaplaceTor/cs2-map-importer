#ifndef MATERIALIMPORTER_H
#define MATERIALIMPORTER_H

#include <QString>
#include <QStringList>

class MaterialImporter {
public:
    MaterialImporter(const QString& materialFolder, const QString& materialFileListText, const QString& addonName);
    bool Run();

private:
    QString m_materialFolder;
    QString m_materialFileListText;
    QString m_addonName;

    void CopyDirectoryRecursively(const QString& sourceDir, const QString& destDir);
};

#endif // MATERIALIMPORTER_H
