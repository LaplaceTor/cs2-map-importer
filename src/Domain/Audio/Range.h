#pragma once

#include <QString>
#include <QStringView>
#include <optional>
#include <cmath>
#include <type_traits>

namespace Domain::Audio {

namespace detail {

template <typename Callback>
inline void forEachCommaOrSpaceToken(QStringView view, Callback&& fn) {
    const qsizetype len = view.size();
    qsizetype i = 0;
    while (i < len) {
        while (i < len && (view[i] == u',' || view[i].isSpace())) {
            ++i;
        }
        if (i >= len) {
            break;
        }
        const qsizetype start = i;
        while (i < len && view[i] != u',' && !view[i].isSpace()) {
            ++i;
        }
        if (!fn(view.sliced(start, i - start))) {
            break;
        }
    }
}

} // namespace detail

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
        QStringView clean = QStringView(str).trimmed();
        if (clean.endsWith(u';')) {
            clean.chop(1);
            clean = clean.trimmed();
        }

        double coords[3] = {0.0, 0.0, 0.0};
        int count = 0;
        bool parseOk = true;
        detail::forEachCommaOrSpaceToken(clean, [&](QStringView token) -> bool {
            if (count >= 3) {
                ++count;
                return false;
            }
            bool ok = false;
            coords[count] = token.toDouble(&ok);
            if (!ok) {
                parseOk = false;
                return false;
            }
            ++count;
            return true;
        });

        if (!parseOk || count != 3) {
            return std::nullopt;
        }

        return Vector3(coords[0], coords[1], coords[2]);
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
        const QStringView clean = QStringView(str).trimmed();
        if (clean.isEmpty()) {
            return std::nullopt;
        }

        bool hasAny = false;
        bool parseOk = true;
        T minVal{};
        T maxVal{};

        detail::forEachCommaOrSpaceToken(clean, [&](QStringView token) -> bool {
            bool ok = false;
            const T val = parseValue(token, &ok);
            if (!ok) {
                parseOk = false;
                return false;
            }
            if (!hasAny) {
                minVal = val;
                maxVal = val;
                hasAny = true;
            } else {
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
            return true;
        });

        if (!parseOk || !hasAny) {
            return std::nullopt;
        }

        return Range<T>(minVal, maxVal);
    }

private:
    static T parseValue(QStringView s, bool* ok) {
        const QStringView trimmed = s.trimmed();
        if (trimmed.startsWith(u'.') || trimmed.startsWith(QStringView(u"-."))) {
            QString normalized = trimmed.toString();
            if (normalized.startsWith(u'.')) {
                normalized.prepend(u'0');
            } else {
                normalized.replace(0, 2, QStringLiteral("-0."));
            }
            if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(normalized.toDouble(ok));
            } else {
                return static_cast<T>(normalized.toLongLong(ok));
            }
        }

        if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(trimmed.toDouble(ok));
        } else {
            return static_cast<T>(trimmed.toLongLong(ok));
        }
    }

    T m_min;
    T m_max;
};

using DoubleRange = Range<double>;
using IntRange = Range<int>;

} // namespace Domain::Audio

