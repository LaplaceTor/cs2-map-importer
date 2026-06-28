#include "MaterialFix.h"
#include "Miscellaneous.h"
#include <QFile>
#include <QTextStream>
#include <QMap>
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

bool MaterialFix::Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials) {
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

                QString vmatfilename = Miscellaneous::GetOptions().s2contentdir + "\\" + vmat;
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

void MaterialFix::SkyboxFix(const QString& vmatFile) {

    QString magickPath = "magick";
    if (QFile::exists("bin/magick.exe")) {
    magickPath = QDir("bin/magick.exe").absolutePath();
    }

    if (Miscellaneous::CanceLImport) return;


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

        QString cubeFile = dirPath + "/" + baseName + "_cube.tga";

        QStringList args;
        args << "(" << "-size" << sizeStr << "xc:black" << up << "xc:black" << "xc:black" << "+append" << ")"
             << "(" << bk << rt << ft << lf << "+append" << ")"
             << "(" << "-size" << sizeStr << "xc:black" << dn << "xc:black" << "xc:black" << "+append" << ")"
             << "-append"
             << cubeFile;

        QProcess process;
        process.setWorkingDirectory(dirPath);
        process.start(magickPath, args);
        process.waitForFinished(-1);

        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
            Miscellaneous::Log("Successfully rebuilt skybox cube: " + baseName + "_cube.tga");
        } else {
            Miscellaneous::Log("Failed to rebuild skybox cube for " + baseName + ". Error: " + process.readAllStandardError());
        }

        // Update vmat file
        QStringList vmatLines = ReadTextFile(vmatFile);
        bool modified = false;

        for (int i = 0; i < vmatLines.size(); ++i) {
            QString lowerLine = vmatLines[i].toLower();

            // Remove F_TEXTURE_FORMAT2
            if (lowerLine.contains("\"f_texture_format2\"")) {
                vmatLines.removeAt(i);
                i--;
                modified = true;
                continue;
            }

            // Update SkyTexture from .pfm to .tga
            if (lowerLine.contains("\"skytexture\"") && lowerLine.contains(".pfm\"")) {
                vmatLines[i].replace(".pfm", ".tga", Qt::CaseInsensitive);
                modified = true;
            }
        }

        if (modified) {
            EnsureFileWritable(vmatFile);
            QFile file(vmatFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                for (const QString& l : vmatLines) out << l << "\n";
                file.close();
            }
        }
    }
}

void MaterialFix::MissingKVFix(QStringList& lines, bool& fileModified) {
    bool hasTranslucentSource = false;
    bool hasAlphaTestSource = false;
    bool hasAdditiveSource = false;

    bool hasTranslucentDest = false;
    bool hasAlphaTestDest = false;
    bool hasAdditiveDest = false;

    int insertIndex = -1;
    QString insertPrefix = "\t";

    for (int i = 0; i < lines.size(); ++i) {
        QString lowerLine = lines[i].trimmed().toLower();

        if (lowerLine.startsWith("\"shader\"") && insertIndex == -1) {
            insertIndex = i + 1;
            QRegularExpressionMatch matchSpace = QRegularExpression("^(\\s*)").match(lines[i]);
            insertPrefix = matchSpace.captured(1);
        }

        if (lowerLine.contains("\"$translucent\"") && lowerLine.contains("\"1\"")) hasTranslucentSource = true;
        if (lowerLine.contains("\"$alphatest\"") && lowerLine.contains("\"1\"")) hasAlphaTestSource = true;
        if (lowerLine.contains("\"$additive\"") && lowerLine.contains("\"1\"")) hasAdditiveSource = true;

        if (lowerLine.startsWith("\"f_translucent\"")) hasTranslucentDest = true;
        if (lowerLine.startsWith("\"f_alpha_test\"")) hasAlphaTestDest = true;
        if (lowerLine.startsWith("\"f_additive_blend\"")) hasAdditiveDest = true;
    }

    if (insertIndex != -1) {
        if (hasTranslucentSource && !hasTranslucentDest) {
            lines.insert(insertIndex, insertPrefix + "\"F_TRANSLUCENT\"\t\t\"1\"");
            fileModified = true;
        }
        if (hasAlphaTestSource && !hasAlphaTestDest) {
            lines.insert(insertIndex, insertPrefix + "\"F_ALPHA_TEST\"\t\t\"1\"");
            fileModified = true;
        }
        if (hasAdditiveSource && !hasAdditiveDest) {
            lines.insert(insertIndex, insertPrefix + "\"F_ADDITIVE_BLEND\"\t\t\"1\"");
            fileModified = true;
        }
    }
}

