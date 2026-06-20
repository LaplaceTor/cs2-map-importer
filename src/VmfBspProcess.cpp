#include "VmfBspProcess.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>

QString VmfBspProcess::ParseMapversion(const QStringList& lines, bool& found) {
    QString mapversion = "2";
    found = false;
    QRegularExpression mapversion_regex("^\\s*\"mapversion\"\\s+\"([^\"]+)\"");

    for (const QString& line : lines) {
        QRegularExpressionMatch match = mapversion_regex.match(line);
        if (match.hasMatch()) {
            mapversion = match.captured(1);
            found = true;
            break;
        }
    }
    return mapversion;
}

QStringList VmfBspProcess::ExtractVisgroups(const QStringList& lines, QStringList& remainingLines) {
    int visgroups_start_idx = -1;
    int visgroups_end_idx = -1;

    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines[i].trimmed();
        if (trimmed == "visgroups" && visgroups_start_idx == -1) {
            visgroups_start_idx = i;
            break;
        }
    }

    QStringList visgroups_lines;
    remainingLines = lines;

    if (visgroups_start_idx != -1) {
        int open_brackets = 0;
        bool found_first_bracket = false;
        for (int i = visgroups_start_idx; i < lines.size(); ++i) {
            open_brackets += lines[i].count('{');
            open_brackets -= lines[i].count('}');
            if (lines[i].contains('{')) {
                found_first_bracket = true;
            }

            if (found_first_bracket && open_brackets == 0) {
                visgroups_end_idx = i;
                break;
            }
        }

        if (visgroups_end_idx != -1) {
            for (int i = visgroups_start_idx; i <= visgroups_end_idx; ++i) {
                visgroups_lines.append(lines[i]);
            }
            remainingLines.erase(remainingLines.begin() + visgroups_start_idx, remainingLines.begin() + visgroups_end_idx + 1);
        }
    }

    return visgroups_lines;
}

QStringList VmfBspProcess::InsertRequiredBlocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups) {
    bool has_versioninfo = false;
    bool has_viewsettings = false;
    bool has_cordon = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines[i].trimmed();
        if (trimmed == "versioninfo") has_versioninfo = true;
        if (trimmed == "viewsettings") has_viewsettings = true;
        if (trimmed == "cordon") has_cordon = true;
    }

    QStringList out_lines = lines;

    if (!has_versioninfo) {
        QString versioninfo_block = "versioninfo\n{\n\t\"editorversion\" \"400\"\n\t\"editorbuild\" \"9999\"\n\t\"mapversion\" \"" + mapversion + "\"\n\t\"formatversion\" \"100\"\n\t\"prefab\" \"0\"\n}";
        out_lines.insert(0, versioninfo_block);
    }

    if (!visgroups.isEmpty()) {
        int insert_idx = has_versioninfo ? 0 : 1;
        for (int i = 0; i < visgroups.size(); ++i) {
            out_lines.insert(insert_idx + i, visgroups[i]);
        }
    }

    if (!has_viewsettings) {
        QString viewsettings_block = "viewsettings\n{\n\t\"bSnapToGrid\" \"1\"\n\t\"bShowGrid\" \"1\"\n\t\"bShowLogicalGrid\" \"0\"\n\t\"nGridSpacing\" \"64\"\n\t\"bShow3DGrid\" \"0\"\n}";
        int insert_idx = (has_versioninfo ? 0 : 1) + visgroups.size();
        out_lines.insert(insert_idx, viewsettings_block);
    }

    if (!has_cordon) {
        QString cordon_block = "cordon\n{\n\t\"mins\" \"(-1024 -1024 -1024)\"\n\t\"maxs\" \"(1024 1024 1024)\"\n\t\"active\" \"0\"\n}";
        out_lines.append(cordon_block);
    }

    return out_lines;
}

