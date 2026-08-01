#include "VmfBspProcess.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QThread>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QByteArray>
#include <QMap>

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
    int current_power = 3;

    for (int i = 0; i < lines.size(); ++i) {
        QString l = lines[i];
        QString trimmed = l.trimmed();

        if (trimmed == "dispinfo") {
            in_dispinfo = true;
            open_brackets_disp = 0;
            in_dispinfo_bracket = false;
            has_offsets = false;
            has_offset_normals = false;
            current_power = 3;
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

            QRegularExpression power_regex("^\"power\"\\s+\"(\\d+)\"");
            QRegularExpressionMatch power_match = power_regex.match(trimmed);
            if (power_match.hasMatch()) {
                current_power = power_match.captured(1).toInt();
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

            int r = (1 << current_power) + 1;
            int m = 3 * r;

            if (!has_offsets) {
                QString offsets_block = indent + "offsets\n" + indent + "{\n";
                for (int row = 0; row < r; ++row) {
                    offsets_block += indent + QString("\t\"row%1\" \"").arg(row);
                    QStringList zeros;
                    for (int j = 0; j < m; ++j) {
                        zeros.append("0");
                    }
                    offsets_block += zeros.join(' ') + "\"\n";
                }
                offsets_block += indent + "}";
                out_lines.append(offsets_block);
            }

            if (!has_offset_normals) {
                QString offset_normals_block = indent + "offset_normals\n" + indent + "{\n";
                for (int row = 0; row < r; ++row) {
                    offset_normals_block += indent + QString("\t\"row%1\" \"").arg(row);
                    QStringList normal_tokens;
                    for (int j = 0; j < r; ++j) {
                        normal_tokens.append("0");
                        normal_tokens.append("0");
                        normal_tokens.append("1");
                    }
                    offset_normals_block += normal_tokens.join(' ') + "\"\n";
                }
                offset_normals_block += indent + "}";
                out_lines.append(offset_normals_block);
            }
        }

        out_lines.append(l);
    }

    return out_lines;
}

