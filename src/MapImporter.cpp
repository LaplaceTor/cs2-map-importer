#include "MapImporter.h"
#include "Miscellaneous.h"
#include "SoundscapeImport.h"
#include "FileExtractFromVPK.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>

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

void MapImporter::StripMDLsFromRefs(const QString& filename) {
    QStringList refs = ReadTextFile(filename);
    QStringList mdls;
    QStringList others;

    for (const QString& ref : refs) {
        if (ref.isEmpty()) continue;
        QString cleanedRef = CleanRefPath(ref);
        if (cleanedRef.isEmpty()) continue;
        QString lowerRef = cleanedRef.toLower();
        if (lowerRef.contains(".mdl")) {
            mdls.append(cleanedRef);
            QString fullPath = QDir(mOptions.s1contentdir).filePath(cleanedRef);
            if (!QFileInfo::exists(fullPath)) {
                FileExtractFromVPK::ExtractModel(cleanedRef, mOptions);
            }
        } else {
            others.append(cleanedRef);
        }
    }

    QString mdlfilename = filename;
    int pos = mdlfilename.lastIndexOf("_refs.txt");
    if (pos != -1) mdlfilename.replace(pos, 9, "_mdl_lst.txt");

    EnsureFileWritable(mdlfilename);
    QFile mdlFile(mdlfilename);
    if (mdlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&mdlFile);
        out << "importfilelist\n{\n";
        for (const QString& m : mdls) out << "\t\"file\"\t\"" << m << "\"\n";
        out << "}\n";
        mdlFile.close();
    }

    QString refsfilename = filename;
    pos = refsfilename.lastIndexOf("_refs.txt");
    if (pos != -1) refsfilename.replace(pos, 9, "_new_refs.txt");

    EnsureFileWritable(refsfilename);
    QFile refFile(refsfilename);
    if (refFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&refFile);
        out << "importfilelist\n{\n";
        for (const QString& o : others) out << "\t\"file\"\t\"" << o << "\"\n";
        out << "}\n";
        refFile.close();
    }
}

bool MapImporter::Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials) {
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

                QString vmatfilename = mOptions.s2contentdir + "\\" + vmat;
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

void MapImporter::ImportAndCompileMapMDLs(const QString& filename) {
    QStringList mdlfiles = ReadTextFile(filename);
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

    QStringList force2UVList;
    QSet<QString> mdlmtls;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty()) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString infile = mdlfile;
        QString outName = mOptions.s2contentdir + "\\" + mdlfile;
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        QString refsName = mOptions.s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        QString importCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4 " + " -i \"" + mOptions.s1gamedir + "\" -o \"" + mOptions.s2contentdir + "\" \"" + infile + "\"";
        Miscellaneous::RunCommandSync(importCmd);

        if (QFile::exists(refsName)) {
            QStringList refs = ReadTextFile(refsName);
            for (const QString& ref : refs) {
                QString cleanedRef = CleanRefPath(ref);
                if (!cleanedRef.isEmpty()) {
                    cleanedRef.replace('\\', '/');
                    mdlmtls.insert(cleanedRef);
                }
            }
            force2UVList.append(refsName);
        }
    }

    QString tempRefs = filename;
    int pos = tempRefs.lastIndexOf("mdl_lst");
    if (pos != -1) tempRefs.replace(pos, 7, "mtl_lst");

    EnsureFileWritable(tempRefs);
    QFile fw(tempRefs);
    if (fw.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&fw);
        out << "importfilelist\n{\n";
        for (const QString& mtl : mdlmtls) {
            QString formattedMtl = mtl;
            formattedMtl.replace('\\', '/');
            out << "\t\"file\"\t\"" << formattedMtl << "\"\n";
        }
        out << "}\n";
        fw.close();
    }

    QString importRefsCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + mOptions.s1gamedir + "\" -s2addon " + mOptions.s2addonname + " -game csgo -usefilelist \"" + tempRefs + "\"";
    Miscellaneous::RunCommandSync(importRefsCmd);

    QSet<QString> global2UVMaterials;

    QMap<QString, bool> mdlForceCompile;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = mOptions.s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = mOptions.s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = Force2UVsIfRequired(refsName, global2UVMaterials);
        mdlForceCompile[m] = bForceCompile;
    }

    global2UVMaterials.clear();

    for (const QString& mtlfile : mdlmtls) {
        if (Miscellaneous::CanceLImport) return;
        if (mtlfile.isEmpty() || mtlfile.startsWith('-')) continue;
        QString mtl = mtlfile;
        mtl.replace('/', '\\');
        QString outName = mOptions.s2contentdir + "\\" + mtl;
        pos = outName.lastIndexOf(".vmt");
        if (pos != -1) outName.replace(pos, 4, ".vmat");

        QString resCompCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        Miscellaneous::RunCommandSync(resCompCmd);
    }

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = mOptions.s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        bool bForceCompile = mdlForceCompile.value(m, false);

        QString resCompCmd;
        if (bForceCompile) {
            resCompCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -f -game csgo \"" + outName + "\"";
        } else {
            resCompCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        }
        Miscellaneous::RunCommandSync(resCompCmd);
    }
}