QStringList VmfBspProcess::PatchDispinfo(const QStringList& lines) {
    QStringList out_lines;
    bool in_dispinfo = false;
    int open_brackets_disp = 0;
    bool in_dispinfo_bracket = false;
    bool has_offsets = false;
    bool has_offset_normals = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString l = lines[i];
        QString trimmed = l.trimmed();

        if (trimmed == "dispinfo") {
            in_dispinfo = true;
            open_brackets_disp = 0;
            in_dispinfo_bracket = false;
            has_offsets = false;
            has_offset_normals = false;
        }

        if (in_dispinfo) {
            open_brackets_disp += l.count('{');
            open_brackets_disp -= l.count('}');
            if (!in_dispinfo_bracket && l.contains('{')) {
                in_dispinfo_bracket = true;
            }

            if (trimmed == "offsets") {
                has_offsets = true;
            }
            if (trimmed == "offset_normals") {
                has_offset_normals = true;
            }

            if (in_dispinfo_bracket && open_brackets_disp == 0) {
                in_dispinfo = false;
            }
        }

        if (in_dispinfo && trimmed == "alphas" && (!has_offsets || !has_offset_normals)) {
            QString indent = "";
            for (int j = 0; j < l.size(); ++j) {
                if (l[j] == ' ' || l[j] == '\t') {
                    indent += l[j];
                } else {
                    break;
                }
            }

            if (!has_offsets) {
                QString offsets_block =
                    indent + "offsets\n" +
                    indent + "{\n" +
                    indent + "\t\"row0\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row1\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row2\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row3\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row4\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row5\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row6\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row7\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "\t\"row8\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                    indent + "}";
                out_lines.append(offsets_block);
            }

            if (!has_offset_normals) {
                QString offset_normals_block =
                    indent + "offset_normals\n" +
                    indent + "{\n" +
                    indent + "\t\"row0\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row1\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row2\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row3\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row4\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row5\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row6\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row7\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "\t\"row8\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                    indent + "}";
                out_lines.append(offset_normals_block);
            }
        }

        out_lines.append(l);
    }

    return out_lines;
}

