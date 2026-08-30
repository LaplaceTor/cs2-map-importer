#include "Domain/Audio/SoundLevelMapper.h"
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

namespace Domain::Audio {

std::optional<int> SoundLevelMapper::parseSoundLevelToDecibels(const QString& soundLevelStr) {
    const QString s = soundLevelStr.trimmed().toUpper();
    if (s.isEmpty()) {
        return std::nullopt;
    }

    // Symbolic SNDLVL constants
    if (s == QStringLiteral("SNDLVL_NONE")) return 0;
    if (s == QStringLiteral("SNDLVL_20DB")) return 20;
    if (s == QStringLiteral("SNDLVL_25DB")) return 25;
    if (s == QStringLiteral("SNDLVL_30DB")) return 30;
    if (s == QStringLiteral("SNDLVL_35DB")) return 35;
    if (s == QStringLiteral("SNDLVL_40DB")) return 40;
    if (s == QStringLiteral("SNDLVL_45DB")) return 45;
    if (s == QStringLiteral("SNDLVL_50DB")) return 50;
    if (s == QStringLiteral("SNDLVL_55DB")) return 55;
    if (s == QStringLiteral("SNDLVL_IDLE") || s == QStringLiteral("SNDLVL_TALKING") || s == QStringLiteral("SNDLVL_60DB")) return 60;
    if (s == QStringLiteral("SNDLVL_STATIC") || s == QStringLiteral("SNDLVL_65DB")) return 65;
    if (s == QStringLiteral("SNDLVL_70DB")) return 70;
    if (s == QStringLiteral("SNDLVL_NORM") || s == QStringLiteral("SNDLVL_75DB")) return 75;
    if (s == QStringLiteral("SNDLVL_80DB")) return 80;
    if (s == QStringLiteral("SNDLVL_85DB")) return 85;
    if (s == QStringLiteral("SNDLVL_90DB")) return 90;
    if (s == QStringLiteral("SNDLVL_95DB")) return 95;
    if (s == QStringLiteral("SNDLVL_100DB")) return 100;
    if (s == QStringLiteral("SNDLVL_105DB")) return 105;
    if (s == QStringLiteral("SNDLVL_120DB")) return 120;
    if (s == QStringLiteral("SNDLVL_130DB")) return 130;
    if (s == QStringLiteral("SNDLVL_GUNFIRE") || s == QStringLiteral("SNDLVL_140DB")) return 140;
    if (s == QStringLiteral("SNDLVL_150DB")) return 150;

    // Attenuation constants
    if (s == QStringLiteral("ATTN_NONE")) return 0;
    if (s == QStringLiteral("ATTN_NORM")) return 75;
    if (s == QStringLiteral("ATTN_IDLE")) return 60;
    if (s == QStringLiteral("ATTN_STATIC")) return 66;
    if (s == QStringLiteral("ATTN_RICOCHET")) return 65;
    if (s == QStringLiteral("ATTN_GUNFIRE")) return 140;

    // Regex extraction for formats like "SNDLVL_75dB", "75dB", "75"
    static const QRegularExpression dbRegex(QStringLiteral("(\\d+)(?:\\s*DB)?"), QRegularExpression::CaseInsensitiveOption);
    const auto match = dbRegex.match(s);
    if (match.hasMatch()) {
        bool ok = false;
        int val = match.captured(1).toInt(&ok);
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

