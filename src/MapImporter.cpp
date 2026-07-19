#include "MapImporter.h"
#include "Miscellaneous.h"
#include "SoundscapeImport.h"
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

QStringList MapImporter::ReadTextFile(const QString& filepath) {
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

void MapImporter::EnsureFileWritable(const QString& filepath) {
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

void MapImporter::ImportAndCompileMapMDLs(const QString& filename) {
    QStringList refs = ReadTextFile(filename);
    QStringList mdlfiles;

    for (const QString& ref : refs) {
        if (ref.isEmpty()) continue;
        QString cleanedRef = CleanRefPath(ref);
        if (cleanedRef.isEmpty()) continue;
        QString lowerRef = cleanedRef.toLower();
        if (lowerRef.contains(".mdl")) {
            mdlfiles.append(cleanedRef);
            QString fullPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(cleanedRef);
            if (!QFileInfo::exists(fullPath)) {
                FileExtractFromVPK::ExtractModel(cleanedRef);
            }
        }
    }

    if (mdlfiles.isEmpty()) {
        Miscellaneous::Log("No MDLs to import");
        return;
    }

    Miscellaneous::Log("Importing models");
    Miscellaneous::Log("--------------------------------");
    for (const QString& x : mdlfiles) {
        if (x.isEmpty() || x.startsWith('-')) continue;
        Miscellaneous::Log(x);
    }
    Miscellaneous::Log("--------------------------------");

    QSet<QString> mdlmtls;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty()) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString infile = mdlfile;
        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        QString refsName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4 " + " -i \"" + Miscellaneous::GetOptions().s1gamedir + "\" -o \"" + Miscellaneous::GetOptions().s2contentdir + "\" \"" + infile + "\"";
        Miscellaneous::RunCommandSync(importCmd);

        if (QFile::exists(refsName)) {
            QStringList modelRefs = ReadTextFile(refsName);
            for (const QString& ref : modelRefs) {
                QString cleanedRef = CleanRefPath(ref);
                if (!cleanedRef.isEmpty()) {
                    cleanedRef.replace('\\', '/');
                    mdlmtls.insert(cleanedRef);
                }
            }
        }
    }

    // Now import each model material (mtl) in mdlmtls
    for (const QString& mtlfile : mdlmtls) {
        if (Miscellaneous::CanceLImport) return;
        if (mtlfile.isEmpty() || mtlfile.startsWith('-')) continue;

        bool isDevOrTool = mtlfile.startsWith("materials/dev/", Qt::CaseInsensitive) || mtlfile.startsWith("materials/tools/", Qt::CaseInsensitive);

        if (isDevOrTool) {
            QString s1GameDirMtl = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(mtlfile);
            if (!QFile::exists(s1GameDirMtl)) {
                FileExtractFromVPK::ExtractMaterial(mtlfile);
            }

            QString tmpVmtRel = mtlfile;
            if (tmpVmtRel.startsWith("materials/dev/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/dev/", "materials/tmp/dev/", Qt::CaseInsensitive);
            } else if (tmpVmtRel.startsWith("materials/tools/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/tools/", "materials/tmp/tools/", Qt::CaseInsensitive);
            }

            QString origVmtS1 = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(mtlfile);
            QString tmpVmtS1 = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(tmpVmtRel);
            QDir().mkpath(QFileInfo(tmpVmtS1).absolutePath());

            if (QFile::exists(tmpVmtS1)) QFile::remove(tmpVmtS1);
            if (QFile::copy(origVmtS1, tmpVmtS1)) {
                QFile::remove(origVmtS1);
            } else {
                continue;
            }

            QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo \"" + QString(tmpVmtRel).replace('/', '\\') + "\"";
            Miscellaneous::RunCommandSync(importCmd);

            QString tmpVmatRel = tmpVmtRel;
            int vmtPos = tmpVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) tmpVmatRel.replace(vmtPos, 4, ".vmat");
            QString origVmatRel = mtlfile;
            vmtPos = origVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) origVmatRel.replace(vmtPos, 4, ".vmat");

            QString tmpVmatS2 = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(tmpVmatRel);
            QString origVmatS2 = QDir(Miscellaneous::GetOptions().s2contentdir).filePath(origVmatRel);

            if (QFile::exists(tmpVmatS2)) {
                QDir().mkpath(QFileInfo(origVmatS2).absolutePath());
                if (QFile::exists(origVmatS2)) QFile::remove(origVmatS2);
                if (QFile::copy(tmpVmatS2, origVmatS2)) {
                    QFile::remove(tmpVmatS2);
                }

                QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + QString(origVmatS2).replace('/', '\\') + "\"";
                Miscellaneous::RunCommandSync(resCompCmd);
            }
        } else {
            QString formattedMtl = mtlfile;
            formattedMtl.replace('/', '\\');
            QString importRefsCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo \"" + formattedMtl + "\"";
            Miscellaneous::RunCommandSync(importRefsCmd);

            QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + formattedMtl;
            int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");

            QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
            Miscellaneous::RunCommandSync(resCompCmd);
        }
    }

    QSet<QString> global2UVMaterials;
    QMap<QString, bool> mdlForceCompile;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = MaterialFix::Force2UVsIfRequired(refsName, global2UVMaterials);
        mdlForceCompile[m] = bForceCompile;
    }

    global2UVMaterials.clear();

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        bool bForceCompile = mdlForceCompile.value(m, false);

        QString resCompCmd;
        if (bForceCompile) {
            resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -f -game csgo \"" + outName + "\"";
        } else {
            resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        }
        Miscellaneous::RunCommandSync(resCompCmd);
    }
}

