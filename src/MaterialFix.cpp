#include "MaterialFix.h"
#include "Miscellaneous.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>
#include <QDirIterator>
#include <QProcess>

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
        int start = input.indexOf(QRegularExpression("[^ \\t]"));
        if (start != -1) {
            input = input.mid(start);
        } else {
            return "";
        }
    }

    QRegularExpression reTrailing("\"\\s*$");
    QRegularExpressionMatch matchTrailing = reTrailing.match(input);
    if (matchTrailing.hasMatch()) {
        input = input.left(input.length() - matchTrailing.capturedLength());
    } else {
        int end = input.lastIndexOf(QRegularExpression("[^ \\t]"));
        if (end != -1) {
            input = input.left(end + 1);
        }
    }

    if (input == "importfilelist" || input == "{" || input == "}") return "";
    return input;
}

static QStringList ReadTextFile(const QString& filepath) {
    QStringList lines;
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        file.close();
    }
    return lines;
}

static void EnsureFileWritable(const QString& filepath) {
    QFile::setPermissions(filepath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther | QFile::WriteOther);
}

bool MaterialFix::Force2UVsIfRequired(const MapImporter::Options& options, const QString& refsName, QSet<QString>& global2UVMaterials) {
    QSet<QString> uvsUpdated;
    QString meshinfofilename = refsName;
    int pos = meshinfofilename.lastIndexOf("_refs.txt");
    if (pos != -1) meshinfofilename.replace(pos, 9, "_refs/mesh/meshinfo.txt");

    meshinfofilename.replace('/', '\\');

    if (!QFile::exists(meshinfofilename)) return false;

    QStringList meshinfo = ReadTextFile(meshinfofilename);
    QString meshstring = meshinfo.join("");

    bool b2UV = false;
    if (!QFile::exists(refsName)) return false;

    QStringList refsList = ReadTextFile(refsName);
    int numuvs = 1; // Simplistic parsing
    if (meshstring.contains("'numuvs': 2") || meshstring.contains("\"numuvs\": 2")) {
        numuvs = 2;
    }

    for (const QString& refLine : refsList) {
        if (Miscellaneous::CanceLImport) return false;
        QString mtlfile = CleanRefPath(refLine);
        if (mtlfile.isEmpty()) continue;
        if (uvsUpdated.contains(mtlfile)) continue;

        if (global2UVMaterials.contains(mtlfile)) {
            b2UV = true;
            uvsUpdated.insert(mtlfile);
        } else {
            if (numuvs == 2) {
                b2UV = true;
                Miscellaneous::Log("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                uvsUpdated.insert(mtlfile);

                global2UVMaterials.insert(mtlfile);

                QString vmat = mtlfile;
                int pos = vmat.lastIndexOf(".vmt");
                if (pos != -1) vmat.replace(pos, 4, ".vmat");

                QString vmatfilename = options.s2contentdir + "\\" + vmat;
                if (QFile::exists(vmatfilename)) {
                    QStringList lines = ReadTextFile(vmatfilename);
                    EnsureFileWritable(vmatfilename);

                    bool added = false;
                    for (int i = 0; i < lines.size(); ++i) {
                        QString txt = lines[i];
                        QString lowerTxt = txt.toLower();

                        int start = lowerTxt.indexOf(QRegularExpression("[^ \\t]"));
                        if (start != -1 && lowerTxt.mid(start).startsWith("\"shader\"")) {
                            if (i + 1 < lines.size()) {
                                QString txtNext = lines[i+1];
                                QString lowerNext = txtNext.toLower();

                                int startNext = lowerNext.indexOf(QRegularExpression("[^ \\t]"));
                                if (startNext == -1 || !lowerNext.mid(startNext).startsWith("\"f_force_uv2\"")) {
                                    lines.insert(i + 1, "\t\"F_FORCE_UV2\" \"1\"");
                                    added = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (added) {
                        Miscellaneous::Log("Added F_FORCE_UV2 to " + vmatfilename);
                        QFile file(vmatfilename);
                        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            QTextStream out(&file);
                            for (const QString& l : lines) out << l << "\n";
                            file.close();
                        }
                    }
                }
            }
        }
    }
    return b2UV;
}

void MaterialFix::SkyboxFix(const MapImporter::Options& options) {
    QString materialsDir = options.s2contentdir + "/materials";
    QDirIterator it(materialsDir, QStringList() << "*.vmat", QDir::Files, QDirIterator::Subdirectories);

    QString magickPath = "magick";
    if (QFile::exists("bin/magick.exe")) {
        magickPath = QDir("bin/magick.exe").absolutePath();
    }

    while (it.hasNext()) {
        if (Miscellaneous::CanceLImport) return;
        QString vmatFile = it.next();

        QStringList lines = ReadTextFile(vmatFile);
        bool isSky = false;
        for (const QString& line : lines) {
            if (line.toLower().contains("\"shader\"") && line.toLower().contains("\"sky.vfx\"")) {
                isSky = true;
                break;
            }
        }

        if (isSky) {
            QFileInfo fileInfo(vmatFile);
            QString dirPath = fileInfo.absolutePath();
            QString baseName = fileInfo.completeBaseName();

            QString up = dirPath + "/" + baseName + "up.tga";
            QString bk = dirPath + "/" + baseName + "bk.tga";
            QString rt = dirPath + "/" + baseName + "rt.tga";
            QString ft = dirPath + "/" + baseName + "ft.tga";
            QString lf = dirPath + "/" + baseName + "lf.tga";
            QString dn = dirPath + "/" + baseName + "dn.tga";

            if (QFile::exists(up) && QFile::exists(bk) && QFile::exists(rt) &&
                QFile::exists(ft) && QFile::exists(lf) && QFile::exists(dn)) {

                Miscellaneous::Log("Rebuilding skybox cube for " + baseName + "...");

                // Determine size dynamically
                QProcess identifyProcess;
                identifyProcess.setWorkingDirectory(dirPath);
                identifyProcess.start(magickPath, QStringList() << "identify" << "-format" << "%wx%h" << up);
                identifyProcess.waitForFinished(-1);

                QString sizeStr = "1024x1024"; // Default fallback
                if (identifyProcess.exitStatus() == QProcess::NormalExit && identifyProcess.exitCode() == 0) {
                    QString output = QString::fromUtf8(identifyProcess.readAllStandardOutput()).trimmed();
                    if (!output.isEmpty() && output.contains('x')) {
                        sizeStr = output;
                    }
                }

                QString cubeFile = dirPath + "/" + baseName + "_cube.pfm";

                QStringList args;
                args << "(" << "-size" << sizeStr << "xc:black" << up << "xc:black" << "xc:black" << "+append" << ")"
                     << "(" << bk << rt << ft << lf << "+append" << ")"
                     << "(" << "-size" << sizeStr << "xc:black" << dn << "xc:black" << "xc:black" << "+append" << ")"
                     << "-append"
                     << "-set" << "colorspace" << "sRGB" << "-colorspace" << "RGB"
                     << cubeFile;

                QProcess process;
                process.setWorkingDirectory(dirPath);
                process.start(magickPath, args);
                process.waitForFinished(-1);

                if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
                    Miscellaneous::Log("Successfully rebuilt skybox cube: " + baseName + "_cube.pfm");
                } else {
                    Miscellaneous::Log("Failed to rebuild skybox cube for " + baseName + ". Error: " + process.readAllStandardError());
                }
            }
        }
    }
}
