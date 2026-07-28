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
#include <QImage>
#include <QColor>
#include <QPainter>


bool MaterialFix::Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials) {
    QSet<QString> uvsUpdated;
    QString meshinfofilename = refsName;
    int pos = meshinfofilename.lastIndexOf("_refs.txt");
    if (pos != -1) meshinfofilename.replace(pos, 9, "_refs/mesh/meshinfo.txt");

    meshinfofilename.replace('/', '\\');

    if (!QFile::exists(meshinfofilename)) return false;

    QStringList meshinfo = Miscellaneous::ReadTextFile(meshinfofilename);
    QString meshstring = meshinfo.join("");

    bool b2UV = false;
    if (!QFile::exists(refsName)) return false;

    QStringList refsList = Miscellaneous::ReadTextFile(refsName);
    int numuvs = 1; // Simplistic parsing
    if (meshstring.contains("'numuvs': 2") || meshstring.contains("\"numuvs\": 2")) {
        numuvs = 2;
    }

    for (const QString& refLine : refsList) {
        if (Miscellaneous::CanceLImport) return false;
        QString mtlfile = Miscellaneous::CleanRefPath(refLine);
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
                    QStringList lines = Miscellaneous::ReadTextFile(vmatfilename);
                    Miscellaneous::EnsureFileWritable(vmatfilename);

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
            QString program = QDir::toNativeSeparators("bin/vtfcmd.exe");
            QStringList arguments = {
                "-file",
                QDir::toNativeSeparators(vtfPath),
                "-output",
                QDir::toNativeSeparators(s2DirPath),
                "-exportformat",
                "jpg"
            };
            Miscellaneous::RunCommandSync(program, arguments);
        }
    }

    // Look for generated jpg files in s2DirPath
    QString up = s2DirPath + "/" + baseName + "up.jpg";
    QString bk = s2DirPath + "/" + baseName + "bk.jpg";
    QString rt = s2DirPath + "/" + baseName + "rt.jpg";
    QString ft = s2DirPath + "/" + baseName + "ft.jpg";
    QString lf = s2DirPath + "/" + baseName + "lf.jpg";
    QString dn = s2DirPath + "/" + baseName + "dn.jpg";

    bool hasUp = QFile::exists(up);
    bool hasBk = QFile::exists(bk);
    bool hasRt = QFile::exists(rt);
    bool hasFt = QFile::exists(ft);
    bool hasLf = QFile::exists(lf);
    bool hasDn = QFile::exists(dn);

    if (hasUp || hasBk || hasRt || hasFt || hasLf || hasDn) {

        Miscellaneous::Log("Rebuilding skybox cube for " + baseName + "...");

        // Create a 4096x3072 QImage initialized with black color
        QImage cubeImage(4096, 3072, QImage::Format_RGB32);
        cubeImage.fill(Qt::black);

        QPainter painter(&cubeImage);

        // Define a helper lambda to load, resize, and draw each face
        auto drawFace = [&](const QString& facePath, bool exists, int gridX, int gridY) {
            QImage face;
            if (exists && face.load(facePath)) {
                // Resize the face to 1024x1024 using high-quality smooth transformation
                QImage scaledFace = face.scaled(1024, 1024, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                painter.drawImage(gridX * 1024, gridY * 1024, scaledFace);
            } else {
                // If it doesn't exist or fails to load, draw black color (already filled, but explicitly make sure)
                painter.fillRect(gridX * 1024, gridY * 1024, 1024, 1024, Qt::black);
            }
        };

        // Row 1: [black, up, black, black]
        drawFace(up, hasUp, 1, 0);

        // Row 2: [bk, rt, ft, lf]
        drawFace(bk, hasBk, 0, 1);
        drawFace(rt, hasRt, 1, 1);
        drawFace(ft, hasFt, 2, 1);
        drawFace(lf, hasLf, 3, 1);

        // Row 3: [black, dn, black, black]
        drawFace(dn, hasDn, 1, 2);

        painter.end();

        QString cubeFile = s2DirPath + "/" + baseName + "cube.jpg";
        if (cubeImage.save(cubeFile, "JPG", 95)) {
            Miscellaneous::Log("Successfully rebuilt skybox cube: " + baseName + "cube.jpg");
        } else {
            Miscellaneous::Log("Failed to rebuild skybox cube for " + baseName + " using QImage.");
        }

        // Update vmat file
        QString vmatFile = s2DirPath + "/" + baseName + ".vmat";
        Miscellaneous::EnsureFileWritable(vmatFile);
        QFile file(vmatFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "Layer0\n";
            out << "{\n";
            out << "\tshader \"sky.vfx\"\n";
            out << "\tSkyTexture \"materials/skybox/" << baseName << "cube.jpg\"\n";
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
                newVal = newVal.left(newVal.size() - 2) + " 1.0]\"";
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

        QStringList lines = Miscellaneous::ReadTextFile(vmatFile);
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
                    int firstQuote = line.indexOf('"', itMap.key().size());
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
            Miscellaneous::EnsureFileWritable(vmatFile);
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
}

void MaterialFix::OldParticleMtlFix() {
    QString vmfPath = Miscellaneous::GetOptions().s1contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + ".vmf";
    if (!QFile::exists(vmfPath)) return;

    QStringList lines = Miscellaneous::ReadTextFile(vmfPath);

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

    QStringList materialList;
    for (const QString& modelPath : spriteMaterials) {
        QString vmtPath = modelPath;
        if (vmtPath.endsWith(".spr", Qt::CaseInsensitive)) {
            vmtPath = vmtPath.left(vmtPath.size() - 4) + ".vmt";
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

        QString program = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
        QStringList arguments = {
            "-retail",
            "-nop4",
            "-nop4sync",
            "-src1gameinfodir",
            Miscellaneous::GetOptions().s1gamedir,
            "-s2addon",
            Miscellaneous::GetOptions().addonName,
            "-game",
            "csgo",
            QDir::toNativeSeparators(vmtPath)
        };
        Miscellaneous::RunCommandSync(program, arguments);

        QString vmatRel = vmtPath;
        vmatRel.replace(".vmt", ".vmat");
        QString s2VmatPath = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(vmatRel);

        QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
        QStringList argumentsRc = {
            "-retail",
            "-nop4",
            "-game",
            "csgo",
            QDir::toNativeSeparators(s2VmatPath)
        };
        Miscellaneous::RunCommandSync(programRc, argumentsRc);

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

        QString programVtex = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
        QStringList argumentsVtex = {
            "-retail",
            "-nop4",
            "-game",
            "csgo",
            QDir::toNativeSeparators(s2VtexPath)
        };
        Miscellaneous::RunCommandSync(programVtex, argumentsVtex);
    }
}

void MaterialFix::OverlayFix() {
    QString vmfPath = Miscellaneous::GetOptions().s1contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + ".vmf";
    if (!QFile::exists(vmfPath)) return;

    QStringList lines = Miscellaneous::ReadTextFile(vmfPath);

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
        QString vmatPath = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/materials/" + matName + ".vmat");

        if (!QFile::exists(vmatPath)) {
            vmatPath = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/materials/" + QDir::fromNativeSeparators(matName) + ".vmat");
            if (!QFile::exists(vmatPath)) continue;
        }

        QStringList vmatLines = Miscellaneous::ReadTextFile(vmatPath);
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

                QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
                QStringList argumentsRc = {
                    "-retail",
                    "-nop4",
                    "-game",
                    "csgo",
                    QDir::toNativeSeparators(newVmatPath)
                };
                Miscellaneous::RunCommandSync(programRc, argumentsRc);
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
            Miscellaneous::EnsureFileWritable(vmfPath);
            QFile file(vmfPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                for (const QString& l : lines) out << l << "\n";
                file.close();
            }
        }
    }
}
