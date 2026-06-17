#include "mapimporter.h"
#include "miscellaneous.h"
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
            QString fullPath = QDir(m_options.s1contentdir).filePath(cleanedRef);
            if (!QFileInfo::exists(fullPath)) {
                ExtractModelFromVPK(cleanedRef);
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

void MapImporter::ExtractModelFromVPK(const QString& filepath) {
    QFileInfo fi(filepath);
    QString basePath = fi.path() + "/" + fi.baseName();
    QString vpkName = (m_options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(m_options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(m_options.s1contentdir).filePath(filepath);
    QString outPath = QDir(m_options.s1gamedir).filePath(filepath);

    QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
    cmd = cmd.replace("/", "\\");
    Miscellaneous::run_command_sync(cmd);

    if (QFile::exists(contentPath)) {
        QFile::copy(contentPath, outPath);

        QStringList extlist = {"vvd","phy","sw.vtx","dx80.vtx","dx90.vtx","ani"};
        for (const QString& ext : extlist) {
            QString target = basePath + "." + ext;
            contentPath = QDir(m_options.s1contentdir).filePath(target);
            cmd = "\"bin\\vpkeditcli.exe\" -e \"" + target + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
            cmd = cmd.replace("/", "\\");
            Miscellaneous::run_command_sync(cmd);

            outPath = QDir(m_options.s1gamedir).filePath(target);
            if (QFile::exists(contentPath)) {
                QFile::copy(contentPath, outPath);
            }
        }
    }
}

void MapImporter::ExtractParticleFromVPK(const QString& filepath) {
    QString vpkName = (m_options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(m_options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(m_options.s1contentdir).filePath(filepath);
    QString outPath = QDir(m_options.s1gamedir).filePath(filepath);

    QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
    cmd = cmd.replace("/", "\\");
    Miscellaneous::run_command_sync(cmd);

    if (QFile::exists(contentPath)) {
        QFile::copy(contentPath, outPath);
    }
}


void MapImporter::ForceUV2ForVMAT(const QString& mtlfile) {
    QString vmat = mtlfile;
    int pos = vmat.lastIndexOf(".vmt");
    if (pos != -1) vmat.replace(pos, 4, ".vmat");

    QString vmatfilename = m_options.s2contentdir + "\\" + vmat;
    if (!QFile::exists(vmatfilename)) return;

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
        Miscellaneous::log("Added F_FORCE_UV2 to " + vmatfilename);
        QFile file(vmatfilename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            for (const QString& l : lines) out << l << "\n";
            file.close();
        }
    }
}

bool MapImporter::Force2UVsIfRequired(const QString& refsName, QSet<QString>& global2UVMaterials, QString& global2UVMaterialsFilepath) {
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
        if (Miscellaneous::cancel_import) return false;
        QString mtlfile = CleanRefPath(refLine);
        if (mtlfile.isEmpty()) continue;
        if (uvsUpdated.contains(mtlfile)) continue;

        if (global2UVMaterials.contains(mtlfile)) {
            b2UV = true;
            uvsUpdated.insert(mtlfile);
        } else {
            if (numuvs == 2) {
                b2UV = true;
                Miscellaneous::log("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                uvsUpdated.insert(mtlfile);

                global2UVMaterials.insert(mtlfile);

                QFile ofs(global2UVMaterialsFilepath);
                if (ofs.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream out(&ofs);
                    out << mtlfile << "\n";
                    ofs.close();
                }
            }
        }
    }
    return b2UV;
}

void MapImporter::ImportAndCompileMapMDLs(const QString& filename) {
    QStringList mdlfiles = ReadTextFile(filename);
    if (mdlfiles.isEmpty()) {
        Miscellaneous::log("No MDLs to import");
        return;
    }

    Miscellaneous::log("Importing models");
    Miscellaneous::log("--------------------------------");
    for (const QString& x : mdlfiles) {
        if (x.isEmpty() || x.startsWith('-')) continue;
        Miscellaneous::log(x);
    }
    Miscellaneous::log("--------------------------------");

    QStringList force2UVList;
    QSet<QString> mdlmtls;
    QString extraoptions = "";

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::cancel_import) return;
        if (m.isEmpty()) continue;
        if (m.startsWith('-')) {
            if (m == "-" || m == "-nooptions") extraoptions = "";
            else extraoptions = m;
        } else {
            QString mdlfile = CleanRefPath(m);
            if (mdlfile.isEmpty()) continue;
            mdlfile.replace('/', '\\');

            QString infile = mdlfile;
            QString outName = m_options.s2contentdir + "\\" + mdlfile;
            int pos = outName.lastIndexOf(".mdl");
            if (pos != -1) outName.replace(pos, 4, ".vmdl");

            QString refsName = m_options.s2contentdir + "\\" + mdlfile;
            pos = refsName.lastIndexOf(".mdl");
            if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

            QString importCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4 " + extraoptions + " -i \"" + m_options.s1gamedir + "\" -o \"" + m_options.s2contentdir + "\" \"" + infile + "\"";
            Miscellaneous::run_command_sync(importCmd);

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
    }

    QString temp_refs = filename;
    int pos = temp_refs.lastIndexOf("mdl_lst");
    if (pos != -1) temp_refs.replace(pos, 7, "mtl_lst");

    EnsureFileWritable(temp_refs);
    QFile fw(temp_refs);
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

    QString importRefsCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + m_options.s1gamedir + "\" -s2addon " + m_options.s2addonname + " -game csgo -usefilelist \"" + temp_refs + "\"";
    Miscellaneous::run_command_sync(importRefsCmd);

    QSet<QString> global2UVMaterials;
    QString global2UVMaterialFilepath = "source1import_2uvmateriallist.txt";
    if (QFile::exists(global2UVMaterialFilepath)) {
        QStringList force2UVListFile = ReadTextFile(global2UVMaterialFilepath);
        for (const QString& mtl : force2UVListFile) {
            global2UVMaterials.insert(mtl);
        }
    }
    EnsureFileWritable(global2UVMaterialFilepath);

    QMap<QString, bool> mdlForceCompile;

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::cancel_import) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = m_options.s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        QString refsName = m_options.s2contentdir + "\\" + mdlfile;
        pos = refsName.lastIndexOf(".mdl");
        if (pos != -1) refsName.replace(pos, 4, "_refs.txt");

        bool bForceCompile = Force2UVsIfRequired(refsName, global2UVMaterials, global2UVMaterialFilepath);
        mdlForceCompile[m] = bForceCompile;
    }

    if (QFile::exists(global2UVMaterialFilepath)) {
        QStringList force2UVListFile = ReadTextFile(global2UVMaterialFilepath);
        for (const QString& mtl : force2UVListFile) {
            ForceUV2ForVMAT(mtl);
        }
    }

    for (const QString& mtlfile : mdlmtls) {
        if (Miscellaneous::cancel_import) return;
        if (mtlfile.isEmpty() || mtlfile.startsWith('-')) continue;
        QString mtl = mtlfile;
        mtl.replace('/', '\\');
        QString outName = m_options.s2contentdir + "\\" + mtl;
        pos = outName.lastIndexOf(".vmt");
        if (pos != -1) outName.replace(pos, 4, ".vmat");

        QString resCompCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        Miscellaneous::run_command_sync(resCompCmd);
    }

    for (const QString& m : mdlfiles) {
        if (Miscellaneous::cancel_import) return;
        if (m.isEmpty() || m.startsWith('-')) continue;
        QString mdlfile = CleanRefPath(m);
        if (mdlfile.isEmpty()) continue;
        mdlfile.replace('/', '\\');

        QString outName = m_options.s2contentdir + "\\" + mdlfile;
        pos = outName.lastIndexOf(".mdl");
        if (pos != -1) outName.replace(pos, 4, ".vmdl");

        if (!QFile::exists(outName)) continue;

        bool bForceCompile = mdlForceCompile.value(m, false);

        QString resCompCmd;
        if (bForceCompile) {
            resCompCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -f -game csgo \"" + outName + "\"";
        } else {
            resCompCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo \"" + outName + "\"";
        }
        Miscellaneous::run_command_sync(resCompCmd);
    }
}

void MapImporter::ImportAndCompileMapRefs(const QString& refsFile) {
    QString importcmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + m_options.s1gamedir + "\" -s2addon " + m_options.s2addonname + " -game csgo -usefilelist \"" + refsFile + "\"";
    Miscellaneous::run_command_sync(importcmd);

    QStringList refs = ReadTextFile(refsFile);
    QString newList = "";

    for (const QString& line : refs) {
        if (Miscellaneous::cancel_import) return;
        QString cleanedRef = CleanRefPath(line);
        if (!cleanedRef.isEmpty()) {
            QString modLine = cleanedRef;
            int pos = modLine.lastIndexOf(".vmt");
            if (pos != -1) modLine.replace(pos, 4, ".vmat");
            modLine.replace(' ', '_');
            modLine.replace('/', '\\');
            newList += m_options.s2contentdir + "\\" + modLine + "\n";
        }
    }

    QString tmpFile = m_options.s2contentdir + "\\maps\\" + m_options.mapname + "_compile_new_refs.txt";
    EnsureFileWritable(tmpFile);
    QFile writeFile(tmpFile);
    if (writeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&writeFile);
        out << newList;
        writeFile.close();
    }

    QString compilercmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game csgo -f -filelist \"" + tmpFile + "\"";
    Miscellaneous::run_command_sync(compilercmd);
}