void MapImporter::ImportAndCompileMapRefs(const QString& refsFile) {
    QString importcmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + mOptions.s1gamedir + "\" -s2addon " + mOptions.s2addonname + " -game csgo -usefilelist \"" + refsFile + "\"";
    Miscellaneous::RunCommandSync(importcmd);

    QStringList refs = ReadTextFile(refsFile);
    QString newList = "";

    for (const QString& line : refs) {
        if (Miscellaneous::CanceLImport) return;
        QString cleanedRef = CleanRefPath(line);
        if (!cleanedRef.isEmpty()) {
            QString modLine = cleanedRef;
            int pos = modLine.lastIndexOf(".vmt");
            if (pos != -1) modLine.replace(pos, 4, ".vmat");
            modLine.replace(' ', '_');
            modLine.replace('/', '\\');
            newList += mOptions.s2contentdir + "\\" + modLine + "\n";
        }
    }

    QString tmpFile = mOptions.s2contentdir + "\\maps\\" + mOptions.mapname + "_compile_new_refs.txt";
    EnsureFileWritable(tmpFile);
    QFile writeFile(tmpFile);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&writeFile);
        out << newList;
        writeFile.close();
    }

    QString compilercmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -f -filelist \"" + tmpFile + "\"";
    Miscellaneous::RunCommandSync(compilercmd);
}

void MapImporter::ImportParticles(){
    QDir mapsDir(mOptions.s1contentdir + "\\maps");
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

            QString fullPath = QDir(mOptions.s1contentdir).filePath(cleanedPath);
            
            if(!QFile::exists(fullPath)){
                FileExtractFromVPK::ExtractParticle(cleanedPath, mOptions);
                if(!QFile::exists(fullPath)) continue;
            }

            cleanedPath.replace('/', '\\');
            QString importCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + mOptions.s1gamedir + "\" -s2addon " + mOptions.s2addonname + " -game csgo \"" + cleanedPath + "\"";
            Miscellaneous::RunCommandSync(importCmd);
        }
    }
}

void MapImporter::ImportSounds() {
    Miscellaneous::Log("Importing sounds...");
    
    QSet<QString> uniqueSounds;
    SoundscapeImport::ImportSoundscapes(this, mOptions, uniqueSounds);

    if (!uniqueSounds.isEmpty()) {
        QString soundListFile = mOptions.s2contentdir + "\\maps\\" + mOptions.mapname + "_sound_list.txt";
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

            QString fullPath = QDir(mOptions.s1contentdir).filePath(sound);

            if (!QFile::exists(fullPath)) {
                FileExtractFromVPK::ExtractSound(sound, mOptions);
            }
        }
    }

    QString sourceSoundDir = mOptions.s1contentdir + "\\sound";
    QString destSoundDir = mOptions.s2contentdir + "\\sounds";

    if (QDir(sourceSoundDir).exists()) {
        CopyDirectoryRecursively(sourceSoundDir, destSoundDir);
    }
}

bool MapImporter::Run() {
    if (Miscellaneous::CanceLImport) return false;
    Miscellaneous::Log("Starting Map Import process via C++.");

    QString usebspStr = mOptions.usebsp ? "-usebsp" : "";
    QString nomergeinstancesStr = mOptions.usebspNomergeinstances ? "-usebsp_nomergeinstances" : "";

    QString mapImportCmd = "\"" + mOptions.cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync " + usebspStr;
    if (!nomergeinstancesStr.isEmpty()) mapImportCmd += " " + nomergeinstancesStr;

    QString targetS1gamedir =  mOptions.csgogamedir;

    mapImportCmd += " -src1gameinfodir \"" + targetS1gamedir + "\" -src1contentdir \"" + mOptions.s1contentdir + "\" -s2addon \"" + mOptions.s2addonname + "\" -game csgo maps\\" + mOptions.mapname + ".vmf";

    Miscellaneous::RunCommandSync(mapImportCmd);

    QString mMapname = mOptions.mapname;
    int pos = mMapname.indexOf("instances");
    if (pos != -1) {
        mMapname.replace(pos, 9, "prefabs");
    }

    if (!mOptions.skipdeps) {
        StripMDLsFromRefs(mOptions.s2contentdir + "\\maps\\" + mMapname + "_refs.txt");
        ImportAndCompileMapMDLs(mOptions.s2contentdir + "\\maps\\" + mMapname + "_mdl_lst.txt");
        ImportAndCompileMapRefs(mOptions.s2contentdir + "\\maps\\" + mMapname + "_new_refs.txt");
        ImportParticles();
        ImportSounds();
        Miscellaneous::RunCommandSync(mapImportCmd);
    }

    Miscellaneous::Log("Import process complete.");
    return true;
}
