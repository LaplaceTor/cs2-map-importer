#include "MaterialFix.h"
#include "Miscellaneous.h"
#include "FileExtractFromVPK.h"
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

void MaterialFix::SkyboxFix() {

    QString magickPath = "magick";
    if (QFile::exists("bin/magick.exe")) {
    magickPath = QDir("bin/magick.exe").absolutePath();
    }

    if (Miscellaneous::CanceLImport) return;

    QString mapName = Miscellaneous::GetOptions().mapName;
    QString vmfPath = Miscellaneous::GetOptions().s1contentdir + "/maps/" + mapName + ".vmf";
    QString baseName = "";

    QFile vmfFile(vmfPath);
    if (vmfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&vmfFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.contains("\"skyname\"")) {
                int firstQuote = line.indexOf('"', line.indexOf("\"skyname\"") + 9);
                int lastQuote = line.lastIndexOf('"');
                if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
                    baseName = line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
                }
                break;
            }
        }
        vmfFile.close();
    }

    if (baseName.isEmpty()) return;

    QString dirPath = Miscellaneous::GetOptions().s1contentdir + "/materials/skybox";
    QString s2DirPath = Miscellaneous::GetOptions().s2contentdir + "/materials/skybox";
    QDir().mkpath(dirPath);
    QDir().mkpath(s2DirPath);

    QString oldFile = dirPath + "/" + baseName + "_cube.pfm";
    if(QFile::exists(oldFile)){
        QFile::remove(oldFile);
    }

    QStringList suffixes = {"up", "bk", "rt", "ft", "lf", "dn"};

    for (const QString& suffix : suffixes) {
        QString vtfName = baseName + suffix + ".vtf";
        QString vtfPath = dirPath + "/" + vtfName;

        if (!QFile::exists(vtfPath)) {
            FileExtractFromVPK::ExtractMaterial("materials/skybox/" + vtfName);
        }

        if (QFile::exists(vtfPath)) {
            QString cmd = "\"bin\\vtfcmd.exe\" -file \"" + vtfPath + "\" -output \"" + s2DirPath + "\" -exportformat \"tga\"";
            cmd = cmd.replace("/", "\\");
            Miscellaneous::RunCommandSync(cmd);
        }
    }

    // Look for generated tga files in s2DirPath
    QString up = s2DirPath + "/" + baseName + "up.tga";
    QString bk = s2DirPath + "/" + baseName + "bk.tga";
    QString rt = s2DirPath + "/" + baseName + "rt.tga";
    QString ft = s2DirPath + "/" + baseName + "ft.tga";
    QString lf = s2DirPath + "/" + baseName + "lf.tga";
    QString dn = s2DirPath + "/" + baseName + "dn.tga";

    bool hasUp = QFile::exists(up);
    bool hasBk = QFile::exists(bk);
    bool hasRt = QFile::exists(rt);
    bool hasFt = QFile::exists(ft);
    bool hasLf = QFile::exists(lf);
    bool hasDn = QFile::exists(dn);

    if (hasUp || hasBk || hasRt || hasFt || hasLf || hasDn) {

        Miscellaneous::Log("Rebuilding skybox cube for " + baseName + "...");

        QString existingFace = "";
        if (hasUp) existingFace = up;
        else if (hasBk) existingFace = bk;
        else if (hasRt) existingFace = rt;
        else if (hasFt) existingFace = ft;
        else if (hasLf) existingFace = lf;
        else if (hasDn) existingFace = dn;

        // Resize existing faces to 1024x1024
        QStringList facePaths = {up, bk, rt, ft, lf, dn};
        for (const QString& facePath : facePaths) {
            if (QFile::exists(facePath)) {
                QProcess resizeProcess;
                resizeProcess.setWorkingDirectory(s2DirPath);
                resizeProcess.start(magickPath, QStringList() << facePath << "-resize" << "1024x1024!" << facePath);
                resizeProcess.waitForFinished(-1);
                if (resizeProcess.exitStatus() != QProcess::NormalExit || resizeProcess.exitCode() != 0) {
                    Miscellaneous::Log("Warning: Failed to resize face " + facePath + " to 1024x1024. Error: " + resizeProcess.readAllStandardError());
                }
            }
        }

        QString sizeStr = "1024x1024";

        QString cubeFile = s2DirPath + "/" + baseName + "cube.tga";

        QStringList args;
        args << "(" << "-size" << sizeStr << "xc:black" << (hasUp ? up : "xc:black") << "xc:black" << "xc:black" << "+append" << ")"
             << "(" << "-size" << sizeStr << (hasBk ? bk : "xc:black") << (hasRt ? rt : "xc:black") << (hasFt ? ft : "xc:black") << (hasLf ? lf : "xc:black") << "+append" << ")"
             << "(" << "-size" << sizeStr << "xc:black" << (hasDn ? dn : "xc:black") << "xc:black" << "xc:black" << "+append" << ")"
             << "-append"
             << cubeFile;

        QProcess process;
        process.setWorkingDirectory(s2DirPath);
        process.start(magickPath, args);
        process.waitForFinished(-1);

        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
            Miscellaneous::Log("Successfully rebuilt skybox cube: " + baseName + "cube.tga");
        } else {
            Miscellaneous::Log("Failed to rebuild skybox cube for " + baseName + ". Error: " + process.readAllStandardError());
        }

        // Update vmat file
        QString vmatFile = s2DirPath + "/" + baseName + ".vmat";
        EnsureFileWritable(vmatFile);
        QFile file(vmatFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "Layer0\n";
            out << "{\n";
            out << "\tshader \"sky.vfx\"\n";
            out << "\tSkyTexture \"materials/skybox/" << baseName << "cube.tga\"\n";
            out << "}\n";
            file.close();
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
        } else if (lowerLine.startsWith("\"f_twotexture\"") && lowerLine.contains("\"1\"")) {
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
    SkyboxFix();

    QString materialsDir = Miscellaneous::GetOptions().s2contentdir + "/materials";
    QDirIterator it(materialsDir, QStringList() << "*.vmat", QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (Miscellaneous::CanceLImport) return;
        QString vmatFile = it.next();

        QStringList lines = ReadTextFile(vmatFile);
        bool fileModified = false;
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

    OverlayFix();
    OldParticleMtlFix();

    GenerateNormalForTextures();
}

void MaterialFix::GenerateNormalForTextures() {
    if (!Miscellaneous::GetOptions().generateNormalForTextures) {
        return;
    }

    if (Miscellaneous::CanceLImport) return;

    QString magickPath = "magick";
    if (QFile::exists("bin/magick.exe")) {
        magickPath = QDir("bin/magick.exe").absolutePath();
    }

    QString materialsDir = Miscellaneous::GetOptions().s2contentdir + "/materials";
    if (!QDir(materialsDir).exists()) {
        return;
    }

    QDirIterator it(materialsDir, QStringList() << "*.vmat", QDir::Files, QDirIterator::Subdirectories);

    // Regex patterns
    // Matches TextureColor, TextureLayer1Color, TextureLayer2Color, etc.
    QRegularExpression colorKeyRe("^\"(Texture(Layer\\d+)?Color)\"");
    // Matches TextureNormal, TextureLayer1Normal, TextureLayer2Normal, etc.
    QRegularExpression normalKeyRe("^\"(Texture(Layer\\d+)?Normal)\"");
    // Matches TextureTranslucency, TextureLayer1Translucency, TextureLayer2Translucency, etc.
    QRegularExpression translucencyKeyRe("^\"(Texture(Layer\\d+)?Translucency)\"");

    while (it.hasNext()) {
        if (Miscellaneous::CanceLImport) return;
        QString vmatFile = it.next();

        QStringList lines = ReadTextFile(vmatFile);
        bool fileModified = false;

        // Check if ANY translucency key exists in the file.
        // If so, skip this entire file.
        bool hasTranslucency = false;
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (translucencyKeyRe.match(trimmed).hasMatch()) {
                hasTranslucency = true;
                break;
            }
        }

        if (hasTranslucency) {
            continue;
        }

        // Collect all existing keys to check if normal key is missing
        QSet<QString> existingKeys;
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            // Match keys by extracting the key name (e.g. "TextureLayer1Color" or "TextureLayer1Normal")
            int firstQuote = trimmed.indexOf('"');
            if (firstQuote != -1) {
                int secondQuote = trimmed.indexOf('"', firstQuote + 1);
                if (secondQuote != -1) {
                    QString keyName = trimmed.mid(firstQuote + 1, secondQuote - firstQuote - 1);
                    existingKeys.insert(keyName);
                }
            }
        }

        // Now process the lines and insert normal maps where color exists but normal does not
        for (int i = 0; i < lines.size(); ++i) {
            QString trimmed = lines[i].trimmed();
            QRegularExpressionMatch colorMatch = colorKeyRe.match(trimmed);
            if (colorMatch.hasMatch()) {
                QString colorKey = colorMatch.captured(1);
                // Determine the corresponding normal key name
                QString normalKey = colorKey;
                normalKey.replace("Color", "Normal");

                // If normal key does not exist, we should generate one
                if (!existingKeys.contains(normalKey)) {
                    // Extract the value (source image path)
                    int firstQuoteVal = trimmed.indexOf('"', colorKey.length() + 2);
                    if (firstQuoteVal != -1) {
                        int lastQuoteVal = trimmed.lastIndexOf('"');
                        if (lastQuoteVal > firstQuoteVal) {
                            QString colorRelPath = trimmed.mid(firstQuoteVal + 1, lastQuoteVal - firstQuoteVal - 1);
                            if (colorRelPath.isEmpty()) continue;

                            // Skip color values that are vectors or rgb values (e.g. starting with '[')
                            if (colorRelPath.startsWith('[') || colorRelPath.startsWith('{')) {
                                continue;
                            }

                            // Absolute path to the input color image in s2contentdir
                            QString colorAbsPath = Miscellaneous::GetOptions().s2contentdir + "/" + colorRelPath;
                            colorAbsPath = QDir::cleanPath(colorAbsPath);

                            if (QFile::exists(colorAbsPath)) {
                                // Construct normal filename next to color filename
                                QFileInfo colorFileInfo(colorAbsPath);
                                QString colorFileName = colorFileInfo.fileName();
                                QString ext = colorFileInfo.suffix();
                                QString normalFileName;

                                if (colorFileName.contains("_color", Qt::CaseInsensitive)) {
                                    // replace _color with _normal
                                    int colorIdx = colorFileName.lastIndexOf("_color", -1, Qt::CaseInsensitive);
                                    normalFileName = colorFileName.left(colorIdx) + "_normal." + ext;
                                } else {
                                    normalFileName = colorFileInfo.baseName() + "_normal." + ext;
                                }

                                QString normalAbsPath = colorFileInfo.absolutePath() + "/" + normalFileName;
                                normalAbsPath = QDir::cleanPath(normalAbsPath);

                                // Relative path of normal map to put in the vmat
                                QString normalRelPath = colorRelPath;
                                int lastSlash = normalRelPath.lastIndexOf('/');
                                if (lastSlash == -1) {
                                    lastSlash = normalRelPath.lastIndexOf('\\');
                                }
                                if (lastSlash != -1) {
                                    normalRelPath = normalRelPath.left(lastSlash + 1) + normalFileName;
                                } else {
                                    normalRelPath = normalFileName;
                                }

                                // Run ImageMagick command:
                                // magick input_picture.tga -alpha off -colorspace linear-gray -define convolve:scale="50%!" -bias 50%
                                //   ( -clone 0 -morphology Convolve Sobel:0 )
                                //   ( -clone 0 -morphology Convolve Sobel:-90 )
                                //   ( -clone 0 -evaluate set 100% )
                                //   -delete 0 -combine output_normalmap.tga
                                QStringList args;
                                args << colorAbsPath
                                     << "-alpha" << "off"
                                     << "-colorspace" << "linear-gray"
                                     << "-define" << "convolve:scale=50%!"
                                     << "-bias" << "50%"
                                     << "(" << "-clone" << "0" << "-morphology" << "Convolve" << "Sobel:0" << ")"
                                     << "(" << "-clone" << "0" << "-morphology" << "Convolve" << "Sobel:-90" << ")"
                                     << "(" << "-clone" << "0" << "-evaluate" << "set" << "100%" << ")"
                                     << "-delete" << "0"
                                     << "-combine"
                                     << normalAbsPath;

                                Miscellaneous::Log("Generating normal map for " + colorAbsPath);

                                QProcess process;
                                process.start(magickPath, args);
                                process.waitForFinished(-1);

                                if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
                                    Miscellaneous::Log("Successfully generated normal map: " + normalAbsPath);

                                    // Insert the new line in lines
                                    // Preserve indentation
                                    int leadingSpaceCount = 0;
                                    while (leadingSpaceCount < lines[i].length() && (lines[i][leadingSpaceCount] == ' ' || lines[i][leadingSpaceCount] == '\t')) {
                                        leadingSpaceCount++;
                                    }
                                    QString indent = lines[i].left(leadingSpaceCount);
                                    QString newLine = indent + "\"" + normalKey + "\"\t\t\"" + normalRelPath + "\"";

                                    lines.insert(i + 1, newLine);
                                    fileModified = true;

                                    // Add to existingKeys to prevent duplicate generation if processed in other loops
                                    existingKeys.insert(normalKey);

                                    // Skip the newly inserted line in next iterations
                                    i++;
                                } else {
                                    Miscellaneous::Log("Failed to generate normal map for " + colorAbsPath + ". Error: " + process.readAllStandardError());
                                }
                            } else {
                                Miscellaneous::Log("Source color file not found: " + colorAbsPath);
                            }
                        }
                    }
                }
            }
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

void MaterialFix::OldParticleMtlFix() {
    QString vmfPath = Miscellaneous::GetOptions().s1contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + ".vmf";
    if (!QFile::exists(vmfPath)) return;

    QStringList lines = ReadTextFile(vmfPath);

    QSet<QString> spriteMaterials;
    int bracketDepth = 0;
    bool inEntity = false;
    bool isEnvSprite = false;
    QString currentModel;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        if (line == "entity") {
            inEntity = true;
            isEnvSprite = false;
            currentModel = "";
        } else if (line == "{") {
            bracketDepth++;
        } else if (line == "}") {
            bracketDepth--;
        }

        if (inEntity && bracketDepth == 1) {
            QString lowerLine = line.toLower();
            if (lowerLine.startsWith("\"classname\"") && lowerLine.contains("\"env_sprite\"")) {
                isEnvSprite = true;
            }

            int matIdx = lowerLine.indexOf("\"model\"");
            if (matIdx != -1) {
                int firstQuote = line.indexOf('"', matIdx + 7);
                int lastQuote = line.lastIndexOf('"');
                if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
                    currentModel = line.mid(firstQuote + 1, lastQuote - firstQuote - 1);
                }
            }
        }

        if (line == "}" && bracketDepth == 0 && inEntity) {
            if (isEnvSprite && !currentModel.isEmpty()) {
                spriteMaterials.insert(currentModel);
            }
            inEntity = false;
        }
    }

    QList<QString> materialList;
    for (const QString& modelPath : spriteMaterials) {
        QString vmtPath = modelPath;
        if (vmtPath.endsWith(".spr", Qt::CaseInsensitive)) {
            vmtPath = vmtPath.left(vmtPath.length() - 4) + ".vmt";
        }
        if (!vmtPath.startsWith("materials/", Qt::CaseInsensitive)) {
            vmtPath = "materials/" + vmtPath;
        }
        materialList.append(vmtPath);
    }

    for (const QString& vmtPath : materialList) {
        if (Miscellaneous::CanceLImport) return;

        QString contentPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(vmtPath);
        if (!QFile::exists(contentPath)) {
            FileExtractFromVPK::ExtractMaterial(vmtPath);
        } else {
            QString s1GameDirVmt = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(vmtPath);
            if (!QFile::exists(s1GameDirVmt)) {
                QFileInfo fi(s1GameDirVmt);
                QDir().mkpath(fi.absolutePath());
                QFile::copy(contentPath, s1GameDirVmt);
            }
        }

        QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo \"" + QString(vmtPath).replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(importCmd);

        QString vmatRel = vmtPath;
        vmatRel.replace(".vmt", ".vmat");
        QString s2VmatPath = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(vmatRel);

        QString resCompCmdVmat = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + QString(s2VmatPath).replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(resCompCmdVmat);

        QString vtexRel = vmtPath;
        vtexRel.replace(".vmt", ".vtex");
        QString s2VtexPath = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(vtexRel);

        QString tgaRel = vmtPath;
        tgaRel.replace(".vmt", "_color.tga");

        QFile file(s2VtexPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "<!-- dmx encoding keyvalues2_noids 1 format vtex 1 -->\n";
            out << "\"CDmeVtex\"\n";
            out << "{\n";
            out << "    \"m_inputTextureArray\" \"element_array\"\n";
            out << "    [\n";
            out << "        \"CDmeInputTexture\"\n";
            out << "        {\n";
            out << "            \"m_name\" \"string\" \"InputTexture0\"\n";
            out << "            \"m_fileName\" \"string\" \"" << tgaRel << "\"\n";
            out << "            \"m_colorSpace\" \"string\" \"srgb\"\n";
            out << "            \"m_typeString\" \"string\" \"2D\"\n";
            out << "            \"m_imageProcessorArray\" \"element_array\"\n";
            out << "            [\n";
            out << "                \"CDmeImageProcessor\"\n";
            out << "                {\n";
            out << "                    \"m_algorithm\" \"string\" \"None\"\n";
            out << "                    \"m_stringArg\" \"string\" \"\"\n";
            out << "                    \"m_vFloat4Arg\" \"vector4\" \"0 0 0 0\"\n";
            out << "                }\n";
            out << "            ]\n";
            out << "        }\n";
            out << "    ]\n";
            out << "    \"m_outputTypeString\" \"string\" \"2D\"\n";
            out << "    \"m_outputFormat\" \"string\" \"DXT5\"\n";
            out << "    \"m_outputClearColor\" \"vector4\" \"0 0 0 0\"\n";
            out << "    \"m_nOutputMinDimension\" \"int\" \"0\"\n";
            out << "    \"m_nOutputMaxDimension\" \"int\" \"0\"\n";
            out << "    \"m_textureOutputChannelArray\" \"element_array\"\n";
            out << "    [\n";
            out << "        \"CDmeTextureOutputChannel\"\n";
            out << "        {\n";
            out << "            \"m_inputTextureArray\" \"string_array\" [ \"InputTexture0\" ]\n";
            out << "            \"m_srcChannels\" \"string\" \"rgba\"\n";
            out << "            \"m_dstChannels\" \"string\" \"rgba\"\n";
            out << "            \"m_mipAlgorithm\" \"CDmeImageProcessor\"\n";
            out << "            {\n";
            out << "                \"m_algorithm\" \"string\" \"Box\"\n";
            out << "                \"m_stringArg\" \"string\" \"\"\n";
            out << "                \"m_vFloat4Arg\" \"vector4\" \"0 0 0 0\"\n";
            out << "            }\n";
            out << "            \"m_outputColorSpace\" \"string\" \"srgb\"\n";
            out << "        }\n";
            out << "    ]\n";
            out << "    \"m_vClamp\" \"vector3\" \"0 0 0\"\n";
            out << "    \"m_bNoLod\" \"bool\" \"0\"\n";
            out << "}\n";
            file.close();
        }

        QString resCompCmdVtex = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + QString(s2VtexPath).replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(resCompCmdVtex);
    }
}