void MaterialFix::TranslucentAlphaTestConflictFix(QStringList& lines, bool& fileModified) {
    bool hasTranslucent = false;
    bool hasAlphaTest = false;
    bool hasOpacityScale = false;

    int translucentIdx = -1;
    int alphaTestIdx = -1;

    for (int i = 0; i < lines.size(); ++i) {
        QString lowerLine = lines[i].trimmed().toLower();
        if (lowerLine.startsWith("\"f_translucent\"")) {
            hasTranslucent = true;
            translucentIdx = i;
        } else if (lowerLine.startsWith("\"f_alpha_test\"")) {
            hasAlphaTest = true;
            alphaTestIdx = i;
        } else if (lowerLine.startsWith("\"g_flopacityscale\"")) {
            hasOpacityScale = true;
        }
    }

    if (hasTranslucent && hasAlphaTest) {
        if (hasOpacityScale) {
            lines.removeAt(alphaTestIdx);
            fileModified = true;
        } else {
            lines.removeAt(translucentIdx);
            fileModified = true;
        }
    }
}

void MaterialFix::ComplexShaderVariablesFix(QStringList& lines, bool& fileModified) {
    bool hasAddedAniso = false;
    QStringList newLines;
    bool localModified = false;

    QRegularExpression leadingSpaceRe("^(\\s*)");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString lowerLine = line.trimmed().toLower();

        QRegularExpressionMatch matchSpace = leadingSpaceRe.match(line);
        QString prefix = matchSpace.captured(1);

        if (lowerLine.startsWith("\"f_vertex_color\"") && lowerLine.contains("\"1\"")) {
            newLines.append(prefix + "\"F_PAINT_VERTEX_COLORS\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_force_uv2\"") && lowerLine.contains("\"1\"")) {
            newLines.append(prefix + "\"F_SECONDARY_UV\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"1\"")) {
            newLines.append(prefix + "\"F_TRANSLUCENT\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"2\"")) {
            newLines.append(prefix + "\"F_ALPHA_TEST\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"3\"")) {
            newLines.append(prefix + "\"F_DETAIL_TEXTURE\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"4\"")) {
            newLines.append(prefix + "\"F_ADDITIVE_BLEND\"\t\t\"1\"");
            newLines.append(prefix + "\"F_TRANSLUCENT\"\t\t\"1\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"5\"")) {
            newLines.append(prefix + "\"F_DETAIL_TEXTURE\"\t\t\"2\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_blend_mode\"") && lowerLine.contains("\"6\"")) {
            newLines.append(prefix + "\"F_DETAIL_TEXTURE\"\t\t\"4\"");
            localModified = true;
        } else if (lowerLine.startsWith("\"f_specular_indirect\"") && lowerLine.contains("\"1\"")) {
            if (!hasAddedAniso) {
                newLines.append(prefix + "\"F_ANISOTROPIC_GLOSS\"\t\t\"1\"");
                hasAddedAniso = true;
            }
            localModified = true;
        } else if (lowerLine.startsWith("\"f_specular_direct\"") && lowerLine.contains("\"1\"")) {
            if (!hasAddedAniso) {
                newLines.append(prefix + "\"F_ANISOTROPIC_GLOSS\"\t\t\"1\"");
                hasAddedAniso = true;
            }
            localModified = true;
        } else {
            newLines.append(line);
        }
    }

    if (localModified) {
        lines = newLines;
        fileModified = true;
    }
}


struct KeyMapping {
    QString newKey;
    bool appendAlpha1;
};

static QMap<QString, KeyMapping> legacyKeyMap = { { "\"$color2\"", { "\"g_vColorTint\"", true } } };

void MaterialFix::ColorFix(QStringList& lines, int layer0StartIdx, int& layer0EndIdx, const QMap<QString, QString>& foundLegacyKeys, bool& fileModified) {
    // Filter keys that exist in our mapping
    QMap<QString, QString> validKeys;
    for (auto itFound = foundLegacyKeys.begin(); itFound != foundLegacyKeys.end(); ++itFound) {
        if (legacyKeyMap.contains(itFound.key())) {
            validKeys[itFound.key()] = itFound.value();
        }
    }

    if (layer0StartIdx != -1 && layer0EndIdx != -1 && !validKeys.isEmpty()) {
        // Try adding/updating the keys in Layer0
        for (auto itFound = validKeys.begin(); itFound != validKeys.end(); ++itFound) {
            QString legacyKey = itFound.key();
            QString legacyVal = itFound.value();
            KeyMapping mapping = legacyKeyMap[legacyKey];

            // Format the new value
            QString newVal = legacyVal;
            if (mapping.appendAlpha1 && newVal.endsWith("]\"")) {
                newVal = newVal.left(newVal.length() - 2) + " 1.0]\"";
            }

            bool keyUpdated = false;
            // Search for the existing key in Layer0 block
            for (int j = layer0StartIdx + 1; j < layer0EndIdx; ++j) {
                QString tLine = lines[j].trimmed().toLower();
                if (tLine.startsWith(mapping.newKey.toLower())) {
                    lines[j] = "\t\t" + mapping.newKey + " " + newVal;
                    keyUpdated = true;
                    fileModified = true;
                    break;
                }
            }

            if (!keyUpdated) {
                // We just append to the end of Layer0 before the closing brace
                lines.insert(layer0EndIdx, "\t\t" + mapping.newKey + " " + newVal);
                layer0EndIdx++; // Adjust end index since we inserted a line
                fileModified = true;
            }
        }
    }
}