void MapImporter::ImportAndCompileMapRefs() {
    QString usebspStr = Miscellaneous::GetOptions().usebsp ? "-usebsp" : "";
    QString nomergeinstancesStr = Miscellaneous::GetOptions().usebspNomergeinstances ? "-usebsp_nomergeinstances" : "";

    QString mapImportCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync " + usebspStr;
    if (!nomergeinstancesStr.isEmpty()) mapImportCmd += " " + nomergeinstancesStr;

    QString targetS1gamedir = Miscellaneous::GetOptions().csgogamedir;

    mapImportCmd += " -src1gameinfodir \"" + targetS1gamedir + "\" -src1contentdir \"" + Miscellaneous::GetOptions().s1contentdir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo maps\\" + Miscellaneous::GetOptions().mapName + ".vmf";

    Miscellaneous::Log("Running mapImportCmd in ImportAndCompileMapRefs: " + mapImportCmd);

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

                    if (lineBuffer.startsWith("Failed loading resource \"materials/")) {
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

        bool isDevOrTool = vmtPath.startsWith("materials/dev/", Qt::CaseInsensitive) || vmtPath.startsWith("materials/tools/", Qt::CaseInsensitive);

        if (isDevOrTool) {
            QString s1GameDirMtl = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(vmtPath);
            if (!QFile::exists(s1GameDirMtl)) {
                FileExtractFromVPK::ExtractMaterial(vmtPath);
            }

            QString tmpVmtRel = vmtPath;
            if (tmpVmtRel.startsWith("materials/dev/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/dev/", "materials/tmp/dev/", Qt::CaseInsensitive);
            } else if (tmpVmtRel.startsWith("materials/tools/", Qt::CaseInsensitive)) {
                tmpVmtRel.replace("materials/tools/", "materials/tmp/tools/", Qt::CaseInsensitive);
            }

            QString origVmtS1 = QDir(Miscellaneous::GetOptions().s1gamedir).filePath(vmtPath);
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
            int vmtPos = tmpVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) tmpVmatRel.replace(vmtPos, 4, ".vmat");
            QString origVmatRel = vmtPath;
            vmtPos = origVmatRel.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) origVmatRel.replace(vmtPos, 4, ".vmat");

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
        } else {
            QString formattedMtl = vmtPath;
            formattedMtl.replace('/', '\\');
            QString importRefsCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo \"" + formattedMtl + "\"";
            Miscellaneous::RunCommandSync(importRefsCmd);

            QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + formattedMtl;
            int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
            if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");

            QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
            Miscellaneous::RunCommandSync(resCompCmd);
        }
    }
}

