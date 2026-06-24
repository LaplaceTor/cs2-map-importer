#include "SoundscapeImport.h"
#include "Miscellaneous.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
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

void SoundscapeImport::ImportSoundscapes(MapImporter* importer, QSet<QString>& uniqueSounds) {
    QDir scriptsDir(Miscellaneous::globalOptions.s1contentdir + "\\scripts");
    if (!scriptsDir.exists()) {
        return;
    }

    QStringList nameFilters;
    nameFilters << "soundscapes_*.txt";
    QFileInfoList soundscapeFiles = scriptsDir.entryInfoList(nameFilters, QDir::Files);

    if (soundscapeFiles.isEmpty()) {
        return;
    }

    for (const QFileInfo& fileInfo : soundscapeFiles) {
        if (Miscellaneous::CanceLImport) return;

        if (fileInfo.fileName().toLower() == "soundscapes_manifest.txt") {
            continue;
        }

        QStringList lines = importer->ReadTextFile(fileInfo.absoluteFilePath());
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

                    for (const QString& w : waves) {
                        QString wNorm = w;
                        wNorm.replace("\\", "/");
                        if (!wNorm.startsWith("sound/")) {
                            wNorm = "sound/" + wNorm;
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

        QString outPath = Miscellaneous::globalOptions.s2contentdir + "\\soundevents\\" + baseName + ".vsndevts";
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        importer->EnsureFileWritable(outPath);
        QFile vsndFile(outPath);
        if (vsndFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&vsndFile);
            out << vsndevtsContent;
            vsndFile.close();
        }
    }
}
