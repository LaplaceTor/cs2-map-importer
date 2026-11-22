#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>

#include <QString>
#include <QStringView>

namespace Domain::Bsp::Internal {

inline constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

struct Vector2f {
    float x = 0.0f;
    float y = 0.0f;

    static const Vector2f kNull;
    static const Vector2f kMaxValue;
    static const Vector2f kMinValue;

    constexpr Vector2f() noexcept = default;
    constexpr Vector2f(float xVal, float yVal) noexcept : x(xVal), y(yVal) {}

    constexpr float get(int index) const noexcept {
        return index == 0 ? x : (index == 1 ? y : 0.0f);
    }

    constexpr float operator[](int index) const noexcept {
        return get(index);
    }

    constexpr Vector2f with(int index, float val) const noexcept {
        return index == 0 ? Vector2f{val, y} : (index == 1 ? Vector2f{x, val} : *this);
    }

    constexpr Vector2f withX(float val) const noexcept { return Vector2f{val, y}; }
    constexpr Vector2f withY(float val) const noexcept { return Vector2f{x, val}; }

    constexpr Vector2f add(float v) const noexcept { return Vector2f{x + v, y + v}; }
    constexpr Vector2f add(const Vector2f& that) const noexcept { return Vector2f{x + that.x, y + that.y}; }
    constexpr Vector2f sub(float v) const noexcept { return Vector2f{x - v, y - v}; }
    constexpr Vector2f sub(const Vector2f& that) const noexcept { return Vector2f{x - that.x, y - that.y}; }
    constexpr Vector2f scalar(float s) const noexcept { return Vector2f{x * s, y * s}; }
    constexpr Vector2f scalar(const Vector2f& that) const noexcept { return Vector2f{x * that.x, y * that.y}; }

    constexpr float dot(const Vector2f& that) const noexcept { return x * that.x + y * that.y; }
    constexpr float cross(const Vector2f& that) const noexcept { return x * that.y - that.x * y; }

    Vector2f min(const Vector2f& that) const noexcept {
        return Vector2f{std::min(x, that.x), std::min(y, that.y)};
    }

    Vector2f max(const Vector2f& that) const noexcept {
        return Vector2f{std::max(x, that.x), std::max(y, that.y)};
    }

    float length() const noexcept {
        return static_cast<float>(std::sqrt(static_cast<double>(x) * x + static_cast<double>(y) * y));
    }

    Vector2f normalize() const noexcept {
        const float len = length();
        return Vector2f{x / len, y / len};
    }

    Vector2f rotate(float angle) const noexcept {
        angle = std::fmod(angle, 360.0f);
        if (angle == 0.0f) {
            return *this;
        }
        if (angle == 90.0f) {
            return Vector2f{-y, x};
        }
        if (angle == 180.0f) {
            return Vector2f{-x, -y};
        }
        if (angle == 270.0f) {
            return Vector2f{y, -x};
        }

        const double radians = static_cast<double>(angle) * kDegToRad;
        const double r = std::hypot(static_cast<double>(x), static_cast<double>(y));
        const double theta = std::atan2(static_cast<double>(y), static_cast<double>(x));
        return Vector2f{
            static_cast<float>(r * std::cos(theta + radians)),
            static_cast<float>(r * std::sin(theta + radians))
        };
    }

    bool isNaN() const noexcept { return std::isnan(x) || std::isnan(y); }
    bool isInfinite() const noexcept { return std::isinf(x) || std::isinf(y); }
    bool isValid() const noexcept { return !isNaN() && !isInfinite(); }

    constexpr bool operator==(const Vector2f& that) const noexcept = default;
    constexpr bool operator!=(const Vector2f& that) const noexcept = default;
};

inline constexpr Vector2f Vector2f::kNull{0.0f, 0.0f};
inline constexpr Vector2f Vector2f::kMaxValue{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
inline constexpr Vector2f Vector2f::kMinValue{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    static const Vector3d kNull;
    static const Vector3d kMaxValue;
    static const Vector3d kMinValue;
    static const Vector3d kBaseVectorX;
    static const Vector3d kBaseVectorY;
    static const Vector3d kBaseVectorZ;

    constexpr Vector3d() noexcept = default;
    constexpr Vector3d(double xVal, double yVal, double zVal) noexcept : x(xVal), y(yVal), z(zVal) {}

    constexpr double get(int index) const noexcept {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : 0.0));
    }