void MapImporter::ImportParticles(){
    QDir mapsDir(Miscellaneous::GetOptions().s1contentdir + "\\maps");
    if (!mapsDir.exists()) {
        return;
    }

    QStringList nameFilters;
    nameFilters << "*_particles.txt";
    QFileInfoList particleFiles = mapsDir.entryInfoList(nameFilters, QDir::Files);

    if (particleFiles.isEmpty()) {
        return;
    }

    Miscellaneous::Log("Importing particles...");

    for (const QFileInfo& fileInfo : particleFiles) {
        if (Miscellaneous::CanceLImport) return;

        QStringList lines = ReadTextFile(fileInfo.absoluteFilePath());
        for (const QString& line : lines) {
            if (Miscellaneous::CanceLImport) return;

            QString trimmedLine = line.trimmed();
            if (!trimmedLine.startsWith("file", Qt::CaseInsensitive)) continue;

            QString cleanedPath = trimmedLine.mid(4).trimmed();
            if (cleanedPath.startsWith('"') && cleanedPath.endsWith('"')) {
                cleanedPath = cleanedPath.mid(1, cleanedPath.length() - 2);
            }
            if (cleanedPath.isEmpty()) continue;

            QString fullPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(cleanedPath);
            
            if(!QFile::exists(fullPath)){
                FileExtractFromVPK::ExtractParticle(cleanedPath);
                if(!QFile::exists(fullPath)) continue;
            }

            cleanedPath.replace('/', '\\');
            QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon " + Miscellaneous::GetOptions().addonName + " -game csgo \"" + cleanedPath + "\"";
            Miscellaneous::RunCommandSync(importCmd);
        }
    }
}

void MapImporter::ImportSounds() {
    Miscellaneous::Log("Importing sounds...");
    
    QSet<QString> uniqueSounds;
    SoundscapeImport::ImportSoundscapes(this, uniqueSounds);

    if (!uniqueSounds.isEmpty()) {
        QString soundListFile = Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + Miscellaneous::GetOptions().mapName + "_sound_list.txt";
        EnsureFileWritable(soundListFile);
        QFile writeFile(soundListFile);
        if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&writeFile);
            for (const QString& sound : uniqueSounds) {
                out << sound << "\n";
            }
            writeFile.close();
        }

        for (const QString& sound : uniqueSounds) {
            if (Miscellaneous::CanceLImport) return;

            QString fullPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(sound);

            if (!QFile::exists(fullPath)) {
                FileExtractFromVPK::ExtractSound(sound);
            }
        }
    }

    QString sourceSoundDir = Miscellaneous::GetOptions().s1contentdir + "\\sound";
    QString destSoundDir = Miscellaneous::GetOptions().s2contentdir + "\\sounds";

    if (QDir(sourceSoundDir).exists()) {
        CopyDirectoryRecursively(sourceSoundDir, destSoundDir);
    }
}

bool MapImporter::Run() {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting Map Import process via C++.");

    QString usebspStr = Miscellaneous::GetOptions().usebsp ? "-usebsp" : "";
    QString nomergeinstancesStr = Miscellaneous::GetOptions().usebspNomergeinstances ? "-usebsp_nomergeinstances" : "";
    QString mapImportCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync ";

    if (nomergeinstancesStr.isEmpty()){
        mapImportCmd += usebspStr;
    } else {
        mapImportCmd += nomergeinstancesStr;
    }
    
    QString targetS1gamedir =  Miscellaneous::GetOptions().csgogamedir;

    mapImportCmd += " -src1gameinfodir \"" + targetS1gamedir + "\" -src1contentdir \"" + Miscellaneous::GetOptions().s1contentdir + "\" -s2addon \"" + Miscellaneous::GetOptions().addonName + "\" -game csgo maps\\" + Miscellaneous::GetOptions().mapName + ".vmf";

    QString mMapname = Miscellaneous::GetOptions().mapName;
    int pos = mMapname.indexOf("instances");
    if (pos != -1) {
        mMapname.replace(pos, 9, "prefabs");
    }

    if (!Miscellaneous::GetOptions().skipdeps) {
        Miscellaneous::RunCommandSync(mapImportCmd);

        ImportAndCompileMapMDLs(Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + mMapname + "_refs.txt");
        ImportAndCompileMapRefs();
        ImportParticles();
        ImportSounds();

        MaterialFix::FixMaterials();
    }

    Miscellaneous::RunCommandSync(mapImportCmd);

    Miscellaneous::Log("Import process complete.");
    return true;
}