void VmfBspProcess::FixLightColor(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    bool is_light = false;
    int colormode_idx = -1;
    QString indent_str = "";

    QStringList out_lines;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            is_light = false;
            colormode_idx = -1;
            indent_str = "";
            out_lines.append(line);
            continue;
        }

        if (in_entity) {
            if (trimmed == "{") {
                bracket_level++;
                out_lines.append(line);
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    if (is_light) {
                        if (colormode_idx == -1) {
                            out_lines.append(indent_str + "\"colormode\" \"0\"");
                        } else {
                            out_lines[colormode_idx] = indent_str + "\"colormode\" \"0\"";
                        }
                    }
                    in_entity = false;
                    out_lines.append(line);
                } else {
                    out_lines.append(line);
                }
            } else {
                if (bracket_level == 1) {
                    QRegularExpression classname_regex("^\"classname\"\\s+\"(light|light_spot)\"$");
                    if (classname_regex.match(trimmed).hasMatch()) {
                        is_light = true;
                    }
                    QRegularExpression colormode_regex("^\"colormode\"\\s+\"(.*)\"$");
                    QRegularExpressionMatch match = colormode_regex.match(trimmed);
                    if (match.hasMatch()) {
                        colormode_idx = out_lines.size(); // Index in out_lines where it will be appended
                    }

                    if (trimmed != "" && indent_str == "") {
                        indent_str = line.left(line.indexOf(trimmed));
                    }
                }
                out_lines.append(line);
            }
        } else {
            out_lines.append(line);
        }
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixPhysboxMultiplayer(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    QStringList current_entity_block;
    QStringList out_lines;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            current_entity_block.clear();
            current_entity_block.append(line);
        } else if (in_entity) {
            current_entity_block.append(line);

            if (trimmed == "{") {
                bracket_level++;
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    in_entity = false;

                    QRegularExpression classname_regex("^(\\s*)\"classname\"\\s+\"([^\"]+)\"(.*)$");

                    for (int j = 0; j < current_entity_block.size(); ++j) {
                        QRegularExpressionMatch match = classname_regex.match(current_entity_block[j]);
                        if (match.hasMatch()) {
                            QString indent = match.captured(1);
                            QString classname_val = match.captured(2);
                            QString rest = match.captured(3);

                            if (classname_val == "func_physbox_multiplayer") {
                                current_entity_block[j] = indent + "\"classname\" \"func_physbox\"" + rest;
                            }
                            break;
                        }
                    }

                    out_lines.append(current_entity_block);
                }
            }
        } else {
            out_lines.append(line);
        }
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixPerformanceMode(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    QStringList out_lines;
    QRegularExpression pm_regex("^(\\s*\"PerformanceMode\"\\s+\")([^\"]+)(\".*)$");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QRegularExpressionMatch pm_match = pm_regex.match(line);
        if (pm_match.hasMatch()) {
            QString val = pm_match.captured(2);
            QString new_val;
            if (val == "0" || val == "2") {
                new_val = "PM_NORMAL";
            } else if (val == "1" || val == "3") {
                new_val = "PM_NO_GIBS";
            } else {
                new_val = val; // Keep as is if not matched
            }
            line = pm_match.captured(1) + new_val + pm_match.captured(3);
        }
        out_lines.append(line);
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixBrush(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    QStringList entity_lines;
    QStringList out_lines;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            entity_lines.clear();
            entity_lines.append(line);
            continue;
        }

        if (in_entity) {
            entity_lines.append(line);
            if (trimmed == "{") {
                bracket_level++;
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    in_entity = false;

                    int inner_level = 0;
                    QString classname = "";
                    QString solid_val = "";
                    QString base_indent = "";

                    for (int j = 0; j < entity_lines.size(); ++j) {
                        QString eline = entity_lines[j];
                        QString etrimmed = eline.trimmed();

                        if (etrimmed == "{") {
                            inner_level++;
                        } else if (etrimmed == "}") {
                            inner_level--;
                        } else if (inner_level == 1) {
                            QRegularExpression classname_regex("^\"classname\"\\s+\"(.*)\"$");
                            QRegularExpressionMatch c_match = classname_regex.match(etrimmed);
                            if (c_match.hasMatch()) {
                                classname = c_match.captured(1);
                                base_indent = eline.left(eline.indexOf(etrimmed));
                            }

                            QRegularExpression solid_regex("^\"solid\"\\s+\"(.*)\"$");
                            QRegularExpressionMatch s_match = solid_regex.match(etrimmed);
                            if (s_match.hasMatch()) {
                                solid_val = s_match.captured(1);
                            }
                        }
                    }

                    bool process_func_detail = Miscellaneous::GetOptions().keepFuncDetailAsBrush && classname == "func_detail";

                    if (classname == "func_illusionary" || classname == "func_wall" || classname == "func_wall_toggle" || classname == "func_lod" || process_func_detail) {
                        QStringList new_entity_lines;
                        inner_level = 0;
                        for (int j = 0; j < entity_lines.size(); ++j) {
                            QString eline = entity_lines[j];
                            QString etrimmed = eline.trimmed();

                            if (etrimmed == "{") {
                                inner_level++;
                                new_entity_lines.append(eline);
                            } else if (etrimmed == "}") {
                                inner_level--;
                                new_entity_lines.append(eline);
                            } else if (inner_level == 1) {
                                QRegularExpression classname_regex("^\"classname\"\\s+\"(.*)\"$");
                                QRegularExpression solid_regex("^\"solid\"\\s+\"(.*)\"$");

                                if (classname_regex.match(etrimmed).hasMatch()) {
                                    new_entity_lines.append(base_indent + "\"classname\" \"func_brush\"");
                                    new_entity_lines.append(base_indent + "\"InputFilter\" \"32\"");

                                    QString solidity_val = "2";
                                    if (classname == "func_illusionary") {
                                        solidity_val = "1";
                                    } else if (classname == "func_wall") {
                                        solidity_val = "2";
                                    } else if (classname == "func_wall_toggle") {
                                        solidity_val = "0";
                                    } else if (classname == "func_detail") {
                                        solidity_val = "2";
                                    } else if (classname == "func_lod") {
                                        if (solid_val == "0") {
                                            solidity_val = "2";
                                        } else if (solid_val == "1") {
                                            solidity_val = "1";
                                        } else {
                                            solidity_val = "2";
                                        }
                                    }
                                    new_entity_lines.append(base_indent + "\"Solidity\" \"" + solidity_val + "\"");
                                } else if (classname == "func_lod" && solid_regex.match(etrimmed).hasMatch()) {
                                    // Remove the original solid key for func_lod
                                } else {
                                    new_entity_lines.append(eline);
                                }
                            } else {
                                new_entity_lines.append(eline);
                            }
                        }
                        out_lines.append(new_entity_lines);
                    } else {
                        out_lines.append(entity_lines);
                    }
                }
            }
        } else {
            out_lines.append(line);
        }
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixRender(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    QMap<QString, QString> renderfxMap = {
        {"0", "kRenderFxNone"},
        {"1", "kRenderFxPulseSlow"},
        {"2", "kRenderFxPulseFast"},
        {"3", "kRenderFxPulseSlowWide"},
        {"4", "kRenderFxPulseFastWide"},
        {"5", "kRenderFxFadeSlow"},
        {"6", "kRenderFxFadeFast"},
        {"7", "kRenderFxSolidSlow"},
        {"8", "kRenderFxSolidFast"},
        {"9", "kRenderFxStrobeSlow"},
        {"10", "kRenderFxStrobeFast"},
        {"11", "kRenderFxStrobeFaster"},
        {"12", "kRenderFxFlickerSlow"},
        {"13", "kRenderFxFlickerFast"},
        {"14", "kRenderFxNoDissipation"}
    };

    QMap<QString, QString> rendermodeMap = {
        {"0", "kRenderNormal"},
        {"1", "kRenderTransColor"},
        {"2", "kRenderTransTexture"},
        {"3", "kRenderGlow"},
        {"4", "kRenderTransAlpha"},
        {"5", "kRenderTransAdd"},
        {"7", "kRenderTransAddFrameBlend"},
        {"9", "kRenderWorldGlow"},
        {"10", "kRenderNone"}
    };

    QStringList out_lines;
    QRegularExpression renderfx_regex("^(\\s*\"renderfx\"\\s+\")([^\"]+)(\".*)$");
    QRegularExpression rendermode_regex("^(\\s*\"rendermode\"\\s+\")([^\"]+)(\".*)$");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QRegularExpressionMatch fx_match = renderfx_regex.match(line);
        if (fx_match.hasMatch()) {
            QString val = fx_match.captured(2);
            if (renderfxMap.contains(val)) {
                line = fx_match.captured(1) + renderfxMap[val] + fx_match.captured(3);
            }
        } else {
            QRegularExpressionMatch mode_match = rendermode_regex.match(line);
            if (mode_match.hasMatch()) {
                QString val = mode_match.captured(2);
                if (rendermodeMap.contains(val)) {
                    line = mode_match.captured(1) + rendermodeMap[val] + mode_match.captured(3);
                }
            }
        }
        out_lines.append(line);
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixDynamicProp(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    QStringList current_entity_block;
    QStringList out_lines;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            current_entity_block.clear();
            current_entity_block.append(line);
        } else if (in_entity) {
            current_entity_block.append(line);

            if (trimmed == "{") {
                bracket_level++;
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    in_entity = false;

                    // Check if it is a prop_dynamic, prop_dynamic_glow, or prop_dynamic_override
                    bool is_prop_dynamic = false;
                    bool is_prop_dynamic_glow = false;
                    bool is_prop_dynamic_override = false;
                    int classname_idx = -1;
                    QRegularExpression classname_regex("^(\\s*\"classname\"\\s+\")([^\"]+)(\".*)$");

                    for (int j = 0; j < current_entity_block.size(); ++j) {
                        QRegularExpressionMatch match = classname_regex.match(current_entity_block[j]);
                        if (match.hasMatch()) {
                            classname_idx = j;
                            QString current_classname = match.captured(2);
                            if (current_classname == "prop_dynamic") {
                                is_prop_dynamic = true;
                            } else if (current_classname == "prop_dynamic_glow") {
                                is_prop_dynamic_glow = true;
                            } else if (current_classname == "prop_dynamic_override") {
                                is_prop_dynamic_override = true;
                            }
                            break;
                        }
                    }

                    if (is_prop_dynamic || is_prop_dynamic_glow || is_prop_dynamic_override) {
                        QRegularExpression default_anim_regex("^(\\s*)\"DefaultAnim\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression hold_animation_regex("^(\\s*)\"HoldAnimation\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression random_animation_regex("^(\\s*)\"RandomAnimation\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression animate_every_frame_regex("^(\\s*)\"AnimateEveryFrame\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression glowdist_regex("^(\\s*)\"glowdist\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression glowenabled_regex("^(\\s*)\"glowenabled\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression min_anim_time_regex("^(\\s*)\"MinAnimTime\"\\s+\"([^\"]*)\"(.*)$");
                        QRegularExpression max_anim_time_regex("^(\\s*)\"MaxAnimTime\"\\s+\"([^\"]*)\"(.*)$");

                        bool has_hold_animation = false;
                        for (const QString& block_line : current_entity_block) {
                            if (hold_animation_regex.match(block_line).hasMatch()) {
                                has_hold_animation = true;
                                break;
                            }
                        }

                        QStringList new_entity_block;
                        for (const QString& block_line : current_entity_block) {
                            QRegularExpressionMatch classname_match = classname_regex.match(block_line);
                            QRegularExpressionMatch default_anim_match = default_anim_regex.match(block_line);
                            QRegularExpressionMatch hold_animation_match = hold_animation_regex.match(block_line);
                            QRegularExpressionMatch random_animation_match = random_animation_regex.match(block_line);
                            QRegularExpressionMatch animate_every_frame_match = animate_every_frame_regex.match(block_line);
                            QRegularExpressionMatch glowdist_match = glowdist_regex.match(block_line);
                            QRegularExpressionMatch glowenabled_match = glowenabled_regex.match(block_line);
                            QRegularExpressionMatch min_anim_time_match = min_anim_time_regex.match(block_line);
                            QRegularExpressionMatch max_anim_time_match = max_anim_time_regex.match(block_line);

                            if (classname_match.hasMatch()) {
                                QString indent = classname_match.captured(1);
                                QString current_classname = classname_match.captured(2);
                                QString rest = classname_match.captured(3);

                                if (current_classname == "prop_dynamic_glow") {
                                    new_entity_block.append(indent + "prop_dynamic" + rest);
                                } else {
                                    new_entity_block.append(block_line);
                                }
                            } else if (default_anim_match.hasMatch()) {
                                QString indent = default_anim_match.captured(1);
                                QString default_anim_val = default_anim_match.captured(2);
                                QString rest = default_anim_match.captured(3);

                                if (!default_anim_val.isEmpty()) {
                                    new_entity_block.append(indent + "\"IdleAnim\" \"" + default_anim_val + "\"" + rest);
                                    if (!has_hold_animation) {
                                        new_entity_block.append(indent + "\"IdleAnimationLoopMode\" \"ANIM_LOOP_MODE_NOT_LOOPING\"");
                                    }
                                }
                                // If empty, do not write anything
                            } else if (hold_animation_match.hasMatch()) {
                                QString indent = hold_animation_match.captured(1);
                                QString hold_val = hold_animation_match.captured(2);
                                QString rest = hold_animation_match.captured(3);

                                if (hold_val == "1") {
                                    new_entity_block.append(indent + "\"IdleAnimationLoopMode\" \"ANIM_LOOP_MODE_LOOPING\"" + rest);
                                } else {
                                    new_entity_block.append(indent + "\"IdleAnimationLoopMode\" \"ANIM_LOOP_MODE_NOT_LOOPING\"" + rest);
                                }
                            } else if (random_animation_match.hasMatch()) {
                                QString indent = random_animation_match.captured(1);
                                QString val = random_animation_match.captured(2);
                                QString rest = random_animation_match.captured(3);

                                new_entity_block.append(indent + "\"randomizecycle\" \"" + val + "\"" + rest);
                            } else if (animate_every_frame_match.hasMatch()) {
                                QString indent = animate_every_frame_match.captured(1);
                                QString val = animate_every_frame_match.captured(2);
                                QString rest = animate_every_frame_match.captured(3);

                                new_entity_block.append(indent + "\"AnimateOnServer\" \"" + val + "\"" + rest);
                            } else if (glowdist_match.hasMatch()) {
                                QString indent = glowdist_match.captured(1);
                                QString val = glowdist_match.captured(2);
                                QString rest = glowdist_match.captured(3);

                                new_entity_block.append(indent + "\"glowrange\" \"" + val + "\"" + rest);
                            } else if (glowenabled_match.hasMatch()) {
                                QString indent = glowenabled_match.captured(1);
                                QString val = glowenabled_match.captured(2);
                                QString rest = glowenabled_match.captured(3);

                                if (val == "1") {
                                    new_entity_block.append(indent + "\"glowstate\" \"3\"" + rest);
                                } else {
                                    new_entity_block.append(indent + "\"glowstate\" \"0\"" + rest);
                                }
                            } else if (min_anim_time_match.hasMatch() || max_anim_time_match.hasMatch()) {
                                // Remove MinAnimTime and MaxAnimTime (skip)
                            } else {
                                new_entity_block.append(block_line);
                            }
                        }
                        current_entity_block = new_entity_block;
                    }

                    out_lines.append(current_entity_block);
                }
            }
        } else {
            out_lines.append(line);
        }
    }

    // Write back to file
    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::SkinKVFix(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    QStringList current_entity_block;
    QStringList out_lines;

    QRegularExpression skin_regex("^(\\s*)\"skin\"\\s+\"([^\"]*)\"(.*)$");

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            current_entity_block.clear();
            current_entity_block.append(line);
        } else if (in_entity) {
            current_entity_block.append(line);

            if (trimmed == "{") {
                bracket_level++;
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    in_entity = false;

                    QStringList new_entity_block;
                    for (const QString& block_line : current_entity_block) {
                        QRegularExpressionMatch skin_match = skin_regex.match(block_line);
                        if (skin_match.hasMatch()) {
                            QString indent = skin_match.captured(1);
                            QString val = skin_match.captured(2);
                            QString rest = skin_match.captured(3);

                            if (val == "0") {
                                new_entity_block.append(indent + "\"skin\" \"default\"" + rest);
                            } else {
                                new_entity_block.append(block_line);
                            }
                        } else {
                            new_entity_block.append(block_line);
                        }
                    }
                    out_lines.append(new_entity_block);
                }
            }
        } else {
            out_lines.append(line);
        }
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}

