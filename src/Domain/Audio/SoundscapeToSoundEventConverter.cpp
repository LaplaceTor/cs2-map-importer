#include "Domain/Audio/SoundscapeToSoundEventConverter.h"
#include "Domain/Audio/DspPresetRegistry.h"
#include "Domain/Audio/SoundLevelMapper.h"
#include "Core/Path/PathUtils.h"
#include <QStringView>
#include <algorithm>
#include <variant>

namespace Domain::Audio {

namespace {

template <typename ElementT>
void populateCommonChildFields(
    SoundEvent& child,
    const QString& partName,
    const QString& defaultMixgroup,
    const ElementT& elem) {
    child.name = partName;
    child.type = QStringLiteral("csgo_mega");
    child.mixgroup = defaultMixgroup;
    child.distanceEffectMix = 0.0;
    child.restrictSourceReverb = true;

    // Volume
    if (elem.volume.isFixed()) {
        child.volume = elem.volume.min();
    } else {
        child.volume = elem.volume.min();
        child.volumeRandomMax = elem.volume.delta();
    }

    // Pitch (100 is 1.0)
    if (elem.pitch.isFixed()) {
        child.pitch = elem.pitch.min() / 100.0;
    } else {
        child.pitch = 1.0;
        child.pitchRandomMin = (elem.pitch.min() - 100.0) / 100.0;
        child.pitchRandomMax = (elem.pitch.max() - 100.0) / 100.0;
    }

    // Positioning
    const QStringView posView = QStringView(elem.position).trimmed();
    if (posView.compare(u"random", Qt::CaseInsensitive) == 0) {
        child.positionRelativeToPlayer = true;
        child.position = Vector3(0.0, 0.0, 0.0);
        child.randomizePositionMinRadius = 0.0;
        child.randomizePositionMaxRadius = 300.0;
        child.useWorldPosition = false;
    } else if (elem.origin.has_value() && !elem.origin->isZero()) {
        child.position = *elem.origin;
        child.useWorldPosition = true;
        child.positionRelativeToPlayer = false;
    } else if (!posView.isEmpty() || elem.positionOverride.has_value()) {
        child.positionRelativeToPlayer = false;
        child.position = Vector3(0.0, 0.0, 0.0);
        child.useWorldPosition = false;
    } else {
        child.positionRelativeToPlayer = true;
        child.position = Vector3(0.0, 0.0, 0.0);
        child.useWorldPosition = false;
    }

    // Soundlevel curve
    if (!QStringView(elem.soundLevel).trimmed().isEmpty()) {
        child.distanceVolumeMappingCurve = SoundLevelMapper::createDistanceVolumeCurve(elem.soundLevel);
        if (!child.distanceVolumeMappingCurve.empty()) {
            child.useDistanceVolumeMappingCurve = true;
        }
    }
}

} // namespace

QString SoundscapeToSoundEventConverter::formatVsndPath(const QString& rawWavePath) {
    QString p = Core::Path::PathUtils::normalize(rawWavePath.trimmed());

    while (p.startsWith(QLatin1Char('/'))) {
        p.remove(0, 1);
    }
    while (p.startsWith(QStringLiteral("./"))) {
        p.remove(0, 2);
    }

    if (p.startsWith(QStringLiteral("sound/"), Qt::CaseInsensitive)) {
        p.replace(0, 6, QStringLiteral("sounds/"));
    } else if (!p.startsWith(QStringLiteral("sounds/"), Qt::CaseInsensitive)) {
        p.prepend(QStringLiteral("sounds/"));
    }

    int dotIndex = p.lastIndexOf(QLatin1Char('.'));
    if (dotIndex != -1) {
        p = p.left(dotIndex) + QStringLiteral(".vsnd");
    } else {
        p += QStringLiteral(".vsnd");
    }

    return p;
}

QString SoundscapeToSoundEventConverter::formatRawAssetPath(const QString& rawWavePath) {
    QString p = Core::Path::PathUtils::normalize(rawWavePath.trimmed());

    while (p.startsWith(QLatin1Char('/'))) {
        p.remove(0, 1);
    }
    while (p.startsWith(QStringLiteral("./"))) {
        p.remove(0, 2);
    }

    if (!p.startsWith(QStringLiteral("sound/"), Qt::CaseInsensitive)) {
        p.prepend(QStringLiteral("sound/"));
    }

    return p;
}

ConversionResult SoundscapeToSoundEventConverter::convert(const SoundscapeDefinition& def, const ConversionOptions& options) {
    ConversionResult result;
    if (def.isEmpty()) {
        return result;
    }

    const QString defaultMixgroup = options.mixgroup.isEmpty() ? QStringLiteral("Amb_Common") : options.mixgroup;

    // 1. Build Master SoundEvent
    SoundEvent master;
    master.name = def.name;
    master.type = QStringLiteral("csgo_mega");
    master.mixgroup = defaultMixgroup;
    master.enableChildEvents = true;
    master.distanceEffectMix = 0.0;
    master.restrictSourceReverb = true;

    if (def.dspIndex.has_value()) {
        auto dspInfo = DspPresetRegistry::lookupByIndex(*def.dspIndex);
        if (dspInfo.has_value() && dspInfo->overrideDsp) {
            master.dspPreset = dspInfo->s2PresetName;
            master.overrideDspPreset = true;
            master.reverbWet = def.dspVolume.value_or(dspInfo->defaultReverbWet);
        }
    }

    if (def.fadeTime.has_value() && *def.fadeTime > 0.0) {
        master.useTimeVolumeMappingCurve = true;
        master.timeVolumeMappingCurve = SoundLevelMapper::createTimeVolumeCurve(*def.fadeTime);
        master.fadetimeVolumeMappingCurve = SoundLevelMapper::createFadeTimeVolumeCurve(*def.fadeTime);
    }

    if (def.origin.has_value() && !def.origin->isZero()) {
        master.setChildPosition = true;
        master.position = *def.origin;
    }

    // Reserve slot 0 upfront for master to avoid O(N) front insertion after building children
    result.soundEvents.reserve(def.elements.size() + 1);
    result.soundEvents.push_back(std::move(master));

    // 2. Build Child SoundEvents
    int partIndex = 1;
    for (const auto& elem : def.elements) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, PlayLoopingElement>) {
                const QString partName = QStringLiteral("%1.part%2").arg(def.name).arg(partIndex++);
                result.soundEvents.front().childEvents.append(partName);

                SoundEvent child;
                populateCommonChildFields(child, partName, defaultMixgroup, node);

                // Audio wave
                if (!QStringView(node.wave).trimmed().isEmpty()) {
                    child.vsndFiles.append(formatVsndPath(node.wave));
                    result.uniqueRawSoundAssets.insert(formatRawAssetPath(node.wave));
                }

                result.soundEvents.push_back(std::move(child));
            } else if constexpr (std::is_same_v<T, PlayRandomElement>) {
                const QString partName = QStringLiteral("%1.part%2").arg(def.name).arg(partIndex++);
                result.soundEvents.front().childEvents.append(partName);

                SoundEvent child;
                populateCommonChildFields(child, partName, defaultMixgroup, node);

                // Retrigger
                child.enableRetrigger = true;
                child.retriggerIntervalMin = node.timeInterval.min();
                child.retriggerIntervalMax = node.timeInterval.max();

                // Audio waves
                for (const auto& w : node.waves) {
                    if (!QStringView(w).trimmed().isEmpty()) {
                        child.vsndFiles.append(formatVsndPath(w));
                        result.uniqueRawSoundAssets.insert(formatRawAssetPath(w));
                    }
                }

                result.soundEvents.push_back(std::move(child));
            } else if constexpr (std::is_same_v<T, PlaySoundscapeElement>) {
                const QString target = node.targetSoundscape.trimmed();
                if (!target.isEmpty()) {
                    result.soundEvents.front().childEvents.append(target);
                }
            }
        }, elem);
    }

    // If this soundscape produced exactly 1 child event and has no additional playsoundscape references,
    // the child directly becomes the soundscape event itself (e.g. Birds instead of Birds.part1).
    if (result.soundEvents.size() == 2 && result.soundEvents.front().childEvents.size() == 1) {
        const SoundEvent& masterRef = result.soundEvents.front();
        SoundEvent& singleChild = result.soundEvents[1];
        singleChild.name = def.name;

        // Inherit DSP and Reverb preset from the parent soundscape definition
        if (!masterRef.dspPreset.isEmpty()) {
            singleChild.dspPreset = masterRef.dspPreset;
            singleChild.overrideDspPreset = masterRef.overrideDspPreset;
            singleChild.reverbWet = masterRef.reverbWet;
        }

        // Inherit Fade/Time volume mapping curves from definition
        if (masterRef.useTimeVolumeMappingCurve) {
            singleChild.useTimeVolumeMappingCurve = true;
            singleChild.timeVolumeMappingCurve = masterRef.timeVolumeMappingCurve;
            singleChild.fadetimeVolumeMappingCurve = masterRef.fadetimeVolumeMappingCurve;
        }

        // Inherit world position / origin if child does not specify its own
        if (masterRef.setChildPosition && !singleChild.useWorldPosition) {
            singleChild.position = masterRef.position;
            singleChild.useWorldPosition = true;
            singleChild.positionRelativeToPlayer = false;
        }

        result.soundEvents.front() = std::move(singleChild);
        result.soundEvents.pop_back();
        return result;
    }

    return result;
}

ConversionResult SoundscapeToSoundEventConverter::convertBatch(const std::vector<SoundscapeDefinition>& defs, const ConversionOptions& options) {
    ConversionResult combined;
    for (const auto& def : defs) {
        auto res = convert(def, options);
        combined.soundEvents.insert(combined.soundEvents.end(),
                                   std::make_move_iterator(res.soundEvents.begin()),
                                   std::make_move_iterator(res.soundEvents.end()));
        combined.uniqueRawSoundAssets.unite(res.uniqueRawSoundAssets);
    }
    return combined;
}

} // namespace Domain::Audio

