#include "vmf_bsp_process.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>

QString VmfBspProcess::parse_mapversion(const QStringList& lines, bool& found) {
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

QStringList VmfBspProcess::extract_visgroups(const QStringList& lines, QStringList& remaining_lines) {
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
    remaining_lines = lines;

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
            remaining_lines.erase(remaining_lines.begin() + visgroups_start_idx, remaining_lines.begin() + visgroups_end_idx + 1);
        }
    }

    return visgroups_lines;
}

QStringList VmfBspProcess::insert_required_blocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups) {
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

QStringList VmfBspProcess::patch_dispinfo(const QStringList& lines) {
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

void VmfBspProcess::fix_special_targetnames(const QString& vmf_path) {
    if (!QFile::exists(vmf_path)) return;

    QFile infile(vmf_path);
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

    Miscellaneous::log("Found special targetnames in VMF. Fixing...");

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

    QFile outfile(vmf_path);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::fix_vmf_from_bsp(const QString& vmf_path) {
    if (!QFile::exists(vmf_path)) return;

    QFile infile(vmf_path);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool mapversion_found = false;
    QString mapversion = parse_mapversion(lines, mapversion_found);

    if (!mapversion_found) {
        Miscellaneous::log("No mapversion found in VMF. Aborting fix.");
        return;
    }

    QStringList remaining_lines;
    QStringList visgroups_lines = extract_visgroups(lines, remaining_lines);

    if (visgroups_lines.isEmpty()) {
        Miscellaneous::log("No visgroups block found in VMF. Aborting fix.");
        return;
    }

    QStringList structured_lines = insert_required_blocks(remaining_lines, mapversion, visgroups_lines);
    QStringList out_lines = patch_dispinfo(structured_lines);

    QFile outfile(vmf_path);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}


void VmfBspProcess::process_bsp(Miscellaneous::Options& options) {
    if (Miscellaneous::cancel_import) return;
    QString app_dir = options.app_dir;
    QString maps_dir = QDir(app_dir).filePath("maps");
    QDir().mkpath(maps_dir);

    QString vmf_dest = QDir(maps_dir).filePath(options.map_name + ".vmf");
    QString bspsrc_jar = QDir(app_dir).filePath("bin/bspsrc.jar");

    if (!QFile::exists(bspsrc_jar)) {
        throw AppException("Could not find bspsrc.jar at " + bspsrc_jar);
    }

    Miscellaneous::log("Decompiling BSP: " + options.bsp_file);

    QString decomp_cmd = "java -jar \"" + bspsrc_jar + "\" \"" + options.bsp_file + "\" -o \"" + vmf_dest + "\" --unpack_embedded";
    int ret = Miscellaneous::run_command_sync(decomp_cmd);
    if (Miscellaneous::cancel_import) return;
    if (ret != 0) {
        throw AppException("BSP Decompilation failed.");
    }

    QString unpacked_dir;
    QStringList possible_locations = {
        QDir::current().filePath(options.map_name),
        QDir(app_dir).filePath(options.map_name),
        QFileInfo(options.bsp_file).absoluteDir().filePath(options.map_name),
        QDir(maps_dir).filePath(options.map_name)
    };

    for (const QString& loc : possible_locations) {
        if (QDir(loc).exists()) {
            unpacked_dir = loc;
            break;
        }
    }

    QString target_unpacked_dir = QDir(maps_dir).filePath(options.map_name);
    if (!unpacked_dir.isEmpty()) {
        Miscellaneous::log("Found unpacked files at " + unpacked_dir);

        if (unpacked_dir != target_unpacked_dir) {
            if (QDir(target_unpacked_dir).exists()) {
                QDir(target_unpacked_dir).removeRecursively();
            }
            if (QDir().rename(unpacked_dir, target_unpacked_dir)) {
                Miscellaneous::log("Moved unpacked directory to " + target_unpacked_dir);
            } else {
                Miscellaneous::log("Failed to rename unpacked directory to " + target_unpacked_dir + ". Attempting recursive copy...");
                if (copyDirectoryRecursively(unpacked_dir, target_unpacked_dir)) {
                    if (QDir(unpacked_dir).removeRecursively()) {
                        Miscellaneous::log("Successfully copied and removed original unpacked directory.");
                    } else {
                        Miscellaneous::log("Successfully copied but failed to remove original unpacked directory: " + unpacked_dir);
                    }
                } else {
                    Miscellaneous::log("Failed to copy unpacked directory.");
                }
            }
        }
    } else {
        Miscellaneous::log("Could not find unpacked embedded files directory '" + options.map_name + "'");
    }

    fix_vmf_from_bsp(vmf_dest);
    if (Miscellaneous::cancel_import) return;

    QString target_maps_dir = QDir(app_dir).filePath("maps/" + options.map_name + "/maps");
    QDir().mkpath(target_maps_dir);
    QString final_vmf_dest = QDir(target_maps_dir).filePath(options.map_name + ".vmf");

    if (QFile::exists(final_vmf_dest)) {
        QFile::remove(final_vmf_dest);
    }

    if (QFile::rename(vmf_dest, final_vmf_dest)) {
        Miscellaneous::log("Moved VMF to: " + final_vmf_dest);
    } else {
        Miscellaneous::log("Failed to move VMF to: " + final_vmf_dest);
    }

    options.content_folder = QDir(app_dir).filePath("maps/" + options.map_name);
    Miscellaneous::log("Decompiled and prepared at: " + final_vmf_dest);

    // Copy materials and models to s1gamedir
    QString s1_subfolder = (options.s1_game_type == "css") ? "cstrike" : "csgo";
    QString s1gamedir = QDir(options.s1game_basefolder).filePath(s1_subfolder);

    if (QDir(target_unpacked_dir).exists()) {
        QString src_materials = QDir(target_unpacked_dir).filePath("materials");
        QString dest_materials = QDir(s1gamedir).filePath("materials");
        if (QDir(src_materials).exists()) {
            Miscellaneous::log("Copying materials to " + dest_materials);
            copyDirectoryRecursively(src_materials, dest_materials);
        }

        QString src_models = QDir(target_unpacked_dir).filePath("models");
        QString dest_models = QDir(s1gamedir).filePath("models");
        if (QDir(src_models).exists()) {
            Miscellaneous::log("Copying models to " + dest_models);
            copyDirectoryRecursively(src_models, dest_models);
        }
    }
}
