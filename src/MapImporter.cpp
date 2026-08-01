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
    if (Miscellaneous::CanceLImport) return;
    QStringList refs = Miscellaneous::ReadTextFile(filename);
    QStringList mdlfiles;

    for (const QString& ref : refs) {
        if (Miscellaneous::CanceLImport) return;
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
        if (Miscellaneous::CanceLImport) return;
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

        QStringList arguments = {
            "-nop4",
            "-i",
            QDir::toNativeSeparators(Miscellaneous::GetOptions().s1gamedir),
            "-o",
            QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir),
            infile
        };
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_CS_MDL_IMPORT, arguments);

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
            Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");

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
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, argumentsS1, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");
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
}

void MapImporter::ImportAndCompileMapRefs() {
    if (Miscellaneous::CanceLImport) return;
    QStringList arguments = {
        "-retail",
        "-nop4",
        "-nop4sync",
        "-src1gameinfodir",
        QDir::toNativeSeparators(Miscellaneous::GetOptions().s1gamedir),
        "-src1contentdir",
        QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir),
        "-s2addon",
        Miscellaneous::GetOptions().addonName,
        "-game",
        "csgo",
        QDir::toNativeSeparators("maps/" + Miscellaneous::GetOptions().mapName + ".vmf")
    };

    QStringList outputLines;
    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, true, &outputLines, true, Miscellaneous::GetOptions().s1GameType == "csgo");

    QStringList missingMaterials;
    for (const QString& lineBuffer : outputLines) {
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
            Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, argumentsMtl, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");

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
        Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, argumentsS1, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");
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
}

void MapImporter::ImportParticles(){
    if (Miscellaneous::CanceLImport) return;
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
            Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, false, nullptr, false, Miscellaneous::GetOptions().s1GameType == "csgo");
        }
    }
}

