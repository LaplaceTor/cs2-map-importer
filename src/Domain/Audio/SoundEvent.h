#pragma once

#include "Domain/Audio/Range.h"
#include "Domain/Audio/SoundLevelMapper.h"
#include <QString>
#include <QStringList>
#include <vector>
#include <optional>

namespace Domain::Audio {

struct SoundEvent {
    QString name;
    QString type = QStringLiteral("csgo_mega");
    QString mixgroup;

    // Volume
    double volume = 1.0;
    std::optional<double> volumeRandomMin;
    std::optional<double> volumeRandomMax;

    // Pitch (relative to 1.0)
    double pitch = 1.0;
    std::optional<double> pitchRandomMin;
    std::optional<double> pitchRandomMax;

    // Retrigger
    bool enableRetrigger = false;
    std::optional<double> retriggerIntervalMin;
    std::optional<double> retriggerIntervalMax;

    // Positioning
    std::optional<Vector3> position;
    bool useWorldPosition = false;
    bool positionRelativeToPlayer = false;
    std::optional<double> randomizePositionMinRadius;
    std::optional<double> randomizePositionMaxRadius;
    std::optional<bool> randomizePositionHemisphere;

    // Children & Hierarchy
    bool enableChildEvents = false;
    QStringList childEvents;
    bool setChildPosition = false;

    // DSP & Reverb
    QString dspPreset;
    bool overrideDspPreset = false;
    std::optional<double> reverbWet;
    bool restrictSourceReverb = true;
    double distanceEffectMix = 0.0;

    // Curves
    bool useDistanceVolumeMappingCurve = false;
    std::vector<CurveControlPoint> distanceVolumeMappingCurve;

    bool useTimeVolumeMappingCurve = false;
    std::vector<CurveControlPoint> timeVolumeMappingCurve;
    std::vector<CurveControlPoint> fadetimeVolumeMappingCurve;

    // Audio files
    QStringList vsndFiles;
};

} // namespace Domain::Audio