void VmfBspProcess::FixEntities(const QString& vmfPath) {
    FixVmfFromBsp(vmfPath);
    FixSpecialTargetnames(vmfPath);
    FixLightColor(vmfPath);
    FixBrush(vmfPath);
    FixRender(vmfPath);
    FixDynamicProp(vmfPath);
    SkinKVFix(vmfPath);
    FixPerformanceMode(vmfPath);
    OldParticleFix(vmfPath);
    FixPhysboxMultiplayer(vmfPath);
    RemoveSkipAndHintSolids(vmfPath);
}

void VmfBspProcess::OldParticleFix(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    bool in_entity = false;
    int bracket_level = 0;
    QStringList current_entity_block;
    QStringList out_lines;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (!in_entity && trimmed == "entity") {
            in_entity = true;
            bracket_level = 0;
            current_entity_block.clear();
            current_entity_block.append(line);
        } else if (in_entity) {
            current_entity_block.append(line);

            if (trimmed == "{") {
                bracket_level++;
            } else if (trimmed == "}") {
                bracket_level--;
                if (bracket_level == 0) {
                    in_entity = false;

                    QRegularExpression classname_regex("^(\\s*)\"classname\"\\s+\"([^\"]+)\"(.*)$");

                    for (int j = 0; j < current_entity_block.size(); ++j) {
                        QRegularExpressionMatch match = classname_regex.match(current_entity_block[j]);
                        if (match.hasMatch()) {
                            QString indent = match.captured(1);
                            QString classname_val = match.captured(2);
                            QString rest = match.captured(3);

                            if (classname_val == "env_lightglow") {
                                current_entity_block[j] = indent + "\"classname\" \"env_sprite\"" + rest;
                            } else if (classname_val == "env_sprite_clientside") {
                                current_entity_block[j] = indent + "\"classname\" \"env_sprite\"" + rest;
                                current_entity_block.insert(j + 1, indent + "\"clientSideEntity\" \"1\"");
                            }
                            break;
                        }
                    }

                    out_lines.append(current_entity_block);
                }
            }
        } else {
            out_lines.append(line);
        }
    }

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
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


