#include "Domain/Audio/SoundscapeToSoundEventConverter.h"
#include "Domain/Audio/DspPresetRegistry.h"
#include "Domain/Audio/SoundLevelMapper.h"
#include "Core/Path/PathUtils.h"
#include <algorithm>

namespace Domain::Audio {

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

    // 2. Build Child SoundEvents
    int partIndex = 1;
    for (const auto& elem : def.elements) {
        if (std::holds_alternative<PlayLoopingElement>(elem)) {
            const auto& looping = std::get<PlayLoopingElement>(elem);
            QString partName = QStringLiteral("%1.part%2").arg(def.name).arg(partIndex++);
            master.childEvents.append(partName);

            SoundEvent child;
            child.name = partName;
            child.type = QStringLiteral("csgo_mega");
            child.mixgroup = defaultMixgroup;
            child.distanceEffectMix = 0.0;
            child.restrictSourceReverb = true;

            // Volume
            if (looping.volume.isFixed()) {
                child.volume = looping.volume.min();
            } else {
                child.volume = looping.volume.min();
                child.volumeRandomMax = looping.volume.delta();
            }

            // Pitch (100 is 1.0)
            if (looping.pitch.isFixed()) {
                child.pitch = looping.pitch.min() / 100.0;
            } else {
                child.pitch = 1.0;
                child.pitchRandomMin = (looping.pitch.min() - 100.0) / 100.0;
                child.pitchRandomMax = (looping.pitch.max() - 100.0) / 100.0;
            }

            // Positioning
            if (looping.position.trimmed().toLower() == QStringLiteral("random")) {
                child.positionRelativeToPlayer = true;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.randomizePositionMinRadius = 0.0;
                child.randomizePositionMaxRadius = 300.0;
                child.useWorldPosition = false;
            } else if (looping.origin.has_value() && !looping.origin->isZero()) {
                child.position = *looping.origin;
                child.useWorldPosition = true;
                child.positionRelativeToPlayer = false;
            } else if (!looping.position.trimmed().isEmpty() || looping.positionOverride.has_value()) {
                child.positionRelativeToPlayer = false;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.useWorldPosition = false;
            } else {
                child.positionRelativeToPlayer = true;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.useWorldPosition = false;
            }

            // Soundlevel curve
            if (!looping.soundLevel.trimmed().isEmpty()) {
                child.distanceVolumeMappingCurve = SoundLevelMapper::createDistanceVolumeCurve(looping.soundLevel);
                if (!child.distanceVolumeMappingCurve.empty()) {
                    child.useDistanceVolumeMappingCurve = true;
                }
            }

            // Audio wave
            if (!looping.wave.trimmed().isEmpty()) {
                child.vsndFiles.append(formatVsndPath(looping.wave));
                result.uniqueRawSoundAssets.insert(formatRawAssetPath(looping.wave));
            }

            result.soundEvents.push_back(std::move(child));

        } else if (std::holds_alternative<PlayRandomElement>(elem)) {
            const auto& randomElem = std::get<PlayRandomElement>(elem);
            QString partName = QStringLiteral("%1.part%2").arg(def.name).arg(partIndex++);
            master.childEvents.append(partName);

            SoundEvent child;
            child.name = partName;
            child.type = QStringLiteral("csgo_mega");
            child.mixgroup = defaultMixgroup;
            child.distanceEffectMix = 0.0;
            child.restrictSourceReverb = true;

            // Retrigger
            child.enableRetrigger = true;
            child.retriggerIntervalMin = randomElem.timeInterval.min();
            child.retriggerIntervalMax = randomElem.timeInterval.max();

            // Volume
            if (randomElem.volume.isFixed()) {
                child.volume = randomElem.volume.min();
            } else {
                child.volume = randomElem.volume.min();
                child.volumeRandomMax = randomElem.volume.delta();
            }

            // Pitch (100 is 1.0)
            if (randomElem.pitch.isFixed()) {
                child.pitch = randomElem.pitch.min() / 100.0;
            } else {
                child.pitch = 1.0;
                child.pitchRandomMin = (randomElem.pitch.min() - 100.0) / 100.0;
                child.pitchRandomMax = (randomElem.pitch.max() - 100.0) / 100.0;
            }

            // Positioning
            if (randomElem.position.trimmed().toLower() == QStringLiteral("random")) {
                child.positionRelativeToPlayer = true;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.randomizePositionMinRadius = 0.0;
                child.randomizePositionMaxRadius = 300.0;
                child.useWorldPosition = false;
            } else if (randomElem.origin.has_value() && !randomElem.origin->isZero()) {
                child.position = *randomElem.origin;
                child.useWorldPosition = true;
                child.positionRelativeToPlayer = false;
            } else if (!randomElem.position.trimmed().isEmpty() || randomElem.positionOverride.has_value()) {
                child.positionRelativeToPlayer = false;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.useWorldPosition = false;
            } else {
                child.positionRelativeToPlayer = true;
                child.position = Vector3(0.0, 0.0, 0.0);
                child.useWorldPosition = false;
            }

            // Soundlevel curve
            if (!randomElem.soundLevel.trimmed().isEmpty()) {
                child.distanceVolumeMappingCurve = SoundLevelMapper::createDistanceVolumeCurve(randomElem.soundLevel);
                if (!child.distanceVolumeMappingCurve.empty()) {
                    child.useDistanceVolumeMappingCurve = true;
                }
            }

            // Audio waves
            for (const auto& w : randomElem.waves) {
                if (!w.trimmed().isEmpty()) {
                    child.vsndFiles.append(formatVsndPath(w));
                    result.uniqueRawSoundAssets.insert(formatRawAssetPath(w));
                }
            }

            result.soundEvents.push_back(std::move(child));

        } else if (std::holds_alternative<PlaySoundscapeElement>(elem)) {
            const auto& scape = std::get<PlaySoundscapeElement>(elem);
            if (!scape.targetSoundscape.trimmed().isEmpty()) {
                master.childEvents.append(scape.targetSoundscape.trimmed());
            }
        }
    }

    // If this soundscape produced exactly 1 child event and has no additional playsoundscape references,
    // the child directly becomes the soundscape event itself (e.g. Birds instead of Birds.part1).
    if (result.soundEvents.size() == 1 && master.childEvents.size() == 1) {
        auto& singleChild = result.soundEvents.front();
        singleChild.name = def.name;

        // Inherit DSP and Reverb preset from the parent soundscape definition
        if (!master.dspPreset.isEmpty()) {
            singleChild.dspPreset = master.dspPreset;
            singleChild.overrideDspPreset = master.overrideDspPreset;
            singleChild.reverbWet = master.reverbWet;
        }

        // Inherit Fade/Time volume mapping curves from definition
        if (master.useTimeVolumeMappingCurve) {
            singleChild.useTimeVolumeMappingCurve = true;
            singleChild.timeVolumeMappingCurve = master.timeVolumeMappingCurve;
            singleChild.fadetimeVolumeMappingCurve = master.fadetimeVolumeMappingCurve;
        }

        // Inherit world position / origin if child does not specify its own
        if (master.setChildPosition && !singleChild.useWorldPosition) {
            singleChild.position = master.position;
            singleChild.useWorldPosition = true;
            singleChild.positionRelativeToPlayer = false;
        }

        return result;
    }

    // Insert master at the beginning of this soundscape's event list
    result.soundEvents.insert(result.soundEvents.begin(), std::move(master));
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