    constexpr double operator[](int index) const noexcept {
        return get(index);
    }

    constexpr Vector3d with(int index, double val) const noexcept {
        if (index == 0) {
            return Vector3d{val, y, z};
        }
        if (index == 1) {
            return Vector3d{x, val, z};
        }
        if (index == 2) {
            return Vector3d{x, y, val};
        }
        return *this;
    }

    constexpr Vector3d withX(double val) const noexcept { return Vector3d{val, y, z}; }
    constexpr Vector3d withY(double val) const noexcept { return Vector3d{x, val, z}; }
    constexpr Vector3d withZ(double val) const noexcept { return Vector3d{x, y, val}; }

    constexpr double dot(const Vector3d& that) const noexcept {
        return x * that.x + y * that.y + z * that.z;
    }

    constexpr Vector3d cross(const Vector3d& that) const noexcept {
        return Vector3d{
            y * that.z - z * that.y,
            z * that.x - x * that.z,
            x * that.y - y * that.x
        };
    }

    double length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3d normalize() const noexcept {
        const double len = length();
        return Vector3d{x / len, y / len, z / len};
    }

    constexpr Vector3d add(double v) const noexcept { return Vector3d{x + v, y + v, z + v}; }
    constexpr Vector3d add(const Vector3d& that) const noexcept { return Vector3d{x + that.x, y + that.y, z + that.z}; }
    constexpr Vector3d sub(double v) const noexcept { return Vector3d{x - v, y - v, z - v}; }
    constexpr Vector3d sub(const Vector3d& that) const noexcept { return Vector3d{x - that.x, y - that.y, z - that.z}; }
    constexpr Vector3d scalar(double s) const noexcept { return Vector3d{x * s, y * s, z * s}; }
    constexpr Vector3d scalar(const Vector3d& that) const noexcept { return Vector3d{x * that.x, y * that.y, z * that.z}; }

    Vector3d min(double v) const noexcept { return Vector3d{std::min(x, v), std::min(y, v), std::min(z, v)}; }
    Vector3d min(const Vector3d& that) const noexcept {
        return Vector3d{std::min(x, that.x), std::min(y, that.y), std::min(z, that.z)};
    }

    Vector3d max(double v) const noexcept { return Vector3d{std::max(x, v), std::max(y, v), std::max(z, v)}; }
    Vector3d max(const Vector3d& that) const noexcept {
        return Vector3d{std::max(x, that.x), std::max(y, that.y), std::max(z, that.z)};
    }

    /// Performs an extrinsic (X-Y-Z) rotation sequence in a right-handed coordinate system.
    /// angles (x, y, z) are rotation angles in degrees around the X, Y, Z axes.
    Vector3d rotate(const Vector3d& angles) const noexcept {
        if (angles.x == 0.0 && angles.y == 0.0 && angles.z == 0.0) {
            return *this;
        }

        const double phiX = angles.x * kDegToRad;
        const double phiY = angles.y * kDegToRad;
        const double phiZ = angles.z * kDegToRad;

        const double cx = std::cos(phiX);
        const double sx = std::sin(phiX);
        const double cy = std::cos(phiY);
        const double sy = std::sin(phiY);
        const double cz = std::cos(phiZ);
        const double sz = std::sin(phiZ);

        const double r00 = cz * cy;
        const double r01 = cz * sy * sx - sz * cx;
        const double r02 = cz * sy * cx + sz * sx;
        const double r10 = sz * cy;
        const double r11 = sz * sy * sx + cz * cx;
        const double r12 = sz * sy * cx - cz * sx;
        const double r20 = -sy;
        const double r21 = cy * sx;
        const double r22 = cy * cx;

        return Vector3d{
            r00 * x + r01 * y + r02 * z,
            r10 * x + r11 * y + r12 * z,
            r20 * x + r21 * y + r22 * z
        };
    }

    bool isNaN() const noexcept { return std::isnan(x) || std::isnan(y) || std::isnan(z); }
    bool isInfinite() const noexcept { return std::isinf(x) || std::isinf(y) || std::isinf(z); }
    bool isValid() const noexcept { return !isNaN() && !isInfinite(); }

    constexpr bool operator==(const Vector3d& that) const noexcept = default;
    constexpr bool operator!=(const Vector3d& that) const noexcept = default;
};