void VmfBspProcess::ProcessBsp() {
    if (Miscellaneous::CanceLImport) return;
    QString appDir = Miscellaneous::GetOptions().appDir;
    QString maps_dir = QDir(appDir).filePath("maps");
    QDir().mkpath(maps_dir);

    QString vmf_dest = QDir(maps_dir).filePath(Miscellaneous::GetOptions().mapName + ".vmf");

    Miscellaneous::Log("Decompiling BSP: " + Miscellaneous::GetOptions().bspFile);

    QStringList arguments = {
        QDir::toNativeSeparators(Miscellaneous::GetOptions().bspFile),
        "-o",
        QDir::toNativeSeparators(vmf_dest)
    };
    int ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_BSPSRC, arguments);
    if (Miscellaneous::CanceLImport) return;
    if (ret != 0) {
        throw AppException("BSP Decompilation failed.");
    }

    if (!QFile::exists(vmf_dest)) {
        throw AppException("BSP Decompilation failed: Decompiled VMF file was not created.");
    }

    QString target_unpacked_dir = QDir(maps_dir).filePath(Miscellaneous::GetOptions().mapName);

    if (!Miscellaneous::GetOptions().skipdeps) {
        QString vpkeditcli_exe = QDir(appDir).filePath("bin/vpkeditcli.exe");
        vpkeditcli_exe = QDir::toNativeSeparators(vpkeditcli_exe);

        if (!QFile::exists(vpkeditcli_exe)) {
            Miscellaneous::Log("Warning: Could not find vpkeditcli.exe at " + vpkeditcli_exe);
        } else {
            Miscellaneous::Log("Extracting embedded files using vpkeditcli...");
            QStringList argumentsVpk = {
                "-e",
                "/",
                "-o",
                QDir::toNativeSeparators(maps_dir),
                QDir::toNativeSeparators(Miscellaneous::GetOptions().bspFile)
            };
            int vpk_ret = Miscellaneous::RunCommandSync(Miscellaneous::PROGRAM_VPKEDITCLI, argumentsVpk);
            if (vpk_ret != 0) {
                Miscellaneous::Log("Warning: vpkeditcli failed to extract embedded files.");
            } else {
                Miscellaneous::Log("Successfully extracted embedded files to " + target_unpacked_dir);
            }
        }
    }

    if (Miscellaneous::CanceLImport) return;

    QString target_maps_dir = QDir(appDir).filePath("maps/" + Miscellaneous::GetOptions().mapName + "/maps");
    QDir().mkpath(target_maps_dir);
    QString final_vmf_dest = QDir(target_maps_dir).filePath(Miscellaneous::GetOptions().mapName + ".vmf");

    if (QFile::exists(final_vmf_dest)) {
        QFile::remove(final_vmf_dest);
    }

    if (QFile::rename(vmf_dest, final_vmf_dest)) {
        Miscellaneous::Log("Moved VMF to: " + final_vmf_dest);
    } else {
        Miscellaneous::Log("Failed to move VMF to: " + final_vmf_dest);
    }

    Miscellaneous::Options newOptions = Miscellaneous::GetOptions();
    newOptions.contentFolder = QDir(appDir).filePath("maps/" + Miscellaneous::GetOptions().mapName);
    Miscellaneous::SetOptions(newOptions);
    Miscellaneous::Log("Decompiled and prepared at: " + final_vmf_dest);

    if (!Miscellaneous::GetOptions().skipdeps) {
        // Copy materials and models to s1gamedir
        QString s1Subfolder = "csgo";
        if (Miscellaneous::GetOptions().s1GameType == "css") s1Subfolder = "cstrike";
        else if (Miscellaneous::GetOptions().s1GameType == "hl2") s1Subfolder = "hl2";
        else if (Miscellaneous::GetOptions().s1GameType == "l4d") s1Subfolder = "left4dead";
        else if (Miscellaneous::GetOptions().s1GameType == "l4d2") s1Subfolder = "left4dead2";
        else if (Miscellaneous::GetOptions().s1GameType == "portal") s1Subfolder = "portal";
        else if (Miscellaneous::GetOptions().s1GameType == "portal2") s1Subfolder = "portal2";
        else if (Miscellaneous::GetOptions().s1GameType == "tf2") s1Subfolder = "tf";
        else if (Miscellaneous::GetOptions().s1GameType == "gmod") s1Subfolder = "garrysmod";
        else if (Miscellaneous::GetOptions().s1GameType == "blackmesa") s1Subfolder = "bms";
        QString s1gamedir = QDir(Miscellaneous::GetOptions().s1gameBasefolder).filePath(s1Subfolder);

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
}