void MaterialFix::OverlayFix() {
    QString vmfPath = Miscellaneous::GetOptions().s1contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + ".vmf";
    if (!QFile::exists(vmfPath)) return;

    QStringList lines = ReadTextFile(vmfPath);

    // Pass 1: find all info_overlay blocks, collect materials used, and track line indices
    QSet<QString> overlayMaterials;
    QMap<int, QString> materialLinesToFix; // index -> material name

    int bracketDepth = 0;
    bool inEntity = false;
    bool isInfoOverlay = false;
    QString currentMaterial;
    int currentMaterialIdx = -1;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        if (line == "entity") {
            inEntity = true;
            isInfoOverlay = false;
            currentMaterial = "";
            currentMaterialIdx = -1;
        } else if (line == "{") {
            bracketDepth++;
        } else if (line == "}") {
            bracketDepth--;
        }

        if (inEntity && bracketDepth == 1) {
            QString lowerLine = line.toLower();
            if (lowerLine.startsWith("\"classname\"") && lowerLine.contains("\"info_overlay\"")) {
                isInfoOverlay = true;
            }


            int matIdx = lowerLine.indexOf("\"material\"");
            if (matIdx != -1) {
                int firstQuote = line.indexOf('"', matIdx + 10);
                int lastQuote = line.lastIndexOf('"');
                if (firstQuote != -1 && lastQuote != -1 && lastQuote > firstQuote) {
                    currentMaterial = line.mid(firstQuote + 1, lastQuote - firstQuote - 1);

                    currentMaterialIdx = i;
                }
            }

        }

        if (line == "}" && bracketDepth == 0 && inEntity) {
            // Finished parsing an entity
            if (isInfoOverlay && !currentMaterial.isEmpty() && currentMaterialIdx != -1) {
                overlayMaterials.insert(currentMaterial);
                materialLinesToFix[currentMaterialIdx] = currentMaterial;
            }
            inEntity = false;
        }
    }

    // Process collected materials
    QMap<QString, QString> materialReplacementMap;

    for (const QString& matName : overlayMaterials) {
        QString vmatPath = Miscellaneous::GetOptions().s2contentdir + "/materials/" + matName + ".vmat";

        if (!QFile::exists(vmatPath)) {
            vmatPath = Miscellaneous::GetOptions().s2contentdir + "\\materials\\" + QString(matName).replace('/', '\\') + ".vmat";
            if (!QFile::exists(vmatPath)) continue;
        }

        QStringList vmatLines = ReadTextFile(vmatPath);
        bool hasFOverlay = false;
        bool hasFDecalTexture = false;
        bool isLightMapped = false;
        bool isComplex = false;
        int shaderLineIdx = -1;

        for (int i = 0; i < vmatLines.size(); ++i) {
            QString line = vmatLines[i].trimmed().toLower();

            if (line.startsWith("\"shader\"") || line.startsWith("shader")) {
                if (line.contains("csgo_lightmappedgeneric.vfx")) isLightMapped = true;
                else if (line.contains("csgo_complex.vfx")) isComplex = true;


                shaderLineIdx = i;
            }

            if (line.contains("f_overlay") && line.contains("1")) hasFOverlay = true;
            if (line.contains("f_decal_texture") && line.contains("1")) hasFDecalTexture = true;
        }

        if (shaderLineIdx != -1) {
            bool needsFix = false;
            if (isLightMapped && !hasFOverlay) needsFix = true;
            else if (isComplex && !hasFDecalTexture) needsFix = true;

            if (needsFix) {
                vmatLines[shaderLineIdx].replace("csgo_lightmappedgeneric.vfx", "csgo_static_overlay.vfx", Qt::CaseInsensitive);
                vmatLines[shaderLineIdx].replace("csgo_complex.vfx", "csgo_static_overlay.vfx", Qt::CaseInsensitive);

                for (int i = 0; i < vmatLines.size(); ++i) {
                    if (isLightMapped) {
                        vmatLines[i].replace("TextureLayer1Color", "TextureColor", Qt::CaseInsensitive);
                        vmatLines[i].replace("TextureLayer1Normal", "TextureNormal", Qt::CaseInsensitive);
                        vmatLines[i].replace("TextureLayer1Roughness", "TextureRoughness", Qt::CaseInsensitive);
                        vmatLines[i].replace("TextureLayer1AmbientOcclusion", "TextureAmbientOcclusion", Qt::CaseInsensitive);
                        vmatLines[i].replace("TextureLayer1Translucency", "TextureTranslucency", Qt::CaseInsensitive);
                    }
                }

                bool hasAlphaTest = false;
                bool hasTranslucent = false;
                bool hasAdditiveBlend = false;

                for (int i = 0; i < vmatLines.size(); ++i) {
                    QString lower = vmatLines[i].toLower();
                    if (lower.contains("f_alpha_test") && lower.contains("1")) { hasAlphaTest = true; vmatLines[i] = ""; }
                    if (lower.contains("f_translucent") && lower.contains("1")) { hasTranslucent = true; vmatLines[i] = ""; }
                    if (lower.contains("f_additive_blend") && lower.contains("1")) { hasAdditiveBlend = true; vmatLines[i] = ""; }
                }


                // Insert new flags BEFORE removing empty lines to preserve shaderLineIdx
                if (hasTranslucent && hasAdditiveBlend) {
                    vmatLines.insert(shaderLineIdx + 1, "	\"F_BLEND_MODE\"\t\t\"4\"");
                } else if (hasTranslucent) {
                    vmatLines.insert(shaderLineIdx + 1, "	\"F_BLEND_MODE\"\t\t\"1\"");
                } else if (hasAlphaTest) {
                    vmatLines.insert(shaderLineIdx + 1, "	\"F_BLEND_MODE\"\t\t\"2\"");
                }

                vmatLines.insert(shaderLineIdx + 1, "	\"F_LIT\"\t\t\"1\"");

                // Cleanup empty lines we created
                for (int i = vmatLines.size() - 1; i >= 0; --i) {
                    if (vmatLines[i].isEmpty()) vmatLines.removeAt(i);
                }


                QString newMatName = matName + "overlay";
                materialReplacementMap[matName] = newMatName;
                QString newVmatPath = Miscellaneous::GetOptions().s2contentdir + "/materials/" + newMatName + ".vmat";

                QFile file(newVmatPath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    for (const QString& l : vmatLines) out << l << "\n";
                    file.close();
                }

                QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + newVmatPath + "\"";
                Miscellaneous::RunCommandSync(resCompCmd);
            }
        }
    }

    // Pass 2: Apply replacements to VMF
    if (!materialReplacementMap.isEmpty()) {
        bool vmfModified = false;
        for (auto it = materialLinesToFix.begin(); it != materialLinesToFix.end(); ++it) {
            int idx = it.key();
            QString matName = it.value();
            if (materialReplacementMap.contains(matName)) {
                QString newMat = materialReplacementMap[matName];
                QString newLine = lines[idx];
                newLine.replace("\"" + matName + "\"", "\"" + newMat + "\"");
                lines[idx] = newLine;
                vmfModified = true;
            }
        }

        if (vmfModified) {
            EnsureFileWritable(vmfPath);
            QFile file(vmfPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                for (const QString& l : lines) out << l << "\n";
                file.close();
            }
        }
    }
}

