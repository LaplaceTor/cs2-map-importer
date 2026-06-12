#include "appcore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>

QAtomicInt AppCore::cancel_import(0);

bool AppCore::check_java() {
    QProcess process;
    process.start("java", QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardError() + process.readAllStandardOutput();
    return output.contains("version");
}

void AppCore::move_vpk_signatures(const QString& cs2_basefolder, bool& vpk_signatures_moved) {
    if (cs2_basefolder.isEmpty()) return;

    QString bin_folder = QDir(cs2_basefolder).filePath("game/bin/win64");
    QString vpk_path = QDir(bin_folder).filePath("vpk.signatures");
    QString temp_folder = QDir(bin_folder).filePath("temp");
    QString temp_vpk_path = QDir(temp_folder).filePath("vpk.signatures");

    if (QFile::exists(vpk_path)) {
        if (!QDir(bin_folder).exists("temp")) {
            QDir(bin_folder).mkdir("temp");
        }
        if (QFile::exists(temp_vpk_path)) {
            QFile::remove(temp_vpk_path);
        }
        QFile::rename(vpk_path, temp_vpk_path);
        vpk_signatures_moved = true;
    }
}

void AppCore::restore_vpk_signatures(const QString& cs2_basefolder) {
    if (cs2_basefolder.isEmpty()) return;

    QString bin_folder = QDir(cs2_basefolder).filePath("game/bin/win64");
    QString vpk_path = QDir(bin_folder).filePath("vpk.signatures");
    QString temp_vpk_path = QDir(bin_folder).filePath("temp/vpk.signatures");

    if (QFile::exists(temp_vpk_path)) {
        if (QFile::exists(vpk_path)) {
            QFile::remove(vpk_path);
        }
        QFile::rename(temp_vpk_path, vpk_path);
    }
}

void AppCore::cancel_all() {
    cancel_import = 1;
}

QString AppCore::parse_mapversion(const QStringList& lines, bool& found) {
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

QStringList AppCore::extract_visgroups(const QStringList& lines, QStringList& remaining_lines) {
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

QStringList AppCore::insert_required_blocks(const QStringList& lines, const QString& mapversion, const QStringList& visgroups) {
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

QStringList AppCore::patch_dispinfo(const QStringList& lines) {
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

void AppCore::fix_vmf_from_bsp(const QString& vmf_path, LogCallback log) {
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
        log("No mapversion found in VMF. Aborting fix.");
        return;
    }

    QStringList remaining_lines;
    QStringList visgroups_lines = extract_visgroups(lines, remaining_lines);

    if (visgroups_lines.isEmpty()) {
        log("No visgroups block found in VMF. Aborting fix.");
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

int AppCore::run_command_sync(const QString& cmd, LogCallback logger) {
    if (cancel_import) return -1;
    if (logger) logger(cmd);

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);

    // In QProcess, if we use setNativeArguments, cmd.exe requires the whole string after /c to be quoted if there are inner quotes.
    // However, a simpler cross-platform way is to use QProcess's own parsing if we avoid cmd.exe.
    // Since we need it to behave like CreateProcess, we can just start the command natively if we are not using pipes.
    // For safety with cmd.exe, it expects /c ""command" "arg1" "arg2"". So we wrap in quotes.
    process.setProgram("cmd.exe");
#ifdef Q_OS_WIN
    process.setNativeArguments("/S /C \"" + cmd + "\"");
#else
    process.setArguments({"/c", cmd});
#endif
    process.start();

    QString lineBuffer;
    bool answeredPrompt = false;
    bool isSource1Import = cmd.contains("source1import.exe");
    bool checkingPrompt = false;

    auto processOutput = [&](const QString& outStr) {
        for (QChar c : outStr) {
            if (c == '\n') {
                if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
                if (!lineBuffer.isEmpty() && logger) {
                    logger(lineBuffer);
                }
                if (isSource1Import && !answeredPrompt) {
                    if (lineBuffer.contains("Adding Search Path")) {
                        checkingPrompt = true;
                    } else if (lineBuffer.contains("Building file list...")) {
                        checkingPrompt = false;
                    }
                }
                lineBuffer.clear();
            } else {
                lineBuffer += c;
            }
        }
    };

    while (process.waitForReadyRead(100) || process.state() != QProcess::NotRunning) {
        if (cancel_import) {
            process.kill();
            return -1;
        }
        QByteArray output = process.readAll();
        if (!output.isEmpty()) {
            processOutput(QString(output));
        } else {
            // Timed out waiting for output
            if (isSource1Import && !answeredPrompt && checkingPrompt && process.state() == QProcess::Running) {
                // We're likely stuck at the invisible "Are you sure you want to continue?" prompt
                process.write("y\n");
                answeredPrompt = true;
                checkingPrompt = false; // Stop checking
            }
        }
    }

    QByteArray output = process.readAll();
    if (!output.isEmpty()) {
        processOutput(QString(output));
    }

    if (!lineBuffer.isEmpty() && logger) {
        if (lineBuffer.endsWith('\r')) lineBuffer.chop(1);
        logger(lineBuffer);
    }

    return process.exitCode();
}

bool copyDirectoryRecursively(const QString &sourceDir, const QString &destinationDir) {
    QDir source(sourceDir);
    if (!source.exists()) {
        return false;
    }

    QDir destination(destinationDir);
    if (!destination.exists()) {
        destination.mkpath(destinationDir);
    }

    bool success = true;

    QStringList files = source.entryList(QDir::Files);
    for (const QString &file : files) {
        QString srcPath = source.filePath(file);
        QString dstPath = destination.filePath(file);
        if (QFile::exists(dstPath)) {
            QFile::remove(dstPath);
        }
        if (!QFile::copy(srcPath, dstPath)) {
            success = false;
        }
    }

    QStringList dirs = source.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : dirs) {
        QString srcPath = source.filePath(dir);
        QString dstPath = destination.filePath(dir);
        if (!copyDirectoryRecursively(srcPath, dstPath)) {
            success = false;
        }
    }

    return success;
}

void AppCore::process_bsp(Options& options) {
    if (cancel_import) return;
    QString app_dir = options.app_dir;
    QString maps_dir = QDir(app_dir).filePath("maps");
    QDir().mkpath(maps_dir);

    QString vmf_dest = QDir(maps_dir).filePath(options.map_name + ".vmf");
    QString bspsrc_jar = QDir(app_dir).filePath("bspsrc.jar");

    if (!QFile::exists(bspsrc_jar)) {
        throw AppException("Could not find bspsrc.jar at " + bspsrc_jar);
    }

    options.logger("Decompiling BSP: " + options.bsp_file);

    QString decomp_cmd = "java -jar \"" + bspsrc_jar + "\" \"" + options.bsp_file + "\" -o \"" + vmf_dest + "\" --unpack_embedded";
    int ret = run_command_sync(decomp_cmd, options.logger);
    if (cancel_import) return;
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
        options.logger("Found unpacked files at " + unpacked_dir);

        if (unpacked_dir != target_unpacked_dir) {
            if (QDir(target_unpacked_dir).exists()) {
                QDir(target_unpacked_dir).removeRecursively();
            }
            if (QDir().rename(unpacked_dir, target_unpacked_dir)) {
                options.logger("Moved unpacked directory to " + target_unpacked_dir);
            } else {
                options.logger("Failed to rename unpacked directory to " + target_unpacked_dir + ". Attempting recursive copy...");
                if (copyDirectoryRecursively(unpacked_dir, target_unpacked_dir)) {
                    if (QDir(unpacked_dir).removeRecursively()) {
                        options.logger("Successfully copied and removed original unpacked directory.");
                    } else {
                        options.logger("Successfully copied but failed to remove original unpacked directory: " + unpacked_dir);
                    }
                } else {
                    options.logger("Failed to copy unpacked directory.");
                }
            }
        }
    } else {
        options.logger("Could not find unpacked embedded files directory '" + options.map_name + "'");
    }

    fix_vmf_from_bsp(vmf_dest, options.logger);
    if (cancel_import) return;

    QString target_maps_dir = QDir(app_dir).filePath("maps/" + options.map_name + "/maps");
    QDir().mkpath(target_maps_dir);
    QString final_vmf_dest = QDir(target_maps_dir).filePath(options.map_name + ".vmf");

    if (QFile::exists(final_vmf_dest)) {
        QFile::remove(final_vmf_dest);
    }

    if (QFile::rename(vmf_dest, final_vmf_dest)) {
        options.logger("Moved VMF to: " + final_vmf_dest);
    } else {
        options.logger("Failed to move VMF to: " + final_vmf_dest);
    }

    options.content_folder = QDir(app_dir).filePath("maps/" + options.map_name);
    options.logger("Decompiled and prepared at: " + final_vmf_dest);

    // Copy materials and models to s1gamedir
    QString s1_subfolder = (options.s1_game_type == "css") ? "cstrike" : "csgo";
    QString s1gamedir = QDir(options.s1game_basefolder).filePath(s1_subfolder);

    if (QDir(target_unpacked_dir).exists()) {
        QString src_materials = QDir(target_unpacked_dir).filePath("materials");
        QString dest_materials = QDir(s1gamedir).filePath("materials");
        if (QDir(src_materials).exists()) {
            options.logger("Copying materials to " + dest_materials);
            copyDirectoryRecursively(src_materials, dest_materials);
        }

        QString src_models = QDir(target_unpacked_dir).filePath("models");
        QString dest_models = QDir(s1gamedir).filePath("models");
        if (QDir(src_models).exists()) {
            options.logger("Copying models to " + dest_models);
            copyDirectoryRecursively(src_models, dest_models);
        }
    }
}
