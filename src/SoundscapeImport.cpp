#include "SoundscapeImport.h"
#include "Miscellaneous.h"
#include "MapImporter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSharedPointer>
#include <QRegularExpression>

struct VDFNode {
    QString name;
    QString value;
    QList<QSharedPointer<VDFNode>> children;
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

        for (int i = 0; i < line.size(); ++i) {
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

static QList<QSharedPointer<VDFNode>> ParseVDF(const QStringList& tokens, int& index, int depth = 0) {
    if (depth > 8) {
        throw AppException("Maximum recursion depth (8 layers) exceeded in ParseVDF");
    }

    QList<QSharedPointer<VDFNode>> nodes;

    while (index < tokens.size()) {
        if (tokens[index] == "}") {
            return nodes;
        }

        auto node = QSharedPointer<VDFNode>::create();
        if (index >= tokens.size()) {
            break;
        }
        node->name = tokens[index++];

        if (index < tokens.size()) {
            if (tokens[index] == "{") {
                index++; // skip '{'
                node->children = ParseVDF(tokens, index, depth + 1);
                if (index < tokens.size() && tokens[index] == "}") {
                    index++; // skip '}'
                }
            } else {
                if (index < tokens.size()) {
                    node->value = tokens[index++];
                }
            }
        }

        nodes.append(node);
    }

    return nodes;
}

static void ParseSoundscapeProperties(const QList<QSharedPointer<VDFNode>>& children, QString& timeMin, QString& timeMax, QString& pitchMin, QString& pitchMax, QString& volMin, QString& volMax, QString& sndLvl, QStringList& waves, QString& origin) {
    for (auto c : children) {
        if (Miscellaneous::CanceLImport) return;
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
        } else if (lowerName == "origin") {
            origin = c->value;
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
    QDir scriptsDir(Miscellaneous::GetOptions().s1contentdir + "\\scripts");
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

        QStringList lines = Miscellaneous::ReadTextFile(fileInfo.absoluteFilePath());
        QStringList tokens = TokenizeVDF(lines);
        int index = 0;
        QList<QSharedPointer<VDFNode>> roots;
        try {
            roots = ParseVDF(tokens, index);
        } catch (const AppException& e) {
            Miscellaneous::Log(QString("Error parsing soundscape file %1: %2. Skipping this file.").arg(fileInfo.fileName()).arg(e.message()));
            continue;
        } catch (const QException& e) {
            Miscellaneous::Log(QString("Error parsing soundscape file %1. Skipping this file.").arg(fileInfo.fileName()));
            continue;
        }

        QString vsndevtsContent = "<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n{\n";

        for (auto root : roots) {
            if (Miscellaneous::CanceLImport) return;
            QString soundscapeName = root->name;
            if (soundscapeName.toLower() == "playlooping" || soundscapeName.toLower() == "playrandom" || soundscapeName.toLower() == "origin") continue; // should not be at root

            QString parentBlock = QString("\t\"%1\" = \n\t{\n").arg(soundscapeName);
            parentBlock += "\t\tenable_child_events = true\n";

            QString parentOrigin = "";
            for (auto c : root->children) {
                if (c->name.toLower() == "origin") {
                    parentOrigin = c->value;
                    break;
                }
            }

            if (!parentOrigin.isEmpty()) {
                QStringList originParts = parentOrigin.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
                if (originParts.size() == 3) {
                    bool ok1, ok2, ok3;
                    float x = originParts[0].toFloat(&ok1);
                    float y = originParts[1].toFloat(&ok2);
                    float z = originParts[2].toFloat(&ok3);
                    if (ok1 && ok2 && ok3) {
                        parentBlock += "\t\tset_child_position = true\n";
                        parentBlock += QString("\t\tposition = [%1, %2, %3]\n").arg(x).arg(y).arg(z);
                    } else {
                        parentBlock += "\t\tset_child_position = false\n";
                    }
                } else {
                    parentBlock += "\t\tset_child_position = false\n";
                }
            } else {
                parentBlock += "\t\tset_child_position = false\n";
            }

            parentBlock += "\t\tsoundevent_01 = \n\t\t[\n";

            QString childrenBlocks = "";
            int partCount = 1;

            for (auto child : root->children) {
                if (Miscellaneous::CanceLImport) return;
                QString cName = child->name.toLower();
                if (cName == "playlooping" || cName == "playrandom") {
                    QString partName = QString("%1.part%2").arg(soundscapeName).arg(partCount);
                    parentBlock += QString("\t\t\t\"%1\",\n").arg(partName);

                    QString timeMin, timeMax, pitchMin, pitchMax, volMin, volMax, sndLvl, partOrigin;
                    QStringList waves;

                    ParseSoundscapeProperties(child->children, timeMin, timeMax, pitchMin, pitchMax, volMin, volMax, sndLvl, waves, partOrigin);

                    childrenBlocks += QString("\n\t\"%1\" = \n\t{\n").arg(partName);
                    childrenBlocks += "\t\ttype = \"csgo_mega\"\n";
                    childrenBlocks += "\t\tmixgroup = \"Amb_Common\"\n";
                    childrenBlocks += "\t\tdistance_effect_mix = 0.0\n";

                    if (!partOrigin.isEmpty()) {
                        QStringList originParts = partOrigin.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
                        if (originParts.size() == 3) {
                            bool ok1, ok2, ok3;
                            float x = originParts[0].toFloat(&ok1);
                            float y = originParts[1].toFloat(&ok2);
                            float z = originParts[2].toFloat(&ok3);
                            if (ok1 && ok2 && ok3) {
                                childrenBlocks += "\t\tuse_world_position = true\n";
                                childrenBlocks += QString("\t\tposition = [%1, %2, %3]\n").arg(x).arg(y).arg(z);
                            } else {
                                childrenBlocks += "\t\tposition_relative_to_player = true\n";
                                childrenBlocks += "\t\tposition = [0.0, 0.0, 0.0]\n";
                            }
                        } else {
                            childrenBlocks += "\t\tposition_relative_to_player = true\n";
                            childrenBlocks += "\t\tposition = [0.0, 0.0, 0.0]\n";
                        }
                    } else {
                        childrenBlocks += "\t\tposition_relative_to_player = true\n";
                        childrenBlocks += "\t\tposition = [0.0, 0.0, 0.0]\n";
                    }

                    if (!timeMin.isEmpty() && !timeMax.isEmpty()) {
                        childrenBlocks += QString("\t\tretrigger_interval_min = %1\n").arg(timeMin);
                        childrenBlocks += QString("\t\tretrigger_interval_max = %1\n").arg(timeMax);
                    }
                    if (!pitchMin.isEmpty() && !pitchMax.isEmpty() && pitchMin != pitchMax) {
                        childrenBlocks += QString("\t\tpitch_random_min = %1\n").arg(pitchMin);
                        childrenBlocks += QString("\t\tpitch_random_max = %1\n").arg(pitchMax);
                    } else if (!pitchMin.isEmpty()) {
                        childrenBlocks += QString("\t\tpitch = %1\n").arg(pitchMin);
                    } else {
                        childrenBlocks += "\t\tpitch = 1.0\n";
                    }

                    if (!volMin.isEmpty() && !volMax.isEmpty() && volMin != volMax) {
                        childrenBlocks += QString("\t\tvolume_random_min = %1\n").arg(volMin);
                        childrenBlocks += QString("\t\tvolume_random_max = %1\n").arg(volMax);
                    } else if (!volMin.isEmpty()) {
                        childrenBlocks += QString("\t\tvolume = %1\n").arg(volMin);
                    } else {
                        childrenBlocks += "\t\tvolume = 1.0\n";
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
                    } else {
                        childrenBlocks += "\t\tvsnd_files_track_01 = []\n";
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

        vsndevtsContent += "}\n";

        QString baseName = fileInfo.baseName(); // soundscapes_xxx
        if (baseName.startsWith("soundscapes_")) {
            baseName.replace(0, 12, "soundevents_");
        } else {
            baseName = "soundevents_" + baseName;
        }

        QString outPath = Miscellaneous::GetOptions().s2contentdir + "\\soundevents\\" + baseName + ".vsndevts";
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        Miscellaneous::EnsureFileWritable(outPath);
        QFile vsndFile(outPath);
        if (vsndFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&vsndFile);
            out << vsndevtsContent;
            vsndFile.close();
        }
    }
}
