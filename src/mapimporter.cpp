#include "mapimporter.h"
#include "miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QMap>
#include <memory>

struct VDFNode {
    QString name;
    QString value;
    QList<std::shared_ptr<VDFNode>> children;
};

static QStringList TokenizeVDF(const QStringList& lines) {
    QStringList tokens;
    for (QString line : lines) {
        int commentPos = line.indexOf("//");
        if (commentPos != -1) {
            line = line.left(commentPos);
        }

        QString currentToken = "";
        bool inQuotes = false;

        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];
            if (c == '"') {
                inQuotes = !inQuotes;
                if (!inQuotes) {
                    tokens.append(currentToken);
                    currentToken = "";
                }
            } else if (inQuotes) {
                currentToken += c;
            } else if (c == '{' || c == '}') {
                if (!currentToken.isEmpty()) {
                    tokens.append(currentToken);
                    currentToken = "";
                }
                tokens.append(QString(c));
            } else if (!c.isSpace()) {
                currentToken += c;
            } else {
                if (!currentToken.isEmpty()) {
                    tokens.append(currentToken);
                    currentToken = "";
                }
            }
        }
        if (!currentToken.isEmpty()) {
            tokens.append(currentToken);
        }
    }
    return tokens;
}

static QList<std::shared_ptr<VDFNode>> ParseVDF(const QStringList& tokens, int& index) {
    QList<std::shared_ptr<VDFNode>> nodes;

    while (index < tokens.size()) {
        if (tokens[index] == "}") {
            return nodes;
        }

        auto node = std::make_shared<VDFNode>();
        node->name = tokens[index++];

        if (index < tokens.size()) {
            if (tokens[index] == "{") {
                index++; // skip '{'
                node->children = ParseVDF(tokens, index);
                index++; // skip '}'
            } else {
                node->value = tokens[index++];
            }
        }

        nodes.append(node);
    }

    return nodes;
}

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
        QFileInfo fi(outPath);
        QDir().mkpath(fi.absolutePath());
        if (QFile::exists(outPath)) {
            QFile::remove(outPath);
        }
        QFile::copy(contentPath, outPath);
    }
}

