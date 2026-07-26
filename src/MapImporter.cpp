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


void MapImporter::ImportAndCompileMapMDLs(const QString& filename) {
    QStringList refs = Miscellaneous::ReadTextFile(filename);
    QStringList mdlfiles;

    for (const QString& ref : refs) {
        if (ref.isEmpty()) continue;
        QString cleanedRef = Miscellaneous::CleanRefPath(ref);
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
        QString mdlfile = Miscellaneous::CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;

        QString infile = QDir::toNativeSeparators(mdlfile);
        QString outName = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/" + mdlfile);
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        QString refsName = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/" + mdlfile);
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        QString program = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/cs_mdl_import.exe");
        QStringList arguments = {
            "-nop4",
            "-i",
            QDir::toNativeSeparators(Miscellaneous::GetOptions().s1gamedir),
            "-o",
            QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir),
            infile
        };
        Miscellaneous::RunCommandSync(program, arguments);

        if (QFile::exists(refsName)) {
            QStringList modelRefs = Miscellaneous::ReadTextFile(refsName);
            for (const QString& ref : modelRefs) {
                QString cleanedRef = Miscellaneous::CleanRefPath(ref);
                if (!cleanedRef.isEmpty()) {
                    cleanedRef = QDir::fromNativeSeparators(cleanedRef);
                    mdlmtls.insert(cleanedRef);
                }
            }
        }
    }

    // Separating dev/tools and normal model materials
    QStringList normalMtls;

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
                QDir::toNativeSeparators(tmpVmtRel)
            };
            Miscellaneous::RunCommandSync(program, arguments);

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

                QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
                QStringList argumentsRc = {
                    "-retail",
                    "-nop4",
                    "-game",
                    "csgo",
                    QDir::toNativeSeparators(origVmatS2)
                };
                Miscellaneous::RunCommandSync(programRc, argumentsRc);
            }
        } else {
            normalMtls.append(mtlfile);
        }
    }

    if (!normalMtls.isEmpty()) {
        QString tempImportFile = Miscellaneous::GetOptions().s1contentdir + "/temp_mtl_import.txt";
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

        QString programS1 = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
        QStringList argumentsS1 = {
            "-retail",
            "-nop4",
            "-nop4sync",
            "-src1gameinfodir",
            Miscellaneous::GetOptions().s1gamedir,
            "-s2addon",
            Miscellaneous::GetOptions().addonName,
            "-game",
            "csgo",
            "-usefilelist",
            QDir::toNativeSeparators(tempImportFile)
        };
        Miscellaneous::RunCommandSync(programS1, argumentsS1);
        QFile::remove(tempImportFile);

        QString tempCompileFile = Miscellaneous::GetOptions().s1contentdir + "/temp_mtl_compile.txt";
        Miscellaneous::EnsureFileWritable(tempCompileFile);
        QFile fCompile(tempCompileFile);
        if (fCompile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fCompile);
            for (const QString& mtl : normalMtls) {
                QString outName = Miscellaneous::GetOptions().s2contentdir + "/" + mtl;
                int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
                if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");
                outName = QDir::toNativeSeparators(outName);
                out << outName << "\n";
            }
            fCompile.close();
        }

        QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
        QStringList argumentsRc = {
            "-retail",
            "-nop4",
            "-game",
            "csgo",
            "-filelist",
            QDir::toNativeSeparators(tempCompileFile)
        };
        Miscellaneous::RunCommandSync(programRc, argumentsRc);
        QFile::remove(tempCompileFile);
    }

    QSet<QString> global2UVMaterials;
    QMap<QString, bool> mdlForceCompile;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = Miscellaneous::CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;

        QString outName = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/" + mdlfile);
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/" + mdlfile);
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = MaterialFix::Force2UVsIfRequired(refsName, global2UVMaterials);
        mdlForceCompile[m] = bForceCompile;
    }

    global2UVMaterials.clear();

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = Miscellaneous::CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;

        QString outName = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/" + mdlfile);
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        bool bForceCompile = mdlForceCompile.value(m, false);

        QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
        QStringList argumentsRc = {
            "-retail",
            "-nop4"
        };
        if (bForceCompile) {
            argumentsRc << "-f";
        }
        argumentsRc << "-game" << "csgo" << QDir::toNativeSeparators(outName);
        Miscellaneous::RunCommandSync(programRc, argumentsRc);
    }
}

