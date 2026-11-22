#include <QCoreApplication>
#include "Domain/Audio/SoundscapeParser.h"
#include <QStringView>

namespace Domain::Audio {

namespace {

bool equalsIgnoreCase(QStringView a, QStringView b) {
    return a.trimmed().compare(b, Qt::CaseInsensitive) == 0;
}

QString cleanVal(QString val) {
    val = val.trimmed();
    if (val.endsWith(';')) {
        val.chop(1);
        val = val.trimmed();
    }
    return val;
}

} // namespace

Core::Result<std::vector<SoundscapeDefinition>> SoundscapeParser::parseString(const QString& content) {
    if (content.trimmed().isEmpty()) {
        return Core::Result<std::vector<SoundscapeDefinition>>::success({});
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromString(content);
    if (!loadRes.isSuccess()) {
        return Core::Result<std::vector<SoundscapeDefinition>>::failure(loadRes.error(), QCoreApplication::translate("SoundscapeParser", "Failed to parse Soundscape VDF content"));
    }

    return parseDocument(doc);
}

Core::Result<std::vector<SoundscapeDefinition>> SoundscapeParser::parseFile(const Core::Path::FilesystemPath& filePath) {
    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromFile(filePath);
    if (!loadRes.isSuccess()) {
        return Core::Result<std::vector<SoundscapeDefinition>>::failure(loadRes.error(), QCoreApplication::translate("SoundscapeParser", "Failed to load Soundscape file"));
    }

    return parseDocument(doc);
}

Core::Result<std::vector<SoundscapeDefinition>> SoundscapeParser::parseDocument(const Core::KeyValues::KeyValuesDocument& doc) {
    std::vector<SoundscapeDefinition> definitions;

    const auto& rootChildren = doc.root().children();
    for (const auto& node : rootChildren) {
        if (!node.isSection()) {
            continue;
        }

        if (equalsIgnoreCase(node.name(), u"soundscapes_manifest") ||
            equalsIgnoreCase(node.name(), u"soundscaples_manifest")) {
            continue;
        }

        auto def = parseSoundscapeNode(node);
        if (def.has_value() && !def->isEmpty()) {
            definitions.push_back(std::move(*def));
        }
    }

    return Core::Result<std::vector<SoundscapeDefinition>>::success(std::move(definitions));
}

std::optional<SoundscapeDefinition> SoundscapeParser::parseSoundscapeNode(const Core::KeyValues::KeyValuesNode& node) {
    if (!node.isSection() || node.name().trimmed().isEmpty()) {
        return std::nullopt;
    }

    SoundscapeDefinition def;
    def.name = node.name().trimmed();

    for (const auto& child : node.children()) {
        if (child.isProperty()) {
            const QStringView key = QStringView(child.name()).trimmed();
            const QString val = cleanVal(child.value());
            if (equalsIgnoreCase(key, u"dsp")) {
                bool ok = false;
                int dsp = val.toInt(&ok);
                if (ok) def.dspIndex = dsp;
            } else if (equalsIgnoreCase(key, u"dsp_volume") || equalsIgnoreCase(key, u"dsp_spatial")) {
                bool ok = false;
                double v = val.toDouble(&ok);
                if (ok) def.dspVolume = v;
            } else if (equalsIgnoreCase(key, u"fadetime")) {
                bool ok = false;
                double ft = val.toDouble(&ok);
                if (ok) def.fadeTime = ft;
            } else if (equalsIgnoreCase(key, u"origin")) {
                def.origin = Vector3::fromString(val);
            } else if (equalsIgnoreCase(key, u"position")) {
                def.position = val;
            } else if (equalsIgnoreCase(key, u"positionoverride") || equalsIgnoreCase(key, u"ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) def.positionOverride = po;
            }
        }
    }

    extractElementsFromNode(node, def.elements);
    return def;
}

void SoundscapeParser::extractElementsFromNode(const Core::KeyValues::KeyValuesNode& node, std::vector<SoundscapeElement>& elements) {
    for (const auto& child : node.children()) {
        if (!child.isSection()) {
            continue;
        }

        const QStringView name = QStringView(child.name()).trimmed();
        if (equalsIgnoreCase(name, u"playlooping")) {
            elements.push_back(parsePlayLooping(child));
        } else if (equalsIgnoreCase(name, u"playrandom")) {
            elements.push_back(parsePlayRandom(child));
        } else if (equalsIgnoreCase(name, u"playsoundscape")) {
            elements.push_back(parsePlaySoundscape(child));
        } else {
            // Nested subsection that may contain playlooping/playrandom/playsoundscape
            extractElementsFromNode(child, elements);
        }
    }
}

PlayLoopingElement SoundscapeParser::parsePlayLooping(const Core::KeyValues::KeyValuesNode& node) {
    PlayLoopingElement elem;

    for (const auto& child : node.children()) {
        if (child.isProperty()) {
            const QStringView key = QStringView(child.name()).trimmed();
            const QString val = cleanVal(child.value());

            if (equalsIgnoreCase(key, u"wave")) {
                elem.wave = val;
            } else if (equalsIgnoreCase(key, u"volume")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.volume = *r;
            } else if (equalsIgnoreCase(key, u"pitch")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.pitch = *r;
            } else if (equalsIgnoreCase(key, u"soundlevel")) {
                elem.soundLevel = val;
            } else if (equalsIgnoreCase(key, u"origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (equalsIgnoreCase(key, u"position")) {
                elem.position = val;
            } else if (equalsIgnoreCase(key, u"positionoverride") || equalsIgnoreCase(key, u"ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) elem.positionOverride = po;
            }
        }
    }

    return elem;
}

PlayRandomElement SoundscapeParser::parsePlayRandom(const Core::KeyValues::KeyValuesNode& node) {
    PlayRandomElement elem;

    for (const auto& child : node.children()) {
        const QStringView key = QStringView(child.name()).trimmed();
        if (child.isProperty()) {
            const QString val = cleanVal(child.value());

            if (equalsIgnoreCase(key, u"time")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.timeInterval = *r;
            } else if (equalsIgnoreCase(key, u"volume")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.volume = *r;
            } else if (equalsIgnoreCase(key, u"pitch")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.pitch = *r;
            } else if (equalsIgnoreCase(key, u"soundlevel")) {
                elem.soundLevel = val;
            } else if (equalsIgnoreCase(key, u"origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (equalsIgnoreCase(key, u"position")) {
                elem.position = val;
            } else if (equalsIgnoreCase(key, u"positionoverride") || equalsIgnoreCase(key, u"ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) elem.positionOverride = po;
            } else if (equalsIgnoreCase(key, u"wave")) {
                elem.waves.append(val);
            }
        } else if (child.isSection()) {
            if (equalsIgnoreCase(key, u"rndwave")) {
                for (const auto& sub : child.children()) {
                    if (sub.isProperty() && equalsIgnoreCase(sub.name(), u"wave")) {
                        elem.waves.append(cleanVal(sub.value()));
                    }
                }
            }
        }
    }

    return elem;
}

PlaySoundscapeElement SoundscapeParser::parsePlaySoundscape(const Core::KeyValues::KeyValuesNode& node) {
    PlaySoundscapeElement elem;

    for (const auto& child : node.children()) {
        if (child.isProperty()) {
            const QStringView key = QStringView(child.name()).trimmed();
            const QString val = cleanVal(child.value());

            if (equalsIgnoreCase(key, u"name")) {
                elem.targetSoundscape = val;
            } else if (equalsIgnoreCase(key, u"volume")) {
                bool ok = false;
                double v = val.toDouble(&ok);
                if (ok) elem.volume = v;
            } else if (equalsIgnoreCase(key, u"origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (equalsIgnoreCase(key, u"position")) {
                elem.position = val;
            } else if (equalsIgnoreCase(key, u"positionoverride") || equalsIgnoreCase(key, u"ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) elem.positionOverride = po;
            }
        }
    }

    return elem;
}

} // namespace Domain::Audio

