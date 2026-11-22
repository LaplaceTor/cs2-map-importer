#pragma once

#include <array>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <QString>

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
    explicit Winding(std::vector<Vector3d> verts);

    const std::vector<Vector3d>& vertices() const noexcept { return m_verts; }
    std::size_t size() const noexcept { return m_verts.size(); }
    bool isEmpty() const noexcept { return m_verts.empty(); }
    const Vector3d& get(std::size_t i) const noexcept { return m_verts[i]; }
    const Vector3d& operator[](std::size_t i) const noexcept { return m_verts[i]; }

    auto begin() const noexcept { return m_verts.begin(); }
    auto end() const noexcept { return m_verts.end(); }

    Winding clipEpsilon(const Vector3d& normal, double dist, double eps, bool back) const;
    Winding clipPlane(const DPlane& pl, bool back) const;
    Winding clipWinding(const Winding& other, const Vector3d& projNormal) const;

    Winding removeDegenerated() const;
    Winding removeCollinear() const;

    Winding rotate(const Vector3d& angles) const;
    Winding translate(const Vector3d& offset) const;
    Winding addBackface() const;

    bool matches(const Winding& that) const;
    bool isInside(const Vector3d& pt) const;

    AABB getBounds() const;
    Vector3d getCenter() const;
    std::array<Vector3d, 3> buildPlane() const;
    bool hasDuplicates() const;
    double getArea() const;

    QString toString() const;

    bool operator==(const Winding& other) const noexcept = default;

private:
    std::vector<Vector3d> m_verts;
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

    Winding fromFace(const BspData& bsp, int faceIndex, bool isOrigFace);
    Winding fromFace(const BspData& bsp, const DFace& face);

    Winding fromSide(const BspData& bsp, int brushIndex, int sideIndex);
    Winding fromSide(const BspData& bsp, const DBrush& brush, int sideIndex);
    Winding fromSide(const BspData& bsp, const DBrush& brush, const DBrushSide& bside);

    Winding fromAreaportal(const BspData& bsp, const DAreaportal& ap);
    Winding fromOccluder(const BspData& bsp, const DOccluderPolyData& opd);
    Winding fromPlane(const DPlane& pl);

    bool isHuge(const Winding& winding) const noexcept;
    bool isHuge(const Vector3f& v) const noexcept;
    bool isHuge(const Vector3d& v) const noexcept;

private:
    struct BrushSideKey {
        const DBrush* brush = nullptr;
        const DBrushSide* side = nullptr;

        bool operator==(const BrushSideKey& other) const noexcept = default;
    };

    struct BrushSideKeyHash {
        std::size_t operator()(const BrushSideKey& key) const noexcept;
    };

    std::unordered_map<const DFace*, Winding> m_faceCache;
    std::unordered_map<BrushSideKey, Winding, BrushSideKeyHash> m_brushSideCache;
    std::unordered_map<const DAreaportal*, Winding> m_areaportalCache;
    std::unordered_map<const DOccluderPolyData*, Winding> m_occluderCache;
    std::unordered_map<const DPlane*, Winding> m_planeCache;
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

    int getMinBrushLeaf() const noexcept { return m_bmin; }
    int getMaxBrushLeaf() const noexcept { return m_bmax; }
    int getMinFaceLeaf() const noexcept { return m_fmin; }
    int getMaxFaceLeaf() const noexcept { return m_fmax; }
    int getMaxNode() const noexcept { return m_nmax; }

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

    AABB getBounds(const BspData& bsp, const DBrush& brush);

private:
    WindingFactory& m_wf;
    std::unordered_map<const DBrush*, AABB> m_aabbCache;
};

} // namespace Domain::Bsp::Internal