void MapImporter::ImportAndCompileMapRefs() {
    QString program = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
    QStringList arguments = {
        "-retail",
        "-nop4",
        "-nop4sync"
    };
    if (Miscellaneous::GetOptions().usebsp) {
        arguments << "-usebsp";
    }
    if (Miscellaneous::GetOptions().usebspNomergeinstances) {
        arguments << "-usebsp_nomergeinstances";
    }

    QString targetS1gamedir = Miscellaneous::GetOptions().csgogamedir;
    arguments << "-src1gameinfodir" << QDir::toNativeSeparators(targetS1gamedir);
    arguments << "-src1contentdir" << QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir);
    arguments << "-s2addon" << Miscellaneous::GetOptions().addonName;
    arguments << "-game" << "csgo";
    arguments << QDir::toNativeSeparators("maps/" + Miscellaneous::GetOptions().mapName + ".vmf");

    // Log the command program and arguments in a clear format
    QString loggedCmd = program;
    for (const QString& arg : arguments) {
        if (arg.contains(' ') || arg.contains('\t') || arg.isEmpty()) {
            loggedCmd += " \"" + arg + "\"";
        } else {
            loggedCmd += " " + arg;
        }
    }
    Miscellaneous::Log("Running mapImportCmd in ImportAndCompileMapRefs: " + loggedCmd);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();

    QString lineBuffer;
    QStringList missingMaterials;

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

    QStringList normalMissing;

    for (QString& vmtPath : missingMaterials) {
        if (Miscellaneous::CanceLImport) return;

        vmtPath = QDir::fromNativeSeparators(vmtPath);

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

            QString programMtl = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
            QStringList argumentsMtl = {
                "-retail",
                "-nop4",
                "-nop4sync",
                "-src1gameinfodir",
                Miscellaneous::GetOptions().s1gamedir,
                "-s2addon",
                Miscellaneous::GetOptions().addonName,
                "-game",
                "csgo",
                QDir::toNativeSeparators(tmpVmtRel)
            };
            Miscellaneous::RunCommandSync(programMtl, argumentsMtl);

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

                QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
                QStringList argumentsRc = {
                    "-retail",
                    "-nop4",
                    "-game",
                    "csgo",
                    QDir::toNativeSeparators(origVmatS2)
                };
                Miscellaneous::RunCommandSync(programRc, argumentsRc);
            }
        } else {
            normalMissing.append(vmtPath);
        }
    }

    if (!normalMissing.isEmpty()) {
        QString tempImportFile = Miscellaneous::GetOptions().s1contentdir + "/temp_missing_import.txt";
        Miscellaneous::EnsureFileWritable(tempImportFile);
        QFile fImport(tempImportFile);
        if (fImport.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fImport);
            out << "importfilelist\n{\n";
            for (const QString& mtl : normalMissing) {
                QString formattedMtl = QDir::fromNativeSeparators(mtl);
                out << "\t\"file\"\t\"" << formattedMtl << "\"\n";
            }
            out << "}\n";
            fImport.close();
        }

        QString programS1 = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
        QStringList argumentsS1 = {
            "-retail",
            "-nop4",
            "-nop4sync",
            "-src1gameinfodir",
            Miscellaneous::GetOptions().s1gamedir,
            "-s2addon",
            Miscellaneous::GetOptions().addonName,
            "-game",
            "csgo",
            "-usefilelist",
            QDir::toNativeSeparators(tempImportFile)
        };
        Miscellaneous::RunCommandSync(programS1, argumentsS1);
        QFile::remove(tempImportFile);

        QString tempCompileFile = Miscellaneous::GetOptions().s1contentdir + "/temp_missing_compile.txt";
        Miscellaneous::EnsureFileWritable(tempCompileFile);
        QFile fCompile(tempCompileFile);
        if (fCompile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&fCompile);
            for (const QString& mtl : normalMissing) {
                QString outName = Miscellaneous::GetOptions().s2contentdir + "/" + mtl;
                int vmtPos = outName.lastIndexOf(".vmt", -1, Qt::CaseInsensitive);
                if (vmtPos != -1) outName.replace(vmtPos, 4, ".vmat");
                outName = QDir::toNativeSeparators(outName);
                out << outName << "\n";
            }
            fCompile.close();
        }

        QString programRc = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/resourcecompiler.exe");
        QStringList argumentsRc = {
            "-retail",
            "-nop4",
            "-game",
            "csgo",
            "-filelist",
            QDir::toNativeSeparators(tempCompileFile)
        };
        Miscellaneous::RunCommandSync(programRc, argumentsRc);
        QFile::remove(tempCompileFile);
    }
}

