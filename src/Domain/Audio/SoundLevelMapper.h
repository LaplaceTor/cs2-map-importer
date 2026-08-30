#pragma once

#include <QString>
#include <vector>
#include <optional>

namespace Domain::Audio {

/**
 * @brief Represents a 6-element curve control point in Source 2 KeyValues3 curve definitions:
 * [ x, y, tangent_in, tangent_out, mode_in, mode_out ]
 */
struct CurveControlPoint {
    double x = 0.0;
    double y = 1.0;
    double slopeIn = 0.0;
    double slopeOut = 0.0;
    double modeIn = 2.0;
    double modeOut = 3.0;

    constexpr CurveControlPoint() noexcept = default;
    constexpr CurveControlPoint(double x_, double y_, double slopeIn_ = 0.0, double slopeOut_ = 0.0,
                                double modeIn_ = 2.0, double modeOut_ = 3.0) noexcept
        : x(x_), y(y_), slopeIn(slopeIn_), slopeOut(slopeOut_), modeIn(modeIn_), modeOut(modeOut_) {}

    bool operator==(const CurveControlPoint& other) const noexcept {
        return x == other.x && y == other.y &&
               slopeIn == other.slopeIn && slopeOut == other.slopeOut &&
               modeIn == other.modeIn && modeOut == other.modeOut;
    }
};

class SoundLevelMapper {
public:
    /**
     * @brief Parses a Source 1 soundlevel string (e.g. "SNDLVL_75dB", "SNDLVL_NORM", "80", "80dB", "ATTN_NORM")
     *        into an integer decibel value.
     */
    static std::optional<int> parseSoundLevelToDecibels(const QString& soundLevelStr);

    /**
     * @brief Calculates the maximum audible distance (in Hammer units) for a given dB soundlevel.
     */
    static double decibelsToMaxDistance(int decibels);

    /**
     * @brief Generates standard Source 2 distance volume mapping curve points for a given soundlevel string or dB.
     */
    static std::vector<CurveControlPoint> createDistanceVolumeCurve(int decibels);
    static std::vector<CurveControlPoint> createDistanceVolumeCurve(const QString& soundLevelStr);

    /**
     * @brief Generates fade in time volume mapping curve points for a given fadetime in seconds.
     */
    static std::vector<CurveControlPoint> createTimeVolumeCurve(double fadeTimeSeconds);

    /**
     * @brief Generates fade out volume mapping curve points for a given fadetime in seconds.
     */
    static std::vector<CurveControlPoint> createFadeTimeVolumeCurve(double fadeTimeSeconds);
};

} // namespace Domain::Audio