void MapImporter::ImportSounds() {
    if (Miscellaneous::CanceLImport) return;
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

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

namespace {
bool IsCorrectSymlink(const QString& linkPath, const QString& targetPath) {
    DWORD attr = GetFileAttributesW((LPCWSTR)linkPath.utf16());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (!(attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return false;
    }
    HANDLE hFile = CreateFileW(
        (LPCWSTR)linkPath.utf16(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    wchar_t resolvedPath[MAX_PATH];
    DWORD len = GetFinalPathNameByHandleW(hFile, resolvedPath, MAX_PATH, 0);
    CloseHandle(hFile);
    if (len > 0 && len < MAX_PATH) {
        QString resolved = QString::fromWCharArray(resolvedPath);
        if (resolved.startsWith("\\\\?\\")) {
            resolved = resolved.mid(4);
        }
        QString normResolved = QDir::toNativeSeparators(resolved).trimmed();
        QString normTarget = QDir::toNativeSeparators(targetPath).trimmed();
        if (normResolved.endsWith('\\')) normResolved.chop(1);
        if (normTarget.endsWith('\\')) normTarget.chop(1);
        if (normResolved.compare(normTarget, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
}

bool MapImporter::Run() {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting Map Import process.");

    if (!Miscellaneous::GetOptions().skipdeps) {
        if (Miscellaneous::CanceLImport) return false;
        ImportAndCompileMapRefs();
        ImportAndCompileMapMDLs(QDir::toNativeSeparators(Miscellaneous::GetOptions().s2contentdir + "/maps/" + Miscellaneous::GetOptions().mapName + "_refs.txt"));
        ImportParticles();
        ImportSounds();
        MaterialFix::FixMaterials();
    }
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

    if (Miscellaneous::GetOptions().s1GameType.compare("csgo", Qt::CaseInsensitive) != 0) {
        QString s1gameBasefolder = Miscellaneous::GetOptions().s1gameBasefolder;
        QString fakeCsgoPath = QDir::toNativeSeparators(s1gameBasefolder + "/csgo");
        QString s1gamedir = QDir::toNativeSeparators(Miscellaneous::GetOptions().s1gamedir);

        if (IsCorrectSymlink(fakeCsgoPath, s1gamedir)) {
            Miscellaneous::Log("Existing correct symbolic link found at: " + fakeCsgoPath);
            targetS1gamedir = fakeCsgoPath;
        } else {
            DWORD attr = GetFileAttributesW((LPCWSTR)fakeCsgoPath.utf16());
            bool exists = (attr != INVALID_FILE_ATTRIBUTES);

            if (exists) {
                if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
                    Miscellaneous::Log("Removing incorrect symbolic link at: " + fakeCsgoPath);
                    RemoveDirectoryW((LPCWSTR)fakeCsgoPath.utf16());
                } else {
                    QString backupPath = s1gameBasefolder + "/csgo_backup";
                    int backupIdx = 1;
                    while (GetFileAttributesW((LPCWSTR)QDir::toNativeSeparators(backupPath).utf16()) != INVALID_FILE_ATTRIBUTES) {
                        backupPath = s1gameBasefolder + "/csgo_backup_" + QString::number(backupIdx++);
                    }
                    QString nativeBackupPath = QDir::toNativeSeparators(backupPath);

                    QString msgText = QString("The map importer has detected a real folder or file at:\n%1\n\nTo fix texture scale errors, we must create a directory symbolic link (symlink) named 'csgo' pointing to your Source 1 game directory so that the importer can treat it like CS:GO.\n\nSince a real folder already exists, we need to rename it to a backup directory:\n%2\n\nWould you like to proceed with renaming the existing folder and creating the symbolic link?")
                        .arg(fakeCsgoPath)
                        .arg(nativeBackupPath);

                    int btn = MessageBoxW(NULL,
                        (LPCWSTR)msgText.utf16(),
                        L"Action Required: Replace csgo Folder",
                        MB_YESNO | MB_ICONWARNING);
                    if (btn != IDYES) {
                        Miscellaneous::Log("User declined to replace the existing 'csgo' folder. Import process aborted.");
                        return false;
                    }

                    if (!MoveFileW((LPCWSTR)fakeCsgoPath.utf16(), (LPCWSTR)nativeBackupPath.utf16())) {
                        Miscellaneous::Log("Error: Failed to rename existing csgo folder to " + nativeBackupPath);
                        MessageBoxW(NULL,
                            (LPCWSTR)QString("Failed to rename the existing 'csgo' folder to '%1'.\nImport process aborted.").arg(QFileInfo(nativeBackupPath).fileName()).utf16(),
                            L"Error Renaming Folder",
                            MB_OK | MB_ICONERROR);
                        return false;
                    }
                }
            }

            QString msgText = QString("To fix texture scale errors, the map importer needs to create a directory symbolic link (symlink) named 'csgo' pointing to your Source 1 game directory:\n%1\n\nThis will allow the importer to treat the game as CS:GO and import it properly.\n\nCreating symbolic links requires Administrator privileges. Would you like to request administrator permission and create the symlink?").arg(s1gamedir);

            int btn = MessageBoxW(NULL,
                (LPCWSTR)msgText.utf16(),
                L"Administrator Permission Required",
                MB_YESNO | MB_ICONINFORMATION);
            if (btn != IDYES) {
                Miscellaneous::Log("User declined administrator elevation. Import process aborted.");
                return false;
            }

            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.cbSize = sizeof(sei);
            sei.lpVerb = L"runas";
            sei.lpFile = L"cmd.exe";
            QString params = QString("/c mklink /d \"%1\" \"%2\"").arg(fakeCsgoPath).arg(s1gamedir);
            sei.lpParameters = (LPCWSTR)params.utf16();
            sei.nShow = SW_HIDE;
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;

            if (ShellExecuteExW(&sei)) {
                if (sei.hProcess != NULL) {
                    WaitForSingleObject(sei.hProcess, INFINITE);
                    CloseHandle(sei.hProcess);
                }
            }

            if (IsCorrectSymlink(fakeCsgoPath, s1gamedir)) {
                Miscellaneous::Log("Successfully created directory symbolic link: " + fakeCsgoPath + " -> " + s1gamedir);
                targetS1gamedir = fakeCsgoPath;
            } else {
                Miscellaneous::Log("Error: Failed to create symbolic link at " + fakeCsgoPath);
                MessageBoxW(NULL,
                    L"Failed to create the directory symbolic link. Please make sure you have granted Administrator privileges when prompted.\n\nImport process aborted.",
                    L"Error Creating Symbolic Link",
                    MB_OK | MB_ICONERROR);
                return false;
            }
        }
    }

    arguments << "-src1gameinfodir" << QDir::toNativeSeparators(targetS1gamedir);
    arguments << "-src1contentdir" << QDir::toNativeSeparators(Miscellaneous::GetOptions().s1contentdir);
    arguments << "-s2addon" << Miscellaneous::GetOptions().addonName;
    arguments << "-game" << "csgo";
    arguments << QDir::toNativeSeparators("maps/" + Miscellaneous::GetOptions().mapName + ".vmf");
    
    Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_SOURCE1IMPORT, arguments, false, nullptr, true, Miscellaneous::GetOptions().s1GameType == "csgo");

    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Import process complete.");
    return true;
}