void VmfBspProcess::FixSpecialTargetnames(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    bool has_activator = false;
    bool has_self = false;
    bool has_caller = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        lines.append(line);
        if (line.contains("!activator")) has_activator = true;
        if (line.contains("!self")) has_self = true;
        if (line.contains("!caller")) has_caller = true;
    }
    infile.close();

    if (!has_activator && !has_self && !has_caller) {
        return; // Nothing to do
    }

    Miscellaneous::Log("Found special targetnames in VMF. Fixing...");

    // Find the last entity block
    int last_entity_idx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed() == "entity") {
            last_entity_idx = i;
        }
    }

    int max_id = 0;
    if (last_entity_idx != -1) {
        QRegularExpression id_regex("\"id\"\\s+\"(\\d+)\"");
        for (int i = last_entity_idx; i < lines.size(); ++i) {
            QRegularExpressionMatch match = id_regex.match(lines[i]);
            if (match.hasMatch()) {
                int id = match.captured(1).toInt();
                if (id > max_id) max_id = id;
            }
        }
    } else {
        // If no entity found, default max_id to 100000
        max_id = 100000;
    }

    QStringList entities_to_add;
    if (has_activator) entities_to_add.append("!activator");
    if (has_self) entities_to_add.append("!self");
    if (has_caller) entities_to_add.append("!caller");

    for (const QString& targetname : entities_to_add) {
        max_id++;
        QString entity_block =
            "entity\n"
            "{\n"
            "\t\"id\" \"" + QString::number(max_id) + "\"\n"
            "\t\"classname\" \"info_target\"\n"
            "\t\"angles\" \"0 0 0\"\n"
            "\t\"spawnflags\" \"0\"\n"
            "\t\"targetname\" \"" + targetname + "\"\n"
            "\t\"origin\" \"0 0 0\"\n"
            "}";
        lines.append(entity_block);
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixVmfFromBsp(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool mapversion_found = false;
    QString mapversion = ParseMapversion(lines, mapversion_found);

    if (!mapversion_found) {
        Miscellaneous::Log("No mapversion found in VMF. Aborting fix.");
        return;
    }

    QStringList remainingLines;
    QStringList visgroups_lines = ExtractVisgroups(lines, remainingLines);

    if (visgroups_lines.isEmpty()) {
        Miscellaneous::Log("No visgroups block found in VMF. Aborting fix.");
        return;
    }

    QStringList structured_lines = InsertRequiredBlocks(remainingLines, mapversion, visgroups_lines);
    QStringList out_lines = PatchDispinfo(structured_lines);

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}


void VmfBspProcess::ProcessBsp(Miscellaneous::Options& options) {
    if (Miscellaneous::CanceLImport) return;
    QString appDir = options.appDir;
    QString maps_dir = QDir(appDir).filePath("maps");
    QDir().mkpath(maps_dir);

    QString vmf_dest = QDir(maps_dir).filePath(options.mapName + ".vmf");
    QString bspsrc_jar = QDir(appDir).filePath("bin/bspsrc.jar");

    if (!QFile::exists(bspsrc_jar)) {
        throw AppException("Could not find bspsrc.jar at " + bspsrc_jar);
    }

    Miscellaneous::Log("Decompiling BSP: " + options.bspFile);

    QString decomp_cmd = "java -jar \"" + bspsrc_jar + "\" \"" + options.bspFile + "\" -o \"" + vmf_dest + "\"";
    int ret = Miscellaneous::RunCommandSync(decomp_cmd);
    if (Miscellaneous::CanceLImport) return;
    if (ret != 0) {
        throw AppException("BSP Decompilation failed.");
    }

    QString target_unpacked_dir = QDir(maps_dir).filePath(options.mapName);

    QString vpkeditcli_exe = QDir(appDir).filePath("bin/vpkeditcli.exe");
    vpkeditcli_exe = QDir::toNativeSeparators(vpkeditcli_exe);

    if (!QFile::exists(vpkeditcli_exe)) {
        Miscellaneous::Log("Warning: Could not find vpkeditcli.exe at " + vpkeditcli_exe);
    } else {
        Miscellaneous::Log("Extracting embedded files using vpkeditcli...");
        QString vpk_cmd = "\"" + vpkeditcli_exe + "\" -e \"/\" -o \"" + maps_dir + "\" \"" + options.bspFile + "\"";
        int vpk_ret = Miscellaneous::RunCommandSync(vpk_cmd);
        if (vpk_ret != 0) {
            Miscellaneous::Log("Warning: vpkeditcli failed to extract embedded files.");
        } else {
            Miscellaneous::Log("Successfully extracted embedded files to " + target_unpacked_dir);
        }
    }

    FixVmfFromBsp(vmf_dest);
    if (Miscellaneous::CanceLImport) return;

    QString target_maps_dir = QDir(appDir).filePath("maps/" + options.mapName + "/maps");
    QDir().mkpath(target_maps_dir);
    QString final_vmf_dest = QDir(target_maps_dir).filePath(options.mapName + ".vmf");

    if (QFile::exists(final_vmf_dest)) {
        QFile::remove(final_vmf_dest);
    }

    if (QFile::rename(vmf_dest, final_vmf_dest)) {
        Miscellaneous::Log("Moved VMF to: " + final_vmf_dest);
    } else {
        Miscellaneous::Log("Failed to move VMF to: " + final_vmf_dest);
    }

    options.contentFolder = QDir(appDir).filePath("maps/" + options.mapName);
    Miscellaneous::Log("Decompiled and prepared at: " + final_vmf_dest);

    // Copy materials and models to s1gamedir
    QString s1Subfolder = "csgo";
    if (options.s1GameType == "css") s1Subfolder = "cstrike";
    else if (options.s1GameType == "hl2") s1Subfolder = "hl2";
    else if (options.s1GameType == "l4d") s1Subfolder = "left4dead";
    else if (options.s1GameType == "l4d2") s1Subfolder = "left4dead2";
    else if (options.s1GameType == "portal") s1Subfolder = "portal";
    else if (options.s1GameType == "portal2") s1Subfolder = "portal2";
    else if (options.s1GameType == "tf2") s1Subfolder = "tf";
    else if (options.s1GameType == "gmod") s1Subfolder = "garrysmod";
    QString s1gamedir = QDir(options.s1gameBasefolder).filePath(s1Subfolder);

    if (QDir(target_unpacked_dir).exists()) {
        QString src_materials = QDir(target_unpacked_dir).filePath("materials");
        QString dest_materials = QDir(s1gamedir).filePath("materials");
        if (QDir(src_materials).exists()) {
            Miscellaneous::Log("Copying materials to " + dest_materials);
            CopyDirectoryRecursively(src_materials, dest_materials);
        }

        QString src_models = QDir(target_unpacked_dir).filePath("models");
        QString dest_models = QDir(s1gamedir).filePath("models");
        if (QDir(src_models).exists()) {
            Miscellaneous::Log("Copying models to " + dest_models);
            CopyDirectoryRecursively(src_models, dest_models);
        }

        QString src_particles = QDir(target_unpacked_dir).filePath("particles");
        QString dest_particles = QDir(s1gamedir).filePath("particles");
        if (QDir(src_particles).exists()) {
            Miscellaneous::Log("Copying particles to " + dest_particles);
            CopyDirectoryRecursively(src_particles, dest_particles);
        }
    }
}
