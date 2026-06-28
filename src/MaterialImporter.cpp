#include "MaterialImporter.h"
#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QDirIterator>

MaterialImporter::MaterialImporter(const QString& materialFolder, const QString& materialFileListText, const QString& addonName)
    : m_materialFolder(materialFolder),
      m_materialFileListText(materialFileListText),
      m_addonName(addonName)
{
}

void MaterialImporter::CopyDirectoryRecursively(const QString& sourceDir, const QString& destDir) {
    QDir dir(sourceDir);
    if (!dir.exists()) return;

    QDir().mkpath(destDir);

    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& info : list) {
        if (Miscellaneous::CanceLImport) return;

        QString destPath = destDir + QDir::separator() + info.fileName();
        if (info.isDir()) {
            CopyDirectoryRecursively(info.absoluteFilePath(), destPath);
        } else if (info.isFile()) {
            if (QFile::exists(destPath)) {
                QFile::setPermissions(destPath, QFile::WriteOwner | QFile::WriteUser);
                QFile::remove(destPath);
            }
            QFile::copy(info.absoluteFilePath(), destPath);
        }
    }
}

bool MaterialImporter::Run() {
    Miscellaneous::Log("Starting Material Import process.");

    if (Miscellaneous::CanceLImport) return false;

    QString s1GameDir = Miscellaneous::GetOptions().s1gamedir;
    if (s1GameDir.isEmpty()) {
        Miscellaneous::Log("Error: S1 Game Dir is empty.");
        return false;
    }

    QString destMaterialsDir = QDir(s1GameDir).filePath("materials");

    QStringList vmtFiles;

    // 1. Process the selected folder
    if (!m_materialFolder.isEmpty()) {
        QDir matDir(m_materialFolder);
        if (matDir.exists()) {
            Miscellaneous::Log("Copying material folder to S1 game directory...");
            QString targetDir = destMaterialsDir;
            if (matDir.dirName().toLower() != "materials") {
                targetDir = QDir(destMaterialsDir).filePath(matDir.dirName());
            }
            CopyDirectoryRecursively(m_materialFolder, targetDir);

            // Find all .vmt files in the target directory (after copy, or we can find in original)
            // It's easier to find in original and then create the relative paths
            QDirIterator it(m_materialFolder, QStringList() << "*.vmt", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString filePath = it.next();
                QString relativePath;
                if (matDir.dirName().toLower() == "materials") {
                    relativePath = "materials/" + matDir.relativeFilePath(filePath);
                } else {
                    relativePath = "materials/" + matDir.dirName() + "/" + matDir.relativeFilePath(filePath);
                }
                vmtFiles.append(relativePath);
            }
        }
    }

    // 2. Process the manual text list
    if (!m_materialFileListText.trimmed().isEmpty()) {
        QStringList lines = m_materialFileListText.split('\n');
        for (QString line : lines) {
            line = line.trimmed();
            if (line.isEmpty()) continue;
            // Clean up if the user copy-pasted with quotes
            if (line.startsWith("\"") && line.endsWith("\"")) {
                line = line.mid(1, line.length() - 2);
            }
            // Ensure forward slashes
            line.replace("\\", "/");
            // If it doesn't start with materials/, you could optionally prefix it, but we assume they type it correctly as per placeholder
            vmtFiles.append(line);
        }
    }

    if (vmtFiles.isEmpty()) {
        Miscellaneous::Log("No .vmt files found or provided.");
        return false;
    }

    // 3. Save the file list to appdir/filelist/time_importfilelist.txt
    QString appDir = Miscellaneous::GetOptions().appDir;
    QString fileListDir = QDir(appDir).filePath("filelist");
    QDir().mkpath(fileListDir);

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString fileListPath = QDir(fileListDir).filePath(timestamp + "_importfilelist.txt");

    QFile listFile(fileListPath);
    if (!listFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Miscellaneous::Log("Failed to create file list at " + fileListPath);
        return false;
    }

    QTextStream out(&listFile);
    out << "importfilelist\n{\n";
    for (const QString& vmt : vmtFiles) {
        out << "\t\"file\"\t\"" << vmt << "\"\n";
    }
    out << "}\n";
    listFile.close();

    Miscellaneous::Log("Created import file list: " + fileListPath);

    // 4. Run source1import on the generated file list
    if (Miscellaneous::CanceLImport) return false;

    QString s2AddonDir = Miscellaneous::GetOptions().cs2Basefolder + "\\content\\csgo_addons\\" + m_addonName;

    QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + s1GameDir + "\" -s2addon " + m_addonName + " -game csgo -usefilelist \"" + fileListPath + "\"";
    Miscellaneous::RunCommandSync(importCmd);

    // 5. Compile the imported materials (.vmat) using resourcecompiler.exe
    QString newList = "";
    for (const QString& line : vmtFiles) {
        if (Miscellaneous::CanceLImport) return false;
        QString modLine = line;
        int pos = modLine.lastIndexOf(".vmt");
        if (pos != -1) modLine.replace(pos, 4, ".vmat");
        modLine.replace(' ', '_');
        modLine.replace('/', '\\');
        newList += s2AddonDir + "\\" + modLine + "\n";
    }

    QString tmpFile = s2AddonDir + "\\materials_compile_new_refs.txt";
    QFile writeFile(tmpFile);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out2(&writeFile);
        out2 << newList;
        writeFile.close();
    }

    if (Miscellaneous::CanceLImport) return false;
    QString compilerCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -f -filelist \"" + tmpFile + "\"";
    Miscellaneous::RunCommandSync(compilerCmd);

    Miscellaneous::Log("Material Import process complete.");
    return true;
}
