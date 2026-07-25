#include "ModelImporter.h"
#include "Miscellaneous.h"
#include "FileExtractFromVPK.h"
#include "MaterialFix.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
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

QStringList ModelImporter::ReadTextFile(const QString& filepath) {
    QStringList lines;
    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (!line.isEmpty() && line.endsWith('\r')) {
                line.chop(1);
            }
            lines.append(line);
        }
    }
    return lines;
}

void ModelImporter::EnsureFileWritable(const QString& filepath) {
    QFileInfo p(filepath);
    if (p.exists()) {
        QFile::setPermissions(filepath, QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther | QFile::permissions(filepath));
    } else {
        QDir dir = p.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    }
}

bool ModelImporter::Run(const QString& mdlPath) {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting standalone Model Import process.");

    QString fullMdlPath = mdlPath;
    fullMdlPath.replace('/', '\\');

    QString relMdlPath = "models\\" + QFileInfo(fullMdlPath).fileName();

    Miscellaneous::Log("Input model path: " + fullMdlPath);
    Miscellaneous::Log("Relative MDL path: " + relMdlPath);

    // Build options for cs_mdl_import
    QString extraOpts;
    const auto& opts = Miscellaneous::GetOptions();
    if (opts.modelSkipAnimation) extraOpts += " -skipcommondmxwrite";
    if (opts.modelChangeBindpose) extraOpts += " -YupToZup";
    if (opts.modelOverrideLean) extraOpts += " -overridelean";
    if (opts.modelHeaderHullBounds) extraOpts += " -header_hull_bounds";
    if (opts.modelImportLods) extraOpts += " -lods";
    if (opts.modelWriteWeaponPrefab) {
        extraOpts += " -write_weapon_anim_prefab";
        QString modelBaseName = QFileInfo(relMdlPath).baseName();
        extraOpts += " -weapon_anim_prefab \"" + modelBaseName + "_prefab\"";
    }

    QString outputDir = opts.s2contentdir + "\\models";
    QString importCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4" + extraOpts + " -o \"" + outputDir + "\" \"" + fullMdlPath + "\"";
    Miscellaneous::RunCommandSync(importCmd);

    if (Miscellaneous::CanceLImport) return false;

    // Define output path for refs file
    QString refsName = outputDir + "\\" + QFileInfo(fullMdlPath).fileName();
    int pos = refsName.lastIndexOf(".mdl");
    if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

    QString outName = outputDir + "\\" + QFileInfo(fullMdlPath).fileName();
    pos = outName.lastIndexOf(".mdl");
    if (pos != -1) outName.replace(pos, 4, ".vmdl");

    QSet<QString> mdlmtls;
    if (QFile::exists(refsName)) {
        QStringList modelRefs = ReadTextFile(refsName);
        for (const QString& ref : modelRefs) {
            QString cleanedRef = CleanRefPath(ref);
            if (!cleanedRef.isEmpty()) {
                cleanedRef.replace('\\', '/');
                mdlmtls.insert(cleanedRef);
            }
        }
    } else {
        Miscellaneous::Log("Warning: refs file not found at " + refsName);
    }

    // Extract, import, and compile materials
    QStringList normalMtls;

    for (const QString& mtlfile : mdlmtls) {
        if (Miscellaneous::CanceLImport) return false;
        if (mtlfile.isEmpty() || mtlfile.startsWith('-')) continue;

        bool isDevOrTool = mtlfile.startsWith("materials/dev/", Qt::CaseInsensitive) || mtlfile.startsWith("materials/tools/", Qt::CaseInsensitive);

        if (isDevOrTool) {
            QString s1GameDirMtl = QDir(opts.s1gamedir).filePath(mtlfile);
            if (!QFile::exists(s1GameDirMtl)) {
                FileExtractFromVPK::ExtractMaterial(mtlfile);
            }

            QString tmpVmtRel = mtlfile;
            if (tmpVmtRel.startsWith("materials/dev/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/dev/", "materials/tmp/dev/", Qt::CaseInsensitive);
            } else if (tmpVmtRel.startsWith("materials/tools/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/tools/", "materials/tmp/tools/", Qt::CaseInsensitive);
            }

            QString origVmtS1 = QDir(opts.s1gamedir).filePath(mtlfile);
            QString tmpVmtS1 = QDir(opts.s1gamedir).filePath(tmpVmtRel);
            QDir().mkpath(QFileInfo(tmpVmtS1).absolutePath());

            if (QFile::exists(tmpVmtS1)) QFile::remove(tmpVmtS1);
            if (QFile::copy(origVmtS1, tmpVmtS1)) {
                QFile::remove(origVmtS1);
            } else {
                continue;
            }

            QString importMtlCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + opts.s1gamedir + "\" -s2addon \"" + opts.addonName + "\" -game csgo \"" + QString(tmpVmtRel).replace('/', '\\') + "\"";
            Miscellaneous::RunCommandSync(importMtlCmd);

            QString tmpVmatRel = tmpVmtRel;
            int vmtPos = tmpVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) tmpVmatRel.replace(vmtPos, 4, ".vmat");
            QString origVmatRel = mtlfile;
            vmtPos = origVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) origVmatRel.replace(vmtPos, 4, ".vmat");

            QString tmpVmatS2 = QDir(opts.s2contentdir).filePath(tmpVmatRel);
            QString origVmatS2 = QDir(opts.s2contentdir).filePath(origVmatRel);

            if (QFile::exists(tmpVmatS2)) {
                QDir().mkpath(QFileInfo(origVmatS2).absolutePath());
                if (QFile::exists(origVmatS2)) QFile::remove(origVmatS2);
                if (QFile::copy(tmpVmatS2, origVmatS2)) {
                    QFile::remove(tmpVmatS2);
                }

                QString resCompCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + QString(origVmatS2).replace('/', '\\') + "\"";
                Miscellaneous::RunCommandSync(resCompCmd);
            }
        } else {
            // Extract from VPK if missing from s1gamedir/s1contentdir
            QString s1gamedirMtl = QDir(opts.s1gamedir).filePath(mtlfile);
            if (!QFileInfo::exists(s1gamedirMtl)) {
                FileExtractFromVPK::ExtractMaterial(mtlfile);
            }
            normalMtls.append(mtlfile);
        }
    }

    if (!normalMtls.isEmpty()) {
        QString tempImportFile = opts.s1contentdir + "/temp_mtl_import.txt";
        EnsureFileWritable(tempImportFile);
        QFile fImport(tempImportFile);
        if (fImport.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fImport);
            out << "importfilelist\n{\n";
            for (const QString& mtl : normalMtls) {
                QString formattedMtl = mtl;
                formattedMtl.replace('\\', '/');
                out << "\t\"file\"\t\"" << formattedMtl << "\"\n";
            }
            out << "}\n";
            fImport.close();
        }

        QString importRefsCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + opts.s1gamedir + "\" -s2addon \"" + opts.addonName + "\" -game csgo -usefilelist \"" + QString(tempImportFile).replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(importRefsCmd);
        QFile::remove(tempImportFile);

        QString tempCompileFile = opts.s1contentdir + "/temp_mtl_compile.txt";
        EnsureFileWritable(tempCompileFile);
        QFile fCompile(tempCompileFile);
        if (fCompile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fCompile);
            for (const QString& mtl : normalMtls) {
                QString outName = opts.s2contentdir + "/" + mtl;
                int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
                if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");
                outName.replace('/', '\\');
                out << outName << "\n";
            }
            fCompile.close();
        }

        QString resCompCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -filelist \"" + QString(tempCompileFile).replace('/', '\\') + "\"";
        Miscellaneous::RunCommandSync(resCompCmd);
        QFile::remove(tempCompileFile);
    }

    if (Miscellaneous::CanceLImport) return false;

    QStringList vmatFilesToFix;
    for (const QString& mtl : mdlmtls) {
        if (mtl.isEmpty() || mtl.startsWith('-')) continue;
        QString mtlOut = opts.s2contentdir + "/" + mtl;
        int vmtPos = mtlOut.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
        if (vmtPos != -1) {
            mtlOut.replace(vmtPos, 4, ".vmat");
        }
        vmatFilesToFix.append(QDir::cleanPath(mtlOut));
    }

    FixModelMaterials(vmatFilesToFix);

    // Check if force compile required
    QSet<QString> global2UVMaterials;
    bool bForceCompile = MaterialFix::Force2UVsIfRequired(refsName, global2UVMaterials);

    if (Miscellaneous::CanceLImport) return false;

    if (QFile::exists(outName)) {
        QString resCompCmd;
        if (bForceCompile) {
            resCompCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -f -game csgo \"" + outName + "\"";
        } else {
            resCompCmd = "\"" + opts.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        }
        Miscellaneous::RunCommandSync(resCompCmd);
    }

    Miscellaneous::Log("Model Import process complete.");
    return true;
}

struct ModelKeyMapping {
    QString newKey;
    bool appendAlpha1;
};

static QMap<QString, ModelKeyMapping> legacyKeyMap = { { "\"$color2\"", { "\"g_vColorTint\"", true } } };

void ModelImporter::FixModelMaterials(const QStringList& vmatFiles) {
    for (const QString& vmatFile : vmatFiles) {
        if (!QFile::exists(vmatFile)) {
            continue;
        }

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
            MaterialFix::ShaderFix(lines, fileModified);
            MaterialFix::ComplexShaderVariablesFix(lines, fileModified);
        }

        MaterialFix::MissingKVFix(lines, fileModified);
        MaterialFix::TranslucentAlphaTestConflictFix(lines, fileModified);

        if (!foundLegacyKeys.isEmpty()) {
            MaterialFix::ColorFix(lines, layer0StartIdx, layer0EndIdx, foundLegacyKeys, fileModified);
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