inline constexpr Vector3d Vector3d::kNull{0.0, 0.0, 0.0};
inline constexpr Vector3d Vector3d::kMaxValue{
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max()
};
inline constexpr Vector3d Vector3d::kMinValue{
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max()
};
inline constexpr Vector3d Vector3d::kBaseVectorX{1.0, 0.0, 0.0};
inline constexpr Vector3d Vector3d::kBaseVectorY{0.0, 1.0, 0.0};
inline constexpr Vector3d Vector3d::kBaseVectorZ{0.0, 0.0, 1.0};

struct Vector3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static const Vector3f kNull;
    static const Vector3f kMaxValue;
    static const Vector3f kMinValue;
    static const Vector3f kBaseVectorX;
    static const Vector3f kBaseVectorY;
    static const Vector3f kBaseVectorZ;

    constexpr Vector3f() noexcept = default;
    constexpr Vector3f(float xVal, float yVal, float zVal) noexcept : x(xVal), y(yVal), z(zVal) {}

    constexpr Vector3d toDouble() const noexcept {
        return Vector3d{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)};
    }

    static constexpr Vector3f fromDouble(const Vector3d& v) noexcept {
        return Vector3f{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
    }

    constexpr float get(int index) const noexcept {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : 0.0f));
    }

    constexpr float operator[](int index) const noexcept {
        return get(index);
    }

    constexpr Vector3f with(int index, float val) const noexcept {
        if (index == 0) {
            return Vector3f{val, y, z};
        }
        if (index == 1) {
            return Vector3f{x, val, z};
        }
        if (index == 2) {
            return Vector3f{x, y, val};
        }
        return *this;
    }

    constexpr Vector3f withX(float val) const noexcept { return Vector3f{val, y, z}; }
    constexpr Vector3f withY(float val) const noexcept { return Vector3f{x, val, z}; }
    constexpr Vector3f withZ(float val) const noexcept { return Vector3f{x, y, val}; }

    constexpr float dot(const Vector3f& that) const noexcept {
        return x * that.x + y * that.y + z * that.z;
    }

    constexpr Vector3f cross(const Vector3f& that) const noexcept {
        return Vector3f{
            y * that.z - z * that.y,
            z * that.x - x * that.z,
            x * that.y - y * that.x
        };
    }

    float length() const noexcept {
        return static_cast<float>(std::sqrt(
            static_cast<double>(x) * x + static_cast<double>(y) * y + static_cast<double>(z) * z));
    }

    Vector3f normalize() const noexcept {
        const float len = length();
        return Vector3f{x / len, y / len, z / len};
    }

    constexpr Vector3f add(float v) const noexcept { return Vector3f{x + v, y + v, z + v}; }
    constexpr Vector3f add(const Vector3f& that) const noexcept { return Vector3f{x + that.x, y + that.y, z + that.z}; }
    constexpr Vector3f sub(float v) const noexcept { return Vector3f{x - v, y - v, z - v}; }
    constexpr Vector3f sub(const Vector3f& that) const noexcept { return Vector3f{x - that.x, y - that.y, z - that.z}; }
    constexpr Vector3f scalar(float s) const noexcept { return Vector3f{x * s, y * s, z * s}; }
    constexpr Vector3f scalar(const Vector3f& that) const noexcept { return Vector3f{x * that.x, y * that.y, z * that.z}; }

    Vector3f min(float v) const noexcept { return Vector3f{std::min(x, v), std::min(y, v), std::min(z, v)}; }
    Vector3f min(const Vector3f& that) const noexcept {
        return Vector3f{std::min(x, that.x), std::min(y, that.y), std::min(z, that.z)};
    }

    Vector3f max(float v) const noexcept { return Vector3f{std::max(x, v), std::max(y, v), std::max(z, v)}; }
    Vector3f max(const Vector3f& that) const noexcept {
        return Vector3f{std::max(x, that.x), std::max(y, that.y), std::max(z, that.z)};
    }

    Vector3f rotate(const Vector3f& angles) const noexcept {
        if (angles.x == 0.0f && angles.y == 0.0f && angles.z == 0.0f) {
            return *this;
        }

        const double phiX = static_cast<double>(angles.x) * kDegToRad;
        const double phiY = static_cast<double>(angles.y) * kDegToRad;
        const double phiZ = static_cast<double>(angles.z) * kDegToRad;

        const double cx = std::cos(phiX);
        const double sx = std::sin(phiX);
        const double cy = std::cos(phiY);
        const double sy = std::sin(phiY);
        const double cz = std::cos(phiZ);
        const double sz = std::sin(phiZ);

        const double r00 = cz * cy;
        const double r01 = cz * sy * sx - sz * cx;
        const double r02 = cz * sy * cx + sz * sx;
        const double r10 = sz * cy;
        const double r11 = sz * sy * sx + cz * cx;
        const double r12 = sz * sy * cx - cz * sx;
        const double r20 = -sy;
        const double r21 = cy * sx;
        const double r22 = cy * cx;

        return Vector3f{
            static_cast<float>(r00 * x + r01 * y + r02 * z),
            static_cast<float>(r10 * x + r11 * y + r12 * z),
            static_cast<float>(r20 * x + r21 * y + r22 * z)
        };
    }

    constexpr Vector2f projectOnPlane(const Vector3f& origin, const Vector3f& axis1, const Vector3f& axis2) const noexcept {
        const Vector3f delta = sub(origin);
        return Vector2f{axis1.dot(delta), axis2.dot(delta)};
    }

    bool isNaN() const noexcept { return std::isnan(x) || std::isnan(y) || std::isnan(z); }
    bool isInfinite() const noexcept { return std::isinf(x) || std::isinf(y) || std::isinf(z); }
    bool isValid() const noexcept { return !isNaN() && !isInfinite(); }

    constexpr bool operator==(const Vector3f& that) const noexcept = default;
    constexpr bool operator!=(const Vector3f& that) const noexcept = default;
};