void MaterialFix::DevTextureFix() {
    QString usebspStr = Miscellaneous::GetOptions().usebsp ? "-usebsp" : "";
    QString nomergeinstancesStr = Miscellaneous::GetOptions().usebspNomergeinstances ? "-usebsp_nomergeinstances" : "";

    QString mapImportCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync " + usebspStr;
    if (!nomergeinstancesStr.isEmpty()) mapImportCmd += " " + nomergeinstancesStr;

    QString targetS1gamedir =  Miscellaneous::GetOptions().csgogamedir;

    mapImportCmd += " -src1gameinfodir \"" + targetS1gamedir + "\" -src1contentdir \"" + Miscellaneous::GetOptions().s1contentdir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo maps\\" + Miscellaneous::GetOptions().mapName + ".vmf";

    Miscellaneous::Log("Running mapImportCmd in DevTextureFix: " + mapImportCmd);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram("cmd.exe");
#ifdef Q_OS_WIN
    process.setNativeArguments("/S /C \"" + mapImportCmd + "\"");
#else
    process.setArguments({"/c", mapImportCmd});
#endif
    process.start();

    QString lineBuffer;
    QList<QString> missingMaterials;

    auto processOutput = [&](const QString& outStr) {
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty()) {
                    Miscellaneous::Log(lineBuffer);

                    if (lineBuffer.startsWith("Failed loading resource \"materials/dev/") || lineBuffer.startsWith("Failed loading resource \"materials/tools/")) {
                        if (lineBuffer.endsWith("vmat_c\" (ERROR_FILEOPEN: File not found)")) {
                            int startIdx = lineBuffer.indexOf('"') + 1;
                            int endIdx = lineBuffer.lastIndexOf('"');
                            if (startIdx > 0 && endIdx > startIdx) {
                                QString matPath = lineBuffer.mid(startIdx, endIdx - startIdx);
                                if (matPath.endsWith(".vmat_c")) {
                                    matPath.replace(".vmat_c", ".vmt");
                                    missingMaterials.append(matPath);
                                }
                            }
                        }
                    }
                }
                lineBuffer.clear();
            } else {
                lineBuffer += c;
            }
        }
    };

    while (process.waitForReadyRead(10000) || process.state() != QProcess::NotRunning) {
        if (Miscellaneous::CanceLImport) {
            process.kill();
            return;
        }
        QByteArray output = process.readAll();
        if (!output.isEmpty()) {
            processOutput(QString(output));
        } else {
            if (process.state() == QProcess::Running) {
                process.write("y\n");
            }
        }
    }

    QByteArray output = process.readAll();
    if (!output.isEmpty()) {
        processOutput(QString(output));
    }

    if (!lineBuffer.isEmpty()) {
        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
        Miscellaneous::Log(lineBuffer);
    }

    for (QString& vmtPath : missingMaterials) {
        if (Miscellaneous::CanceLImport) return;

        vmtPath.replace('\\', '/'); // Make sure paths are standardized

        FileExtractFromVPK::ExtractMaterial(vmtPath);

        QString origVmtS1 = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(vmtPath);
        if (!QFile::exists(origVmtS1)) {
            Miscellaneous::Log("Failed to extract " + vmtPath);
            continue;
        }

        QString tmpVmtRel = vmtPath;
        if (tmpVmtRel.startsWith("materials/dev/")) {
            tmpVmtRel.replace("materials/dev/", "materials/tmp/dev/");
        } else if (tmpVmtRel.startsWith("materials/tools/")) {
            tmpVmtRel.replace("materials/tools/", "materials/tmp/tools/");
        }

        QString tmpVmtS1 = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(tmpVmtRel);
        QDir().mkpath(QFileInfo(tmpVmtS1).absolutePath());

        if (QFile::exists(tmpVmtS1)) QFile::remove(tmpVmtS1);
        if (QFile::copy(origVmtS1, tmpVmtS1)) {
            QFile::remove(origVmtS1); // Move operation
        } else {
            continue;
        }

        QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo \"" + tmpVmtRel.replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(importCmd);

        QString tmpVmatRel = tmpVmtRel;
        tmpVmatRel.replace(".vmt", ".vmat");
        QString origVmatRel = vmtPath;
        origVmatRel.replace(".vmt", ".vmat");

        QString tmpVmatS2 = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(tmpVmatRel);
        QString origVmatS2 = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(origVmatRel);

        if (QFile::exists(tmpVmatS2)) {
            QDir().mkpath(QFileInfo(origVmatS2).absolutePath());
            if (QFile::exists(origVmatS2)) QFile::remove(origVmatS2);
            if (QFile::copy(tmpVmatS2, origVmatS2)) {
                QFile::remove(tmpVmatS2);
            }

            QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + origVmatS2.replace('/', '\\') + "\"";
            Miscellaneous::RunCommandSync(resCompCmd);
        }
    }
}
