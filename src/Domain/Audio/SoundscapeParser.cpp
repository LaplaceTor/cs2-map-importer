#include "Domain/Audio/SoundscapeParser.h"
#include "Core/KeyValues/KeyValuesParser.h"
#include <QSet>

namespace Domain::Audio {

namespace {

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
        return Core::Result<std::vector<SoundscapeDefinition>>::failure(loadRes.error(), QStringLiteral("Failed to parse Soundscape VDF content"));
    }

    return parseDocument(doc);
}

Core::Result<std::vector<SoundscapeDefinition>> SoundscapeParser::parseFile(const Core::Path::FilesystemPath& filePath) {
    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromFile(filePath);
    if (!loadRes.isSuccess()) {
        return Core::Result<std::vector<SoundscapeDefinition>>::failure(loadRes.error(), QStringLiteral("Failed to load Soundscape file"));
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

        const QString lowerName = node.name().trimmed().toLower();
        if (lowerName == QStringLiteral("soundscapes_manifest") ||
            lowerName == QStringLiteral("soundscaples_manifest")) {
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
        const QString key = child.name().trimmed().toLower();
        if (child.isProperty()) {
            const QString val = cleanVal(child.value());
            if (key == QStringLiteral("dsp")) {
                bool ok = false;
                int dsp = val.toInt(&ok);
                if (ok) def.dspIndex = dsp;
            } else if (key == QStringLiteral("dsp_volume") || key == QStringLiteral("dsp_spatial")) {
                bool ok = false;
                double v = val.toDouble(&ok);
                if (ok) def.dspVolume = v;
            } else if (key == QStringLiteral("fadetime")) {
                bool ok = false;
                double ft = val.toDouble(&ok);
                if (ok) def.fadeTime = ft;
            } else if (key == QStringLiteral("origin")) {
                def.origin = Vector3::fromString(val);
            } else if (key == QStringLiteral("position")) {
                def.position = val;
            } else if (key == QStringLiteral("positionoverride") || key == QStringLiteral("ambientpositionoverride")) {
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

        const QString lowerName = child.name().trimmed().toLower();
        if (lowerName == QStringLiteral("playlooping")) {
            elements.push_back(parsePlayLooping(child));
        } else if (lowerName == QStringLiteral("playrandom")) {
            elements.push_back(parsePlayRandom(child));
        } else if (lowerName == QStringLiteral("playsoundscape")) {
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
            const QString key = child.name().trimmed().toLower();
            const QString val = cleanVal(child.value());

            if (key == QStringLiteral("wave")) {
                elem.wave = val;
            } else if (key == QStringLiteral("volume")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.volume = *r;
            } else if (key == QStringLiteral("pitch")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.pitch = *r;
            } else if (key == QStringLiteral("soundlevel")) {
                elem.soundLevel = val;
            } else if (key == QStringLiteral("origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (key == QStringLiteral("position")) {
                elem.position = val;
            } else if (key == QStringLiteral("positionoverride") || key == QStringLiteral("ambientpositionoverride")) {
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
        const QString key = child.name().trimmed().toLower();
        if (child.isProperty()) {
            const QString val = cleanVal(child.value());

            if (key == QStringLiteral("time")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.timeInterval = *r;
            } else if (key == QStringLiteral("volume")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.volume = *r;
            } else if (key == QStringLiteral("pitch")) {
                auto r = DoubleRange::fromString(val);
                if (r) elem.pitch = *r;
            } else if (key == QStringLiteral("soundlevel")) {
                elem.soundLevel = val;
            } else if (key == QStringLiteral("origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (key == QStringLiteral("position")) {
                elem.position = val;
            } else if (key == QStringLiteral("positionoverride") || key == QStringLiteral("ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) elem.positionOverride = po;
            } else if (key == QStringLiteral("wave")) {
                elem.waves.append(val);
            }
        } else if (child.isSection()) {
            if (key == QStringLiteral("rndwave")) {
                for (const auto& sub : child.children()) {
                    if (sub.isProperty() && sub.name().trimmed().toLower() == QStringLiteral("wave")) {
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
            const QString key = child.name().trimmed().toLower();
            const QString val = cleanVal(child.value());

            if (key == QStringLiteral("name")) {
                elem.targetSoundscape = val;
            } else if (key == QStringLiteral("volume")) {
                bool ok = false;
                double v = val.toDouble(&ok);
                if (ok) elem.volume = v;
            } else if (key == QStringLiteral("origin")) {
                elem.origin = Vector3::fromString(val);
            } else if (key == QStringLiteral("position")) {
                elem.position = val;
            } else if (key == QStringLiteral("positionoverride") || key == QStringLiteral("ambientpositionoverride")) {
                bool ok = false;
                int po = val.toInt(&ok);
                if (ok) elem.positionOverride = po;
            }
        }
    }

    return elem;
}

} // namespace Domain::Audio