void MapImporter::ImportParticles(){
    QDir mapsDir(m_options.s1contentdir + "\\maps");
    if (!mapsDir.exists()) {
        return;
    }

    QStringList nameFilters;
    nameFilters << "*_particles.txt";
    QFileInfoList particleFiles = mapsDir.entryInfoList(nameFilters, QDir::Files);

    if (particleFiles.isEmpty()) {
        return;
    }

    Miscellaneous::log("Importing particles...");

    for (const QFileInfo& fileInfo : particleFiles) {
        if (Miscellaneous::cancel_import) return;

        QStringList lines = ReadTextFile(fileInfo.absoluteFilePath());
        for (const QString& line : lines) {
            if (Miscellaneous::cancel_import) return;

            QString trimmedLine = line.trimmed();
            if (!trimmedLine.startsWith("file", Qt::CaseInsensitive)) continue;

            QString cleanedPath = trimmedLine.mid(4).trimmed();
            if (cleanedPath.startsWith('"') && cleanedPath.endsWith('"')) {
                cleanedPath = cleanedPath.mid(1, cleanedPath.length() - 2);
            }
            if (cleanedPath.isEmpty()) continue;

            QString fullPath = QDir(m_options.s1contentdir).filePath(cleanedPath);
            
            if(!QFile::exists(fullPath)){
                ExtractParticleFromVPK(cleanedPath);
                if(!QFile::exists(fullPath)) continue;
            }

            cleanedPath.replace('/', '\\');
            QString importCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"" + m_options.s1gamedir + "\" -s2addon " + m_options.s2addonname + " -game csgo \"" + cleanedPath + "\"";
            Miscellaneous::run_command_sync(importCmd);
        }
    }
}


