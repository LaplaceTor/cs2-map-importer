#include "MaterialImporter.h"
#include "Miscellaneous.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>

QStringList MaterialImporter::ReadTextFile(const QString& filepath) {
    QStringList lines;
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            lines.append(in.readLine().trimmed());
        }
        file.close();
    }
    return lines;
}

void MaterialImporter::EnsureFileWritable(const QString& filepath) {
    QFile file(filepath);
    if (file.exists()) {
        file.setPermissions(file.permissions() | QFileDevice::WriteUser);
    } else {
        QFileInfo fi(filepath);
        QDir dir = fi.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    }
}

static QString CleanRefPath(QString input) {
    int filePos = input.indexOf("\"file\"");
    if (filePos != -1) {
        input = input.mid(filePos + 6);
    }

    QRegularExpression reLeading("^\\s*\"");
    QRegularExpressionMatch matchLeading = reLeading.match(input);
    if (matchLeading.hasMatch()) {
        input = input.mid(matchLeading.capturedLength());
    } else {
        QRegularExpression reLeading2("^\\s*");
        QRegularExpressionMatch matchLeading2 = reLeading2.match(input);
        if (matchLeading2.hasMatch()) {
            input = input.mid(matchLeading2.capturedLength());
        }
    }

    QRegularExpression reTrailing("\"\\s*$");
    QRegularExpressionMatch matchTrailing = reTrailing.match(input);
    if (matchTrailing.hasMatch()) {
        input = input.mid(0, input.length() - matchTrailing.capturedLength());
    }
    return input.trimmed();
}

bool MaterialImporter::Run(const QString& refsFile) {
    QString importcmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon " + Miscellaneous::GetOptions().addonName + " -game csgo -usefilelist \"" + refsFile + "\"";
    Miscellaneous::RunCommandSync(importcmd);

    QStringList refs = ReadTextFile(refsFile);
    QString newList = "";

    for (const QString& line : refs) {
        if (Miscellaneous::CanceLImport) return false;
        QString cleanedRef = CleanRefPath(line);
        if (!cleanedRef.isEmpty()) {
            QString modLine = cleanedRef;
            int pos = modLine.lastIndexOf(".vmt");
            if (pos != -1) modLine.replace(pos, 4, ".vmat");
            modLine.replace(' ', '_');
            modLine.replace('/', '\\');
            newList += Miscellaneous::GetOptions().s2contentdir + "\\" + modLine + "\n";
        }
    }

    QString tmpFile = Miscellaneous::GetOptions().s2contentdir + "\\compile_new_refs.txt";
    EnsureFileWritable(tmpFile);
    QFile writeFile(tmpFile);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&writeFile);
        out << newList;
        writeFile.close();
    }

    QString compilercmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -f -filelist \"" + tmpFile + "\"";
    Miscellaneous::RunCommandSync(compilercmd);

    return true;
}