void MaterialFix::ShaderFix(QStringList& lines, bool& fileModified) {
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString lowerLine = line.trimmed().toLower();

        if (lowerLine.startsWith("\"shader\"")) {
            if (lowerLine.contains("\"csgo_unlitgeneric.vfx\"") || lowerLine.contains("\"csgo_vertexlitgeneric.vfx\"")) {
                // Replace the specific legacy shader with csgo_complex.vfx while trying to preserve spacing
                QString newLine = line;
                newLine.replace("csgo_unlitgeneric.vfx", "csgo_complex.vfx", Qt::CaseInsensitive);
                newLine.replace("csgo_vertexlitgeneric.vfx", "csgo_complex.vfx", Qt::CaseInsensitive);
                lines[i] = newLine;
                fileModified = true;
            }
        }
    }
}

void MaterialFix::FixMaterials() {
    QString materialsDir = Miscellaneous::GetOptions().s2contentdir + "/materials";
    QDirIterator it(materialsDir, QStringList() << "*.vmat", QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (Miscellaneous::CanceLImport) return;
        QString vmatFile = it.next();

        QStringList lines = ReadTextFile(vmatFile);
        bool fileModified = false;
        bool isSky = false;
        bool isLegacyShader = false;

        // We look for legacy keys and values
        QMap<QString, QString> foundLegacyKeys;
        int layer0StartIdx = -1;
        int layer0EndIdx = -1;
        int bracketDepth = 0;

        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i].trimmed();
            QString lowerLine = line.toLower();

            // Track blocks
            if (line == "{") bracketDepth++;
            else if (line == "}") bracketDepth--;

            if (lowerLine.contains("\"shader\"") && lowerLine.contains("\"sky.vfx\"")) {
                isSky = true;
            }

            if (lowerLine.startsWith("\"shader\"") && (lowerLine.contains("\"csgo_unlitgeneric.vfx\"") || lowerLine.contains("\"csgo_vertexlitgeneric.vfx\""))) {
                isLegacyShader = true;
            }

            if (lowerLine == "\"layer0\"" && i + 1 < lines.size() && lines[i+1].trimmed() == "{") {
                layer0StartIdx = i + 1; // Index of the opening brace
                int tempDepth = bracketDepth;
                for (int j = i + 1; j < lines.size(); ++j) {
                    QString tLine = lines[j].trimmed();
                    if (tLine == "{") tempDepth++;
                    else if (tLine == "}") {
                        tempDepth--;
                        if (tempDepth == bracketDepth) {
                            layer0EndIdx = j;
                            break;
                        }
                }
            }

            // Check for legacy_import block inside Layer0
            if (layer0StartIdx != -1 && layer0EndIdx != -1) {
                for (int j = layer0StartIdx + 1; j < layer0EndIdx; ++j) {
                    if (lines[j].trimmed().toLower() == "\"legacy_import\"") {
                        layer0EndIdx = j; // Set the insertion point *before* legacy_import
                        break;
                    }
                    }
                }
            }

            for (auto itMap = legacyKeyMap.begin(); itMap != legacyKeyMap.end(); ++itMap) {
                if (lowerLine.startsWith(itMap.key())) {
                    int firstQuote = line.indexOf('"', itMap.key().length());
                    int lastQuote = line.lastIndexOf('"');
                    if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
                        QString valueStr = line.mid(firstQuote, lastQuote - firstQuote + 1);
                        foundLegacyKeys[itMap.key()] = valueStr;
                    }
                }
            }
        }

        if (isSky) {
            SkyboxFix(vmatFile);
        }

        if (isLegacyShader) {
            ShaderFix(lines, fileModified);
            ComplexShaderVariablesFix(lines, fileModified);
        }

        MissingKVFix(lines, fileModified);
        TranslucentAlphaTestConflictFix(lines, fileModified);

        if (!foundLegacyKeys.isEmpty()) {
            ColorFix(lines, layer0StartIdx, layer0EndIdx, foundLegacyKeys, fileModified);
        }

        if (fileModified) {
            EnsureFileWritable(vmatFile);
            QFile file(vmatFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                for (const QString& l : lines) out << l << "\n";
                file.close();
            }
        }
    }
}