bool MapImporter::Run() {
    if (Miscellaneous::cancel_import) return false;
    Miscellaneous::log("Starting Map Import process via C++.");

    QString usebspStr = m_options.usebsp ? "-usebsp" : "";
    QString nomergeinstancesStr = m_options.usebsp_nomergeinstances ? "-usebsp_nomergeinstances" : "";

    QString mapImportCmd = "\"" + m_options.cs2_basefolder + "\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync " + usebspStr;
    if (!nomergeinstancesStr.isEmpty()) mapImportCmd += " " + nomergeinstancesStr;

    QString target_s1gamedir = m_options.s1gamedir;
    if (m_options.usebsp || m_options.usebsp_nomergeinstances) {
        target_s1gamedir = m_options.csgogamedir;
    }

    mapImportCmd += " -src1gameinfodir \"" + target_s1gamedir + "\" -src1contentdir \"" + m_options.s1contentdir + "\" -s2addon \"" + m_options.s2addonname + "\" -game csgo maps\\" + m_options.mapname + ".vmf";

    Miscellaneous::run_command_sync(mapImportCmd);

    QString m_mapname = m_options.mapname;
    int pos = m_mapname.indexOf("instances");
    if (pos != -1) {
        m_mapname.replace(pos, 9, "prefabs");
    }

    if (!m_options.skipdeps) {
        StripMDLsFromRefs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_refs.txt");
        ImportAndCompileMapMDLs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_mdl_lst.txt");
        ImportAndCompileMapRefs(m_options.s2contentdir + "\\maps\\" + m_mapname + "_new_refs.txt");
        ImportParticles();
        Miscellaneous::run_command_sync(mapImportCmd);
    }

    Miscellaneous::log("Import process complete.");
    return true;
}
