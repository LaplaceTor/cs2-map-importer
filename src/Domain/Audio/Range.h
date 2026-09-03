#pragma once

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <optional>
#include <cmath>
#include <type_traits>

namespace Domain::Audio {

/**
 * @brief Represents a 3D coordinate in game space.
 */
struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vector3() noexcept = default;
    constexpr Vector3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    bool isZero() const noexcept {
        return std::abs(x) < 1e-6 && std::abs(y) < 1e-6 && std::abs(z) < 1e-6;
    }

    bool operator==(const Vector3& other) const noexcept {
        return std::abs(x - other.x) < 1e-6 &&
               std::abs(y - other.y) < 1e-6 &&
               std::abs(z - other.z) < 1e-6;
    }

    bool operator!=(const Vector3& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Parses a 3D coordinate from string formats like "100, 200, 300", "100 200 300", or "100, 200, 300;".
     */
    static std::optional<Vector3> fromString(const QString& str) {
        QString clean = str.trimmed();
        if (clean.endsWith(';')) {
            clean.chop(1);
            clean = clean.trimmed();
        }

        const QStringList parts = clean.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
        if (parts.size() != 3) {
            return std::nullopt;
        }

        bool okX = false, okY = false, okZ = false;
        double x = parts[0].toDouble(&okX);
        double y = parts[1].toDouble(&okY);
        double z = parts[2].toDouble(&okZ);

        if (!okX || !okY || !okZ) {
            return std::nullopt;
        }

        return Vector3(x, y, z);
    }
};

/**
 * @brief Represents a numeric scalar or interval range [min, max].
 */
template <typename T>
class Range {
    static_assert(std::is_arithmetic_v<T>, "Range requires an arithmetic type");

public:
    constexpr Range() noexcept : m_min(0), m_max(0) {}
    constexpr explicit Range(T val) noexcept : m_min(val), m_max(val) {}
    constexpr Range(T minVal, T maxVal) noexcept : m_min(minVal), m_max(maxVal) {}

    constexpr T min() const noexcept { return m_min; }
    constexpr T max() const noexcept { return m_max; }

    constexpr void setMin(T val) noexcept { m_min = val; }
    constexpr void setMax(T val) noexcept { m_max = val; }
    constexpr void set(T val) noexcept { m_min = val; m_max = val; }
    constexpr void set(T minVal, T maxVal) noexcept { m_min = minVal; m_max = maxVal; }

    constexpr bool isFixed() const noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(m_min - m_max) < 1e-6;
        } else {
            return m_min == m_max;
        }
    }

    constexpr T delta() const noexcept {
        return m_max - m_min;
    }

    constexpr bool operator==(const Range<T>& other) const noexcept {
        if constexpr (std::is_floating_point_v<T>) {
            return std::abs(m_min - other.m_min) < 1e-6 &&
                   std::abs(m_max - other.m_max) < 1e-6;
        } else {
            return m_min == other.m_min && m_max == other.m_max;
        }
    }

    constexpr bool operator!=(const Range<T>& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Parses a range from string formats like "0.4", ".4", "0.1, 0.5", "85, 105", "13 35".
     */
    static std::optional<Range<T>> fromString(const QString& str) {
        QString clean = str.trimmed();
        if (clean.isEmpty()) {
            return std::nullopt;
        }

        const QStringList parts = clean.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            return std::nullopt;
        }

        if (parts.size() == 1) {
            bool ok = false;
            T val = parseValue(parts[0], &ok);
            if (!ok) {
                return std::nullopt;
            }
            return Range<T>(val);
        }

        if (parts.size() >= 2) {
            bool ok = false;
            T minVal = parseValue(parts[0], &ok);
            if (!ok) {
                return std::nullopt;
            }
            T maxVal = minVal;

            for (int i = 1; i < parts.size(); ++i) {
                bool okPart = false;
                T val = parseValue(parts[i], &okPart);
                if (!okPart) {
                    return std::nullopt;
                }
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
            return Range<T>(minVal, maxVal);
        }

        return std::nullopt;
    }

private:
    static T parseValue(const QString& s, bool* ok) {
        QString normalized = s.trimmed();
        if (normalized.startsWith(QLatin1Char('.'))) {
            normalized.prepend(QLatin1Char('0'));
        } else if (normalized.startsWith(QStringLiteral("-."))) {
            normalized.replace(0, 2, QStringLiteral("-0."));
        }

        if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(normalized.toDouble(ok));
        } else {
            return static_cast<T>(normalized.toLongLong(ok));
        }
    }

    T m_min;
    T m_max;
};

using DoubleRange = Range<double>;
using IntRange = Range<int>;

} // namespace Domain::Audio