VmfBspProcess::VmfNode* VmfBspProcess::ParseVmfTree(const QStringList& lines) {
    VmfNode* root = new VmfNode();
    root->isBlock = true;
    root->name = "root";

    QList<VmfNode*> stack;
    stack.append(root);

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QString trimmed = line.trimmed();

        if (trimmed == "{") {
            VmfNode* parent = stack.last();
            if (!parent->children.isEmpty() && !parent->children.last()->isBlock) {
                VmfNode* headerNode = parent->children.last();
                parent->children.removeLast();

                VmfNode* newBlock = new VmfNode();
                newBlock->isBlock = true;
                newBlock->name = headerNode->rawLine.trimmed();
                newBlock->rawLine = headerNode->rawLine;
                newBlock->openBrace = line;
                parent->children.append(newBlock);
                stack.append(newBlock);

                delete headerNode;
            } else {
                VmfNode* newBlock = new VmfNode();
                newBlock->isBlock = true;
                newBlock->name = "";
                newBlock->rawLine = "";
                newBlock->openBrace = line;
                parent->children.append(newBlock);
                stack.append(newBlock);
            }
        } else if (trimmed == "}") {
            if (stack.size() > 1) {
                stack.last()->closeBrace = line;
                stack.removeLast();
            } else {
                VmfNode* rawNode = new VmfNode();
                rawNode->isBlock = false;
                rawNode->rawLine = line;
                stack.last()->children.append(rawNode);
            }
        } else {
            VmfNode* rawNode = new VmfNode();
            rawNode->isBlock = false;
            rawNode->rawLine = line;
            stack.last()->children.append(rawNode);
        }
    }
    return root;
}

