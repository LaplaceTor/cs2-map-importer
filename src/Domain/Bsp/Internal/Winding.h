#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <QString>
#include <QVarLengthArray>

#include "Domain/Bsp/Internal/BspMath.h"
#include "Domain/Bsp/Internal/BspStructs.h"

namespace Domain::Bsp::Internal {

/**
 * @brief 3D convex polygon winding and clipping utility.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.Winding.
 */
class Winding {
public:
    static constexpr int SIDE_FRONT = 0;
    static constexpr int SIDE_BACK = 1;
    static constexpr int SIDE_ON = 2;

    static constexpr double EPS_SPLIT = 0.01f;
    static constexpr double EPS_COMP = 0.5f;
    static constexpr double EPS_DEGEN = 0.1f;

    Winding() = default;
    explicit Winding(QVarLengthArray<Vector3d, 16> verts);
    explicit Winding(std::span<const Vector3d> verts);

    const QVarLengthArray<Vector3d, 16>& vertices() const noexcept { return m_verts; }
    std::size_t size() const noexcept { return static_cast<std::size_t>(m_verts.size()); }
    bool isEmpty() const noexcept { return m_verts.isEmpty(); }
    const Vector3d& operator[](std::size_t i) const { return m_verts[static_cast<qsizetype>(i)]; }

    auto begin() const noexcept { return m_verts.begin(); }
    auto end() const noexcept { return m_verts.end(); }

    Winding clipEpsilon(const Vector3d& normal, double dist, double eps, bool back) const;
    Winding clipPlane(const DPlane& pl, bool back) const;
    Winding clipWinding(const Winding& other, const Vector3d& projNormal) const;

    Winding& removeDegeneratedInPlace();
    Winding& removeCollinearInPlace();
    Winding& rotateInPlace(const Vector3d& angles);
    Winding& translateInPlace(const Vector3d& offset);

    Winding removeDegenerated() const;
    Winding removeCollinear() const;

    Winding rotate(const Vector3d& angles) const;
    Winding translate(const Vector3d& offset) const;
    Winding addBackface() const;

    bool matches(const Winding& that) const;
    bool isInside(const Vector3d& pt) const;

    AABB bounds() const;
    Vector3d center() const;
    std::array<Vector3d, 3> buildPlane() const;
    bool hasDuplicates() const;
    double area() const;

    QString toString() const;

    bool operator==(const Winding& other) const = default;

private:
    QVarLengthArray<Vector3d, 16> m_verts;
};

/**
 * @brief Cached factory for constructing windings from BSP geometry primitives.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.WindingFactory.
 */
class WindingFactory {
public:
    int maxLen = 0;
    int maxCoord = 0;

    explicit WindingFactory(int coordSize);
    static WindingFactory forAppId(int appId);

    const Winding& fromFace(const BspData& bsp, int faceIndex, bool isOrigFace);
    const Winding& fromSide(const BspData& bsp, int brushIndex, int brushSideIndex);
    const Winding& fromAreaportal(const BspData& bsp, int areaportalIndex);
    const Winding& fromOccluder(const BspData& bsp, int occluderPolyIndex);
    const Winding& fromPlane(const BspData& bsp, int planeIndex);
    Winding fromPlane(const DPlane& pl) const;

    bool isHuge(const Winding& winding) const noexcept;
    bool isHuge(const Vector3f& v) const noexcept;
    bool isHuge(const Vector3d& v) const noexcept;

private:
    std::vector<std::optional<Winding>> m_faceCache;
    std::vector<std::optional<Winding>> m_origFaceCache;
    std::vector<std::optional<Winding>> m_brushSideCache;
    std::vector<std::optional<Winding>> m_areaportalCache;
    std::vector<std::optional<Winding>> m_occluderCache;
    std::vector<std::optional<Winding>> m_planeCache;
};

/**
 * @brief BSP tree walker to find leaf brush/face index extrema.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.BspTreeStats.
 */
class BspTreeStats {
public:
    explicit BspTreeStats(const BspData& bsp);

    void reset() noexcept;
    void walk(int nodeIndex);

    int minBrushLeaf() const noexcept { return m_bmin; }
    int maxBrushLeaf() const noexcept { return m_bmax; }
    int minFaceLeaf() const noexcept { return m_fmin; }
    int maxFaceLeaf() const noexcept { return m_fmax; }
    int maxNode() const noexcept { return m_nmax; }

private:
    const BspData& m_bsp;
    int m_bmin = 0;
    int m_bmax = -1;
    int m_fmin = 0;
    int m_fmax = -1;
    int m_nmax = -1;
};

/**
 * @brief Cached AABB calculator for BSP brushes.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.geom.BrushBounds.
 */
class BrushBounds {
public:
    explicit BrushBounds(WindingFactory& wf);

    AABB bounds(const BspData& bsp, int brushIndex);

private:
    WindingFactory& m_wf;
    std::vector<std::optional<AABB>> m_aabbCache;
};

} // namespace Domain::Bsp::Internal