void MapImporter::ImportParticles(){
    QDir mapsDir(QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir + "/maps"));
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

        QStringList lines = Miscellaneous::ReadTextFile(fileInfo.absoluteFilePath());
        for (const QString& line : lines) {
            if (Miscellaneous::CanceLImport) return;

            QString trimmedLine = line.trimmed();
            if (!trimmedLine.startsWith("file", Qt::CaseInsensitive)) continue;

            QString cleanedPath = trimmedLine.mid(4).trimmed();
            if (cleanedPath.startsWith('"') && cleanedPath.endsWith('"')) {
                cleanedPath = cleanedPath.mid(1, cleanedPath.size() - 2);
            }
            if (cleanedPath.isEmpty()) continue;

            QString fullPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(cleanedPath);
            
            if(!QFile::exists(fullPath)){
                FileExtractFromVPK::ExtractParticle(cleanedPath);
                if(!QFile::exists(fullPath)) continue;
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
                QDir::toNativeSeparators(cleanedPath)
            };
            Miscellaneous::RunCommandSync(program, arguments);
        }
    }
}

void MapImporter::ImportSounds() {
    Miscellaneous::Log("Importing sounds...");
    
    QSet<QString> uniqueSounds;
    SoundscapeImport::ImportSoundscapes(this, uniqueSounds);

    if (!uniqueSounds.isEmpty()) {
        QString soundListFile = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + "_sound_list.txt");
        Miscellaneous::EnsureFileWritable(soundListFile);
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

    QString sourceSoundDir = QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir + "/sound");
    QString destSoundDir = QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/sounds");

    if (QDir(sourceSoundDir).exists()) {
        CopyDirectoryRecursively(sourceSoundDir, destSoundDir);
    }
}

bool MapImporter::Run() {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting Map Import process via C++.");

    QString program = QDir::toNativeSeparators(Miscellaneous::GetOptions().cs2Basefolder + "/game/bin/win64/source1import.exe");
    QStringList arguments = {
        "-retail",
        "-nop4",
        "-nop4sync"
    };
    if (Miscellaneous::GetOptions().usebspNomergeinstances) {
        arguments << "-usebsp_nomergeinstances";
    } else if (Miscellaneous::GetOptions().usebsp) {
        arguments << "-usebsp";
    }
    
    QString targetS1gamedir = Miscellaneous::GetOptions().csgogamedir;
    arguments << "-src1gameinfodir" << QDir::toNativeSeparators(targetS1gamedir);
    arguments << "-src1contentdir" << QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir);
    arguments << "-s2addon" << Miscellaneous::GetOptions().addonName;
    arguments << "-game" << "csgo";
    arguments << QDir::toNativeSeparators("maps/" + Miscellaneous::GetOptions().mapName + ".vmf");

    QString mMapname = Miscellaneous::GetOptions().mapName;
    int pos = mMapname.indexOf("instances");
    if (pos != -1) {
        mMapname.replace(pos, 9, "prefabs");
    }

    if (!Miscellaneous::GetOptions().skipdeps) {
        Miscellaneous::RunCommandSync(program, arguments);

        ImportAndCompileMapMDLs(QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/maps/" + mMapname + "_refs.txt"));
        ImportAndCompileMapRefs();
        ImportParticles();
        ImportSounds();

        MaterialFix::FixMaterials();
    }

    Miscellaneous::RunCommandSync(program, arguments);

    Miscellaneous::Log("Import process complete.");
    return true;
}