QString VmfBspProcess::GetVmfKeyValue(const QString& rawLine, const QString& key) {
    QRegularExpression re(QString("^\\s*\"%1\"\\s+\"([^\"]*)\"").arg(QRegularExpression::escape(key)), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(rawLine);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

bool VmfBspProcess::IsSkipOrHintMaterial(const QString& material) {
    QString lowered = material.toLower();
    return (lowered == "tools/toolsskip" || lowered == "tools/toolshint");
}

bool VmfBspProcess::HasSkipOrHint(const VmfNode* node) {
    if (node->isBlock && node->name.toLower() == "side") {
        for (const VmfNode* child : node->children) {
            if (!child->isBlock) {
                QString mat = GetVmfKeyValue(child->rawLine, "material");
                if (!mat.isEmpty() && IsSkipOrHintMaterial(mat)) {
                    return true;
                }
            }
        }
    }
    for (const VmfNode* child : node->children) {
        if (HasSkipOrHint(child)) {
            return true;
        }
    }
    return false;
}

QString VmfBspProcess::ReplaceMaterialLine(const QString& rawLine, const QString& newMaterial) {
    QString indent = "";
    for (int i = 0; i < rawLine.size(); ++i) {
        if (rawLine[i].isSpace()) {
            indent += rawLine[i];
        } else {
            break;
        }
    }
    return indent + QString("\"material\" \"%1\"").arg(newMaterial);
}

void VmfBspProcess::ModifySolidMaterials(VmfNode* node) {
    if (node->isBlock && node->name.toLower() == "side") {
        for (VmfNode* child : node->children) {
            if (!child->isBlock) {
                QString mat = GetVmfKeyValue(child->rawLine, "material");
                if (!mat.isEmpty() && IsSkipOrHintMaterial(mat)) {
                    child->rawLine = ReplaceMaterialLine(child->rawLine, "tools/toolsnodraw");
                }
            }
        }
    }
    for (VmfNode* child : node->children) {
        if (child->isBlock) {
            ModifySolidMaterials(child);
        }
    }
}

void VmfBspProcess::ProcessVmfTree(VmfNode* node, VmfContext context) {
    VmfContext childContext = context;
    if (node->isBlock) {
        QString loweredName = node->name.toLower();
        if (loweredName == "world") {
            childContext = ContextWorld;
        } else if (loweredName == "entity") {
            QString classname = "";
            for (const VmfNode* child : node->children) {
                if (!child->isBlock) {
                    QString val = GetVmfKeyValue(child->rawLine, "classname");
                    if (!val.isEmpty()) {
                        classname = val;
                        break;
                    }
                }
            }
            if (classname.toLower() == "func_detail") {
                childContext = ContextFuncDetail;
            } else {
                childContext = ContextOtherEntity;
            }
        }
    }

    QList<VmfNode*> newChildren;
    for (VmfNode* child : node->children) {
        if (child->isBlock && child->name.toLower() == "solid") {
            if (HasSkipOrHint(child)) {
                if (childContext == ContextWorld || childContext == ContextFuncDetail || childContext == ContextRoot) {
                    delete child;
                    continue;
                } else if (childContext == ContextOtherEntity) {
                    ModifySolidMaterials(child);
                    newChildren.append(child);
                } else {
                    newChildren.append(child);
                }
            } else {
                ProcessVmfTree(child, childContext);
                newChildren.append(child);
            }
        } else {
            if (child->isBlock) {
                ProcessVmfTree(child, childContext);
            }
            newChildren.append(child);
        }
    }
    node->children = newChildren;
}

void VmfBspProcess::SerializeVmfTree(const VmfNode* node, QStringList& out_lines) {
    if (node->name != "root") {
        if (!node->rawLine.isEmpty()) {
            out_lines.append(node->rawLine);
        }
        if (!node->openBrace.isEmpty()) {
            out_lines.append(node->openBrace);
        }
    }
    for (const VmfNode* child : node->children) {
        if (child->isBlock) {
            SerializeVmfTree(child, out_lines);
        } else {
            out_lines.append(child->rawLine);
        }
    }
    if (node->name != "root") {
        if (!node->closeBrace.isEmpty()) {
            out_lines.append(node->closeBrace);
        }
    }
}

void VmfBspProcess::RemoveSkipAndHintSolids(const QString& vmfPath) {
    if (!QFile::exists(vmfPath)) return;

    QFile infile(vmfPath);
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines;
    QTextStream in(&infile);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    infile.close();

    VmfNode* root = ParseVmfTree(lines);
    ProcessVmfTree(root, ContextRoot);

    QStringList out_lines;
    SerializeVmfTree(root, out_lines);

    delete root;

    QFile outfile(vmfPath);
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outfile);
        for (const QString& l : out_lines) {
            out << l << "\n";
        }
        outfile.close();
    }
}