inline constexpr Vector3f Vector3f::kNull{0.0f, 0.0f, 0.0f};
inline constexpr Vector3f Vector3f::kMaxValue{
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max()
};
inline constexpr Vector3f Vector3f::kMinValue{
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max()
};
inline constexpr Vector3f Vector3f::kBaseVectorX{1.0f, 0.0f, 0.0f};
inline constexpr Vector3f Vector3f::kBaseVectorY{0.0f, 1.0f, 0.0f};
inline constexpr Vector3f Vector3f::kBaseVectorZ{0.0f, 0.0f, 1.0f};

struct Vector4f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    static const Vector4f kNull;
    static const Vector4f kMaxValue;
    static const Vector4f kMinValue;

    constexpr Vector4f() noexcept = default;
    constexpr Vector4f(float xVal, float yVal, float zVal, float wVal) noexcept
        : x(xVal), y(yVal), z(zVal), w(wVal) {}

    constexpr float get(int index) const noexcept {
        switch (index) {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        case 3: return w;
        default: return 0.0f;
        }
    }

    constexpr float operator[](int index) const noexcept {
        return get(index);
    }

    constexpr Vector4f withX(float val) const noexcept { return Vector4f{val, y, z, w}; }
    constexpr Vector4f withY(float val) const noexcept { return Vector4f{x, val, z, w}; }
    constexpr Vector4f withZ(float val) const noexcept { return Vector4f{x, y, val, w}; }
    constexpr Vector4f withW(float val) const noexcept { return Vector4f{x, y, z, val}; }

    bool isNaN() const noexcept { return std::isnan(x) || std::isnan(y) || std::isnan(z) || std::isnan(w); }
    bool isInfinite() const noexcept { return std::isinf(x) || std::isinf(y) || std::isinf(z) || std::isinf(w); }
    bool isValid() const noexcept { return !isNaN() && !isInfinite(); }

    constexpr bool operator==(const Vector4f& that) const noexcept = default;
    constexpr bool operator!=(const Vector4f& that) const noexcept = default;
};

