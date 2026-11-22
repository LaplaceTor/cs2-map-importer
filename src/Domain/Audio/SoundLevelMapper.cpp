#include "Domain/Audio/SoundLevelMapper.h"
#include <QLatin1StringView>
#include <QRegularExpression>
#include <QStringView>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Domain::Audio {

namespace {

constexpr auto kSoundLevelTable = std::to_array<std::pair<QLatin1StringView, int>>({
    // Symbolic SNDLVL constants
    {QLatin1StringView("SNDLVL_NONE"),    0},
    {QLatin1StringView("SNDLVL_20DB"),    20},
    {QLatin1StringView("SNDLVL_25DB"),    25},
    {QLatin1StringView("SNDLVL_30DB"),    30},
    {QLatin1StringView("SNDLVL_35DB"),    35},
    {QLatin1StringView("SNDLVL_40DB"),    40},
    {QLatin1StringView("SNDLVL_45DB"),    45},
    {QLatin1StringView("SNDLVL_50DB"),    50},
    {QLatin1StringView("SNDLVL_55DB"),    55},
    {QLatin1StringView("SNDLVL_IDLE"),    60},
    {QLatin1StringView("SNDLVL_TALKING"), 60},
    {QLatin1StringView("SNDLVL_60DB"),    60},
    {QLatin1StringView("SNDLVL_STATIC"),  65},
    {QLatin1StringView("SNDLVL_65DB"),    65},
    {QLatin1StringView("SNDLVL_70DB"),    70},
    {QLatin1StringView("SNDLVL_NORM"),    75},
    {QLatin1StringView("SNDLVL_75DB"),    75},
    {QLatin1StringView("SNDLVL_80DB"),    80},
    {QLatin1StringView("SNDLVL_85DB"),    85},
    {QLatin1StringView("SNDLVL_90DB"),    90},
    {QLatin1StringView("SNDLVL_95DB"),    95},
    {QLatin1StringView("SNDLVL_100DB"),   100},
    {QLatin1StringView("SNDLVL_105DB"),   105},
    {QLatin1StringView("SNDLVL_120DB"),   120},
    {QLatin1StringView("SNDLVL_130DB"),   130},
    {QLatin1StringView("SNDLVL_GUNFIRE"), 140},
    {QLatin1StringView("SNDLVL_140DB"),   140},
    {QLatin1StringView("SNDLVL_150DB"),   150},

    // Attenuation constants
    {QLatin1StringView("ATTN_NONE"),      0},
    {QLatin1StringView("ATTN_NORM"),      75},
    {QLatin1StringView("ATTN_IDLE"),      60},
    {QLatin1StringView("ATTN_STATIC"),    66},
    {QLatin1StringView("ATTN_RICOCHET"),  65},
    {QLatin1StringView("ATTN_GUNFIRE"),   140},
});

} // namespace

std::optional<int> SoundLevelMapper::parseSoundLevelToDecibels(const QString& soundLevelStr) {
    const QStringView trimmed = QStringView(soundLevelStr).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    for (const auto& [name, db] : kSoundLevelTable) {
        if (trimmed.compare(name, Qt::CaseInsensitive) == 0) {
            return db;
        }
    }

    // Regex extraction for formats like "SNDLVL_75dB", "75dB", "75"
    static const QRegularExpression dbRegex(QStringLiteral("(\\d+)(?:\\s*DB)?"), QRegularExpression::CaseInsensitiveOption);
    const auto match = dbRegex.matchView(trimmed);
    if (match.hasMatch()) {
        bool ok = false;
        const int val = match.capturedView(1).toInt(&ok);
        if (ok) {
            return val;
        }
    }

    return std::nullopt;
}

double SoundLevelMapper::decibelsToMaxDistance(int decibels) {
    if (decibels <= 0) {
        return 0.0;
    }
    if (decibels <= 50) return 200.0;
    if (decibels <= 55) return 250.0;
    if (decibels <= 60) return 300.0;
    if (decibels <= 65) return 450.0;
    if (decibels <= 70) return 600.0;
    if (decibels <= 75) return 800.0;
    if (decibels <= 80) return 1000.0;
    if (decibels <= 85) return 1250.0;
    if (decibels <= 90) return 1500.0;
    if (decibels <= 95) return 1800.0;
    if (decibels <= 100) return 2200.0;
    if (decibels <= 105) return 2600.0;
    if (decibels <= 120) return 3500.0;
    if (decibels <= 130) return 4200.0;
    if (decibels <= 140) return 5000.0;
    return 6000.0;
}

std::vector<CurveControlPoint> SoundLevelMapper::createDistanceVolumeCurve(int decibels) {
    double maxDist = decibelsToMaxDistance(decibels);
    if (maxDist <= 0.0) {
        return {};
    }

    return {
        CurveControlPoint(0.0, 1.0, 0.0, 0.0, 2.0, 3.0),
        CurveControlPoint(maxDist, 0.0, 0.0, 0.0, 2.0, 3.0)
    };
}

std::vector<CurveControlPoint> SoundLevelMapper::createDistanceVolumeCurve(const QString& soundLevelStr) {
    auto db = parseSoundLevelToDecibels(soundLevelStr);
    if (!db.has_value() || *db <= 0) {
        return {};
    }
    return createDistanceVolumeCurve(*db);
}

std::vector<CurveControlPoint> SoundLevelMapper::createTimeVolumeCurve(double fadeTimeSeconds) {
    double ft = std::max(0.05, fadeTimeSeconds);
    return {
        CurveControlPoint(0.0, 0.0, 0.0, 0.0, 2.0, 3.0),
        CurveControlPoint(ft, 1.0, 0.0, 0.0, 2.0, 3.0)
    };
}

std::vector<CurveControlPoint> SoundLevelMapper::createFadeTimeVolumeCurve(double fadeTimeSeconds) {
    double ft = std::max(0.05, fadeTimeSeconds);
    return {
        CurveControlPoint(0.0, 1.0, 0.0, 0.0, 2.0, 3.0),
        CurveControlPoint(ft, 0.0, 0.0, 0.0, 2.0, 3.0)
    };
}

} // namespace Domain::Audio