void MapImporter::ExtractSoundFromVPK(const QString& filepath) {
    QString vpkName = (m_options.s1gamename == "css") ? "cstrike_pak_dir.vpk" : "pak01_dir.vpk";
    QString vpkPath = QDir(m_options.s1gamedir).filePath(vpkName);
    QString contentPath = QDir(m_options.s1contentdir).filePath(filepath);
    QString outPath = QDir(m_options.s1gamedir).filePath(filepath);

    QString cmd = "\"bin\\vpkeditcli.exe\" -e \"" + filepath + "\" \"" + vpkPath + "\" -o \"" + contentPath + "\"";
    cmd = cmd.replace("/", "\\");
    Miscellaneous::run_command_sync(cmd);

    if (QFile::exists(contentPath)) {
        QFileInfo fi(outPath);
        QDir().mkpath(fi.absolutePath());
        if (QFile::exists(outPath)) {
            QFile::remove(outPath);
        }
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

static void ParseSoundscapeProperties(const QList<std::shared_ptr<VDFNode>>& children, QString& timeMin, QString& timeMax, QString& pitchMin, QString& pitchMax, QString& volMin, QString& volMax, QString& sndLvl, QStringList& waves) {
    for (auto c : children) {
        QString lowerName = c->name.toLower();
        if (lowerName == "time") {
            QString val = c->value;
            int commaPos = val.indexOf(",");
            if (commaPos != -1) {
                timeMin = val.left(commaPos).trimmed();
                timeMax = val.mid(commaPos + 1).trimmed();
            } else {
                timeMin = timeMax = val.trimmed();
            }
        } else if (lowerName == "pitch") {
            QString val = c->value;
            int commaPos = val.indexOf(",");
            if (commaPos != -1) {
                pitchMin = val.left(commaPos).trimmed();
                pitchMax = val.mid(commaPos + 1).trimmed();
            } else {
                pitchMin = pitchMax = val.trimmed();
            }
        } else if (lowerName == "volume") {
            QString val = c->value;
            int commaPos = val.indexOf(",");
            if (commaPos != -1) {
                volMin = val.left(commaPos).trimmed();
                volMax = val.mid(commaPos + 1).trimmed();
            } else {
                volMin = volMax = val.trimmed();
            }
        } else if (lowerName == "soundlevel") {
            sndLvl = c->value;
        } else if (lowerName == "wave") {
            waves.append(c->value);
        } else if (lowerName == "rndwave") {
            for (auto r : c->children) {
                if (r->name.toLower() == "wave") {
                    waves.append(r->value);
                }
            }
        }
    }
}

static QString FormatVsndPath(QString wavePath) {
    wavePath.replace("\\", "/");
    if (!wavePath.startsWith("sounds/")) {
        if (wavePath.startsWith("sound/")) {
            wavePath.replace(0, 6, "sounds/");
        } else if (wavePath.startsWith("ambient/")) {
            wavePath = "sounds/" + wavePath;
        } else {
            wavePath = "sounds/" + wavePath;
        }
    }
    int dotPos = wavePath.lastIndexOf('.');
    if (dotPos != -1) {
        wavePath = wavePath.left(dotPos) + ".vsnd";
    } else {
        wavePath += ".vsnd";
    }
    return wavePath;
}

void MapImporter::ImportSounds() {
    QDir scriptsDir(m_options.s1contentdir + "\\scripts");
    if (!scriptsDir.exists()) {
        return;
    }

    QStringList nameFilters;
    nameFilters << "soundscapes_*.txt";
    QFileInfoList soundscapeFiles = scriptsDir.entryInfoList(nameFilters, QDir::Files);

    if (!soundscapeFiles.isEmpty()) {
        Miscellaneous::log("Importing sounds...");

        QSet<QString> uniqueSounds;

        for (const QFileInfo& fileInfo : soundscapeFiles) {
            if (Miscellaneous::cancel_import) return;

            QStringList lines = ReadTextFile(fileInfo.absoluteFilePath());
            QStringList tokens = TokenizeVDF(lines);
            int index = 0;
            QList<std::shared_ptr<VDFNode>> roots = ParseVDF(tokens, index);

            QString vsndevtsContent = "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n{\n";

            for (auto root : roots) {
                QString soundscapeName = root->name;
                if (soundscapeName.toLower() == "playlooping" || soundscapeName.toLower() == "playrandom") continue; // should not be at root

                QString parentBlock = QString("\t\"%1\" = \n\t{\n\t\tbase = \"amb.soundscapeParent.base\"\n\t\tenable_child_events = true\n\t\tsoundevent_01 = \n\t\t[\n").arg(soundscapeName);

                QString childrenBlocks = "";
                int partCount = 1;

                for (auto child : root->children) {
                    QString cName = child->name.toLower();
                    if (cName == "playlooping" || cName == "playrandom") {
                        QString partName = QString("%1.part%2").arg(soundscapeName).arg(partCount);
                        parentBlock += QString("\t\t\t\"%1\",\n").arg(partName);

                        QString timeMin, timeMax, pitchMin, pitchMax, volMin, volMax, sndLvl;
                        QStringList waves;

                        ParseSoundscapeProperties(child->children, timeMin, timeMax, pitchMin, pitchMax, volMin, volMax, sndLvl, waves);

                        childrenBlocks += QString("\n\t\"%1\" = \n\t{\n").arg(partName);
                        if (cName == "playlooping") {
                            childrenBlocks += "\t\tbase = \"amb.looping.stereo.base\"\n";
                            if (!volMin.isEmpty()) childrenBlocks += QString("\t\tvolume = %1\n").arg(volMin);
                            if (!pitchMin.isEmpty()) childrenBlocks += QString("\t\tpitch = %1\n").arg(pitchMin);
                        } else {
                            childrenBlocks += "\t\tbase = \"amb.intermittent.randomAroundPlayer.base\"\n";
                            if (!timeMin.isEmpty() && !timeMax.isEmpty()) {
                                childrenBlocks += QString("\t\tretrigger_interval_min = %1\n").arg(timeMin);
                                childrenBlocks += QString("\t\tretrigger_interval_max = %1\n").arg(timeMax);
                            }
                            if (!pitchMin.isEmpty() && !pitchMax.isEmpty()) {
                                childrenBlocks += QString("\t\tpitch_random_min = %1\n").arg(pitchMin);
                                childrenBlocks += QString("\t\tpitch_random_max = %1\n").arg(pitchMax);
                            } else if (!pitchMin.isEmpty()) {
                                childrenBlocks += QString("\t\tpitch = %1\n").arg(pitchMin);
                            }
                            if (!volMin.isEmpty() && !volMax.isEmpty()) {
                                childrenBlocks += QString("\t\tvolume_random_min = %1\n").arg(volMin);
                                childrenBlocks += QString("\t\tvolume_random_max = %1\n").arg(volMax);
                            } else if (!volMin.isEmpty()) {
                                childrenBlocks += QString("\t\tvolume = %1\n").arg(volMin);
                            }
                        }

                        if (waves.size() == 1) {
                            QString vPath = FormatVsndPath(waves[0]);
                            childrenBlocks += QString("\t\tvsnd_files_track_01 = \"%1\"\n").arg(vPath);
                        } else if (waves.size() > 1) {
                            childrenBlocks += "\t\tvsnd_files_track_01 = \n\t\t[\n";
                            for (const QString& w : waves) {
                                childrenBlocks += QString("\t\t\t\"%1\",\n").arg(FormatVsndPath(w));
                            }
                            childrenBlocks += "\t\t]\n";
                        }

                        childrenBlocks += "\t}\n";

                        // collect raw sound for extraction later
                        for (const QString& w : waves) {
                            QString wNorm = w;
                            wNorm.replace("\\", "/");
                            if (!wNorm.startsWith("sound/")) {
                                if (wNorm.startsWith("ambient/")) {
                                    wNorm = "sound/" + wNorm;
                                } else {
                                    wNorm = "sound/" + wNorm;
                                }
                            }
                            uniqueSounds.insert(wNorm);
                        }

                        partCount++;
                    }
                }

                parentBlock += "\t\t]\n\t}\n";

                vsndevtsContent += parentBlock;
                vsndevtsContent += childrenBlocks;
                vsndevtsContent += "\n";
            }

            vsndevtsContent += "\n///////////////////////////////////////////////////////////\n";
            vsndevtsContent += "/////////////// BASE SOUNDEVENT TEMPLATES\n\n";
            vsndevtsContent += "\t\"amb.base\" = \n\t{\n\t\ttype = \"csgo_mega\"\n\t\tmixgroup = \"Amb_Common\"\n\t\tocclusion_intensity = 0.0\n\t\tdistance_effect_mix = 0.0\n\t\trestrict_source_reverb = true\n\t\tuse_distance_unfiltered_stereo_mapping_curve = true\n\t\tuse_time_volume_mapping_curve = true\n\t\tdistance_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, -0.00394, -0.00394,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 0.0, -0.002991, -0.002991,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\tfadetime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, -1.223776, -1.223776,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t0.208571, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\tdistance_unfiltered_stereo_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\ttime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t0.297143, 1.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t}\n";
            vsndevtsContent += "\t\"amb.intermittent.randomAroundPlayer.base\" = \n\t{\n\t\tbase = \"amb.base\"\n\t\tdelay = 5.0\n\t\tvolume = 1.0\n\t\trandomize_position_min_radius = 4000.0\n\t\trandomize_position_max_radius = 4000.0\n\t\trandomize_position_hemisphere = true\n\t\tvolume_random_min = -0.5\n\t\tpitch_random_min = -0.6\n\t\tenable_retrigger = true\n\t\tretrigger_interval_min = 13.0\n\t\tretrigger_interval_max = 35.0\n\t\tretrigger_radius = 10000.0\n\t\tposition_relative_to_player = true\n\t\treverb_wet = 1.0\n\t\tdistance_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, 0.0, 0.0,\n\t\t\t\t0.0, 0.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 1.0, 0.0, 0.0,\n\t\t\t\t0.0, 0.0,\n\t\t\t],\n\t\t]\n\t\tfadetime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, -1.223776, -1.223776,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t0.691429, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\ttime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t1.0, 1.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t}\n";
            vsndevtsContent += "\t\"amb.intermittent.atXYZ.base\" = \n\t{\n\t\tbase = \"amb.base\"\n\t\tposition = [ 0, 0, 0 ]\n\t\tvolume = 1.0\n\t\tvolume_random_min = -0.3\n\t\tvolume_random_max = 0.1\n\t\tpitch_random_min = -0.03\n\t\tpitch_random_max = 0.03\n\t\treverb_wet = 1.0\n\t\tenable_retrigger = true\n\t\tretrigger_interval_min = 1.0\n\t\tretrigger_interval_max = 10.0\n\t\tuse_world_position = false\n\t\tposition_relative_to_player = false\n\t\tdistance_unfiltered_stereo_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\tdistance_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t97.14286, 1.0, -0.001763, -0.001763,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t2000.0, 0.0, -0.000526, -0.000526,\n\t\t\t\t1.0, 1.0,\n\t\t\t],\n\t\t]\n\t}\n";
            vsndevtsContent += "\t\"amb.looping.stereo.base\" = \n\t{\n\t\tbase = \"amb.base\"\n\t\tvolume = 1.0\n\t\tpitch = 1.0\n\t\tdistance_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, 0.0, 0.0,\n\t\t\t\t0.0, 0.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 1.0, 0.0, 0.0,\n\t\t\t\t0.0, 0.0,\n\t\t\t],\n\t\t]\n\t\tfadetime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, -1.223776, -1.223776,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t1.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\ttime_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 0.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t1.0, 1.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t\tdistance_unfiltered_stereo_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t300.0, 1.0, 0.0, 0.0,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t]\n\t}\n";
            vsndevtsContent += "\t\"amb.looping.atXYZ.base\" = \n\t{\n\t\tbase = \"amb.base\"\n\t\tvolume = 1.0\n\t\tposition = [ 1135.96875, 1391.918457, 64.03125 ]\n\t\tuse_world_position = true\n\t\tposition_relative_to_player = false\n\t\treverb_wet = 1.0\n\t\tdistance_volume_mapping_curve = \n\t\t[\n\t\t\t[\n\t\t\t\t0.0, 1.0, -0.001763, -0.001763,\n\t\t\t\t2.0, 3.0,\n\t\t\t],\n\t\t\t[\n\t\t\t\t1500.0, 0.0, -0.000667, -0.000667,\n\t\t\t\t1.0, 1.0,\n\t\t\t],\n\t\t]\n\t}\n";
            vsndevtsContent += "\t\"amb.soundscapeParent.base\" = \n\t{\n\t\tbase = \"amb.base\"\n\t\toverride_dsp_preset = true\n\t\tdsp_preset = \"reverb_22_outsideOpen\"\n\t\tenable_child_events = false\n\t\tset_child_position = false\n\t\tsoundevent_01 = \"\"\n\t}\n";
            vsndevtsContent += "\n}\n";

            QString baseName = fileInfo.baseName(); // soundscapes_xxx
            if (baseName.startsWith("soundscapes_")) {
                baseName.replace(0, 12, "soundevents_");
            } else {
                baseName = "soundevents_" + baseName;
            }

            QString outPath = m_options.s2contentdir + "\\soundevents\\" + baseName + ".vsndevts";
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            EnsureFileWritable(outPath);
            QFile vsndFile(outPath);
            if (vsndFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&vsndFile);
                out << vsndevtsContent;
                vsndFile.close();
            }
        }

        if (!uniqueSounds.isEmpty()) {
            QString soundListFile = m_options.s2contentdir + "\\maps\\" + m_options.mapname + "_sound_list.txt";
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
                if (Miscellaneous::cancel_import) return;

                QString fullPath = QDir(m_options.s1contentdir).filePath(sound);

                if (!QFile::exists(fullPath)) {
                    ExtractSoundFromVPK(sound);
                }
            }
        }
    }

    QString sourceSoundDir = m_options.s1contentdir + "\\sound";
    QString destSoundDir = m_options.s2contentdir + "\\sounds";

    if (QDir(sourceSoundDir).exists()) {
        copyDirectoryRecursively(sourceSoundDir, destSoundDir);
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
        ImportSounds();
        Miscellaneous::run_command_sync(mapImportCmd);
    }

    Miscellaneous::log("Import process complete.");
    return true;
}