inline constexpr Vector4f Vector4f::kNull{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Vector4f Vector4f::kMaxValue{
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max(),
    std::numeric_limits<float>::max()
};
inline constexpr Vector4f Vector4f::kMinValue{
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max(),
    -std::numeric_limits<float>::max()
};

struct AABB {
    Vector3d min = Vector3d::kMaxValue;
    Vector3d max = Vector3d::kMinValue;

    static const AABB kZero;

    constexpr AABB() noexcept = default;
    constexpr AABB(const Vector3d& mins, const Vector3d& maxs) noexcept : min(mins), max(maxs) {}

    constexpr Vector3d getMin() const noexcept { return min; }
    constexpr Vector3d getMax() const noexcept { return max; }
    constexpr Vector3d getSize() const noexcept { return max.sub(min); }

    AABB include(const AABB& that) const noexcept {
        return AABB{min.min(that.min), max.max(that.max)};
    }

    constexpr AABB expand(double e) const noexcept {
        return expand(Vector3d{e, e, e});
    }

    constexpr AABB expand(const Vector3d& v) const noexcept {
        return AABB{min.sub(v), max.add(v)};
    }

    constexpr bool intersectsWith(const AABB& that) const noexcept {
        return that.max.x > min.x && that.min.x < max.x
            && that.max.y > min.y && that.min.y < max.y
            && that.max.z > min.z && that.min.z < max.z;
    }

    constexpr bool operator==(const AABB& that) const noexcept = default;
    constexpr bool operator!=(const AABB& that) const noexcept = default;
};

inline constexpr AABB AABB::kZero{Vector3d::kMaxValue, Vector3d::kMinValue};

struct AlphanumComparator {
    static int compare(QStringView s1, QStringView s2) noexcept {
        auto isAsciiDigit = [](QChar ch) noexcept {
            return ch >= u'0' && ch <= u'9';
        };

        auto nextChunk = [&](QStringView s, qsizetype& pos) noexcept -> QStringView {
            if (pos >= s.size()) {
                return {};
            }
            const qsizetype start = pos;
            const bool digitChunk = isAsciiDigit(s[pos]);
            ++pos;
            while (pos < s.size() && isAsciiDigit(s[pos]) == digitChunk) {
                ++pos;
            }
            return s.sliced(start, pos - start);
        };

        auto compareNumbers = [](QStringView n1, QStringView n2) noexcept -> int {
            qsizetype z1 = 0;
            while (z1 < n1.size() && n1[z1] == u'0') {
                ++z1;
            }
            qsizetype z2 = 0;
            while (z2 < n2.size() && n2[z2] == u'0') {
                ++z2;
            }

            if (z2 != z1) {
                return static_cast<int>(z2 - z1);
            }

            const QStringView trim1 = n1.sliced(z1);
            const QStringView trim2 = n2.sliced(z2);
            const bool leadingZero = (z1 + z2) > 0;

            if (!leadingZero && trim1.size() != trim2.size()) {
                return static_cast<int>(trim1.size() - trim2.size());
            }

            const qsizetype minLen = std::min(trim1.size(), trim2.size());
            for (qsizetype i = 0; i < minLen; ++i) {
                const int d1 = trim1[i].unicode() - u'0';
                const int d2 = trim2[i].unicode() - u'0';
                if (d1 != d2) {
                    return d1 - d2;
                }
            }
            return 0;
        };

        qsizetype p1 = 0;
        qsizetype p2 = 0;
        while (p1 < s1.size() && p2 < s2.size()) {
            const QStringView c1 = nextChunk(s1, p1);
            const QStringView c2 = nextChunk(s2, p2);

            int result = 0;
            if (isAsciiDigit(c1[0]) && isAsciiDigit(c2[0])) {
                result = compareNumbers(c1, c2);
            } else {
                result = c1.compare(c2);
            }

            if (result != 0) {
                return result;
            }
        }

        const bool has1 = p1 < s1.size();
        const bool has2 = p2 < s2.size();
        return (has1 ? 1 : 0) - (has2 ? 1 : 0);
    }

    bool operator()(const QString& a, const QString& b) const noexcept {
        return compare(a, b) < 0;
    }
};

class BinarySpanReader {
public:
    explicit BinarySpanReader(std::span<const std::byte> data, bool bigEndian = false) noexcept
        : m_data(data), m_bigEndian(bigEndian) {}

    std::size_t position() const noexcept { return m_pos; }

    void setPosition(std::size_t pos) {
        if (pos > m_data.size()) {
            throw std::out_of_range("BinarySpanReader::setPosition out of bounds");
        }
        m_pos = pos;
    }

    void seek(std::ptrdiff_t offset) {
        const std::ptrdiff_t next = static_cast<std::ptrdiff_t>(m_pos) + offset;
        if (next < 0 || static_cast<std::size_t>(next) > m_data.size()) {
            throw std::out_of_range("BinarySpanReader::seek out of bounds");
        }
        m_pos = static_cast<std::size_t>(next);
    }

    std::size_t remaining() const noexcept { return m_data.size() - m_pos; }
    bool hasRemaining() const noexcept { return m_pos < m_data.size(); }
    std::size_t size() const noexcept { return m_data.size(); }
    bool isBigEndian() const noexcept { return m_bigEndian; }
    void setBigEndian(bool bigEndian) noexcept { m_bigEndian = bigEndian; }

    std::int8_t readInt8() {
        return static_cast<std::int8_t>(readUInt8());
    }

    std::uint8_t readUInt8() {
        ensureAvailable(1);
        return std::to_integer<std::uint8_t>(m_data[m_pos++]);
    }

    std::int16_t readInt16() {
        return static_cast<std::int16_t>(readUInt16());
    }

    std::uint16_t readUInt16() {
        ensureAvailable(2);
        const std::uint16_t b0 = std::to_integer<std::uint16_t>(m_data[m_pos]);
        const std::uint16_t b1 = std::to_integer<std::uint16_t>(m_data[m_pos + 1]);
        m_pos += 2;
        if (m_bigEndian) {
            return static_cast<std::uint16_t>((b0 << 8) | b1);
        }
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }

    std::int32_t readInt32() {
        return static_cast<std::int32_t>(readUInt32());
    }

    std::uint32_t readUInt32() {
        ensureAvailable(4);
        const std::uint32_t b0 = std::to_integer<std::uint32_t>(m_data[m_pos]);
        const std::uint32_t b1 = std::to_integer<std::uint32_t>(m_data[m_pos + 1]);
        const std::uint32_t b2 = std::to_integer<std::uint32_t>(m_data[m_pos + 2]);
        const std::uint32_t b3 = std::to_integer<std::uint32_t>(m_data[m_pos + 3]);
        m_pos += 4;
        if (m_bigEndian) {
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        }
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    std::int64_t readInt64() {
        return static_cast<std::int64_t>(readUInt64());
    }

    std::uint64_t readUInt64() {
        ensureAvailable(8);
        std::uint64_t val = 0;
        if (m_bigEndian) {
            for (std::size_t i = 0; i < 8; ++i) {
                val = (val << 8) | std::to_integer<std::uint64_t>(m_data[m_pos + i]);
            }
        } else {
            for (std::size_t i = 0; i < 8; ++i) {
                val |= std::to_integer<std::uint64_t>(m_data[m_pos + i]) << (i * 8);
            }
        }
        m_pos += 8;
        return val;
    }

    float readFloat() {
        return std::bit_cast<float>(readUInt32());
    }

    std::span<const std::byte> readBytes(std::size_t count) {
        ensureAvailable(count);
        auto slice = m_data.subspan(m_pos, count);
        m_pos += count;
        return slice;
    }

    QString readStringFixed(std::size_t length) {
        auto bytes = readBytes(length);
        std::size_t len = 0;
        while (len < bytes.size() && bytes[len] != std::byte{0}) {
            ++len;
        }
        return QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(len));
    }

    QString readStringNullTerminated(std::size_t maxLen = 4096) {
        const std::size_t avail = remaining();
        const std::size_t limit = std::min(avail, maxLen);
        std::size_t len = 0;
        while (len < limit && m_data[m_pos + len] != std::byte{0}) {
            ++len;
        }
        const QString result = QString::fromUtf8(
            reinterpret_cast<const char*>(m_data.data() + m_pos),
            static_cast<qsizetype>(len));
        m_pos += len;
        if (len < limit && m_pos < m_data.size() && m_data[m_pos] == std::byte{0}) {
            ++m_pos;
        }
        return result;
    }

    Vector3f readVector3f() {
        const float x = readFloat();
        const float y = readFloat();
        const float z = readFloat();
        return Vector3f{x, y, z};
    }

    Vector3f readVector3iAsFloat() {
        const std::int32_t x = readInt32();
        const std::int32_t y = readInt32();
        const std::int32_t z = readInt32();
        return Vector3f{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    }

    Vector4f readVector4f() {
        const float x = readFloat();
        const float y = readFloat();
        const float z = readFloat();
        const float w = readFloat();
        return Vector4f{x, y, z, w};
    }

private:
    void ensureAvailable(std::size_t count) const {
        if (count > remaining()) {
            throw std::out_of_range("BinarySpanReader: unexpected end of buffer");
        }
    }

    std::span<const std::byte> m_data;
    std::size_t m_pos = 0;
    bool m_bigEndian = false;
};

} // namespace Domain::Bsp::Internal
