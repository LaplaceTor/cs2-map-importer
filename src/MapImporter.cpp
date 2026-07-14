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
            QString fullPath = QDir(Miscellaneous::GetOptions().s1contentdir).filePath(cleanedRef);
            if (!QFileInfo::exists(fullPath)) {
                FileExtractFromVPK::ExtractModel(cleanedRef);
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
        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        int pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        QString refsName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        QString importCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4 " + " -i \"" + Miscellaneous::GetOptions().s1gamedir + "\" -o \"" + Miscellaneous::GetOptions().s2contentdir + "\" \"" + infile + "\"";
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

    QString importRefsCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon " + Miscellaneous::GetOptions().addonName + " -game csgo -usefilelist \"" + tempRefs + "\"";
    Miscellaneous::RunCommandSync(importRefsCmd);

    QSet<QString> global2UVMaterials;

    QMap<QString, bool> mdlForceCompile;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = MaterialFix::Force2UVsIfRequired(refsName, global2UVMaterials);
        mdlForceCompile[m] = bForceCompile;
    }

    global2UVMaterials.clear();

    for (const QString& mtlfile : mdlmtls) {
        if (Miscellaneous::CanceLImport) return;
        if (mtlfile.isEmpty() || mtlfile.startsWith('-')) continue;
        QString mtl = mtlfile;
        mtl.replace('/', '\\');
        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mtl;
        pos = outName.lastIndexOf(".vmt");
        if (pos != -1) outName.replace(pos, 4, ".vmat");

        QString resCompCmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        Miscellaneous::RunCommandSync(resCompCmd);
    }

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::CanceLImport) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = Miscellaneous::GetOptions().s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
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

void MapImporter::ImportAndCompileMapRefs(const QString& refsFile) {
    QString importcmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + Miscellaneous::GetOptions().s1gamedir + "\" -s2addon " + Miscellaneous::GetOptions().addonName + " -game csgo -usefilelist \"" + refsFile + "\"";
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
            newList += Miscellaneous::GetOptions().s2contentdir + "\\" + modLine + "\n";
        }
    }

    QString tmpFile = Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + Miscellaneous::GetOptions().mapName + "_compile_new_refs.txt";
    EnsureFileWritable(tmpFile);
    QFile writeFile(tmpFile);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&writeFile);
        out << newList;
        writeFile.close();
    }

    QString compilercmd = "\"" + Miscellaneous::GetOptions().cs2Basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -f -filelist \"" + tmpFile + "\"";
    Miscellaneous::RunCommandSync(compilercmd);
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

        StripMDLsFromRefs(Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + mMapname + "_refs.txt");
        ImportAndCompileMapMDLs(Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + mMapname + "_mdl_lst.txt");
        ImportAndCompileMapRefs(Miscellaneous::GetOptions().s2contentdir + "\\maps\\" + mMapname + "_new_refs.txt");
        MaterialFix::DevTextureFix();
        ImportParticles();
        ImportSounds();

        MaterialFix::FixMaterials();
    }

    Miscellaneous::RunCommandSync(mapImportCmd);

    Miscellaneous::Log("Import process complete.");
    return true;
}
