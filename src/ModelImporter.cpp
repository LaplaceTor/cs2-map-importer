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


bool ModelImporter::Run(const QString& mdlPath) {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting standalone Model Import process.");

    QString fullMdlPath = QDir::toNativeSeparators(mdlPath);

    QString relMdlPath = QDir::toNativeSeparators("models/" + QFileInfo(fullMdlPath).fileName());

    Miscellaneous::Log("Input model path: " + fullMdlPath);
    Miscellaneous::Log("Relative MDL path: " + relMdlPath);

    // Build options for cs_mdl_import
    const auto& opts = Miscellaneous::GetOptions();
    QStringList arguments = { "-nop4" };
    if (opts.modelSkipAnimation) arguments << "-skipcommondmxwrite";
    if (opts.modelChangeBindpose) arguments << "-YupToZup";
    if (opts.modelOverrideLean) arguments << "-overridelean";
    if (opts.modelHeaderHullBounds) arguments << "-header_hull_bounds";
    if (opts.modelImportLods) arguments << "-lods";
    if (opts.modelWriteWeaponPrefab) {
        arguments << "-write_weapon_anim_prefab";
        QString modelBaseName = QFileInfo(relMdlPath).baseName();
        arguments << "-weapon_anim_prefab" << (modelBaseName + "_prefab");
    }

    QString outputDir = QDir::toNativeSeparators(opts.s2contentdir + "/models");
    arguments << "-o" << outputDir << fullMdlPath;

    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_CS_MDL_IMPORT, arguments);

    if (Miscellaneous::CanceLImport) return false;

    // Define output path for refs file
    QString refsName = QDir::toNativeSeparators(QDir(outputDir).filePath(QFileInfo(fullMdlPath).fileName()));
    int pos = refsName.lastIndexOf(".mdl");
    if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

    QString outName = QDir::toNativeSeparators(QDir(outputDir).filePath(QFileInfo(fullMdlPath).fileName()));
    pos = outName.lastIndexOf(".mdl");
    if (pos != -1) outName.replace(pos, 4, ".vmdl");

    QSet<QString> mdlmtls;
    if (QFile::exists(refsName)) {
        QStringList modelRefs = Miscellaneous::ReadTextFile(refsName);
        for (const QString& ref : modelRefs) {
            QString cleanedRef = Miscellaneous::CleanRefPath(ref);
            if (!cleanedRef.isEmpty()) {
                cleanedRef = QDir::fromNativeSeparators(cleanedRef);
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

            QStringList arguments = {
                "-retail",
                "-nop4",
                "-nop4sync",
                "-src1gameinfodir",
                opts.s1gamedir,
                "-s2addon",
                opts.addonName,
                "-game",
                "csgo",
                QDir::toNativeSeparators(tmpVmtRel)
            };
            Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, nullptr, false, opts.s1GameType == "csgo");

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

                QStringList argumentsRc = {
                    "-retail",
                    "-nop4",
                    "-game",
                    "csgo",
                    QDir::toNativeSeparators(origVmatS2)
                };
                Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_RESOURCECOMPILER, argumentsRc);
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
        Miscellaneous::EnsureFileWritable(tempImportFile);
        QFile fImport(tempImportFile);
        if (fImport.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fImport);
            out << "importfilelist\n{\n";
            for (const QString& mtl : normalMtls) {
                QString formattedMtl = QDir::fromNativeSeparators(mtl);
                out << "\t\"file\"\t\"" << formattedMtl << "\"\n";
            }
            out << "}\n";
            fImport.close();
        }

        QStringList argumentsS1 = {
            "-retail",
            "-nop4",
            "-nop4sync",
            "-src1gameinfodir",
            opts.s1gamedir,
            "-s2addon",
            opts.addonName,
            "-game",
            "csgo",
            "-usefilelist",
            QDir::toNativeSeparators(tempImportFile)
        };
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, argumentsS1, nullptr, false, opts.s1GameType == "csgo");
        QFile::remove(tempImportFile);

        QString tempCompileFile = opts.s1contentdir + "/temp_mtl_compile.txt";
        Miscellaneous::EnsureFileWritable(tempCompileFile);
        QFile fCompile(tempCompileFile);
        if (fCompile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fCompile);
            for (const QString& mtl : normalMtls) {
                QString outName = opts.s2contentdir + "/" + mtl;
                int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
                if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");
                outName = QDir::toNativeSeparators(outName);
                out << outName << "\n";
            }
            fCompile.close();
        }

        QStringList argumentsRc = {
            "-retail",
            "-nop4",
            "-game",
            "csgo",
            "-filelist",
            QDir::toNativeSeparators(tempCompileFile)
        };
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_RESOURCECOMPILER, argumentsRc);
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
        QStringList argumentsRc = {
            "-retail",
            "-nop4"
        };
        if (bForceCompile) {
            argumentsRc << "-f";
        }
        argumentsRc << "-game" << "csgo" << QDir::toNativeSeparators(outName);
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_RESOURCECOMPILER, argumentsRc);
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
        if (Miscellaneous::CanceLImport) return;
        if (!QFile::exists(vmatFile)) {
            continue;
        }

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
            MaterialFix::ShaderFix(lines, fileModified);
            MaterialFix::ComplexShaderVariablesFix(lines, fileModified);
        }

        MaterialFix::MissingKVFix(lines, fileModified);
        MaterialFix::TranslucentAlphaTestConflictFix(lines, fileModified);

        if (!foundLegacyKeys.isEmpty()) {
            MaterialFix::ColorFix(lines, layer0StartIdx, layer0EndIdx, foundLegacyKeys, fileModified);
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
}
