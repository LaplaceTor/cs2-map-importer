#include "Domain/Bsp/Internal/Winding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QStringList>

#include "Domain/Bsp/Internal/SourceAppDb.h"

namespace Domain::Bsp::Internal {

// ---------------------------------------------------------------------------
// Winding
// ---------------------------------------------------------------------------

Winding::Winding(std::vector<Vector3d> verts)
    : m_verts(std::move(verts)) {}

Winding Winding::clipEpsilon(const Vector3d& normal, double dist, double eps, bool back) const {
    const std::size_t size = m_verts.size();
    if (size == 0) {
        return Winding{};
    }

    int counts[3] = {0, 0, 0};
    std::vector<double> dists(size + 1, 0.0);
    std::vector<int> sides(size + 1, 0);

    for (std::size_t i = 0; i < size; ++i) {
        double dot = m_verts[i].dot(normal);
        dot -= dist;
        dists[i] = dot;

        if (dot > eps) {
            sides[i] = SIDE_FRONT;
        } else if (dot < -eps) {
            sides[i] = SIDE_BACK;
        } else {
            sides[i] = SIDE_ON;
        }

        counts[sides[i]]++;
    }

    sides[size] = sides[0];
    dists[size] = dists[0];

    if (counts[SIDE_FRONT] == 0) {
        if (!back) {
            if (counts[SIDE_ON] == 0) {
                return Winding{};
            }
        } else {
            return *this;
        }
    }
    if (counts[SIDE_BACK] == 0) {
        if (back) {
            if (counts[SIDE_ON] == 0) {
                return Winding{};
            }
        } else {
            return *this;
        }
    }

    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(size + 4);

    for (std::size_t i = 0; i < size; ++i) {
        const Vector3d& p1 = m_verts[i];

        if (sides[i] == SIDE_ON) {
            vertsNew.push_back(p1);
            continue;
        }

        if (sides[i] == SIDE_FRONT && !back) {
            vertsNew.push_back(p1);
        }

        if (sides[i] == SIDE_BACK && back) {
            vertsNew.push_back(p1);
        }

        if (sides[i + 1] == SIDE_ON) {
            continue;
        }

        if (sides[i + 1] == sides[i]) {
            continue;
        }

        const Vector3d& p2 = (i == size - 1) ? m_verts[0] : m_verts[i + 1];
        const double dot = dists[i] / (dists[i] - dists[i + 1]);

        Vector3d mv = Vector3d::kNull;
        for (int j = 0; j < 3; ++j) {
            if (normal.get(j) == 1.0) {
                mv = mv.with(j, dist);
            } else if (normal.get(j) == -1.0) {
                mv = mv.with(j, -dist);
            } else {
                mv = mv.with(j, p1.get(j) + dot * (p2.get(j) - p1.get(j)));
            }
        }

        vertsNew.push_back(mv);
    }

    return Winding{std::move(vertsNew)};
}

Winding Winding::clipPlane(const DPlane& pl, bool back) const {
    return clipEpsilon(pl.normal.toDouble(), pl.dist, EPS_SPLIT, back);
}

Winding Winding::clipWinding(const Winding& other, const Vector3d& projNormal) const {
    if (other.size() < 3) {
        return *this;
    }

    Winding result = *this;
    const std::size_t otherSize = other.size();
    for (std::size_t i = 0; i < otherSize; ++i) {
        const Vector3d edge = other.get((i + 1) % otherSize).sub(other.get(i));
        const Vector3d normal = edge.cross(projNormal).normalize();
        const double dist = normal.dot(other.get(i));

        result = result.clipEpsilon(normal, dist, EPS_SPLIT, false);
    }
    return result;
}

Winding Winding::removeDegenerated() const {
    if (m_verts.empty()) {
        return *this;
    }

    const std::size_t size = m_verts.size();
    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(size);

    for (std::size_t i = 0; i < size; ++i) {
        const std::size_t j = (i + 1) % size;
        if (m_verts[i].sub(m_verts[j]).length() > EPS_DEGEN) {
            vertsNew.push_back(m_verts[i]);
        }
    }

    return Winding{std::move(vertsNew)};
}

Winding Winding::removeCollinear() const {
    if (m_verts.empty()) {
        return *this;
    }

    const std::size_t size = m_verts.size();
    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(size);

    for (std::size_t i = 0; i < size; ++i) {
        const std::size_t j = (i + 1) % size;
        const std::size_t k = (i + size - 1) % size;
        const Vector3d v1 = m_verts[j].sub(m_verts[i]).normalize();
        const Vector3d v2 = m_verts[i].sub(m_verts[k]).normalize();

        if (v1.dot(v2) < 0.999) {
            vertsNew.push_back(m_verts[i]);
        }
    }

    return Winding{std::move(vertsNew)};
}

Winding Winding::rotate(const Vector3d& angles) const {
    if (m_verts.empty()) {
        return *this;
    }

    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(m_verts.size());
    for (const Vector3d& vert : m_verts) {
        vertsNew.push_back(vert.rotate(angles));
    }

    return Winding{std::move(vertsNew)};
}

Winding Winding::translate(const Vector3d& offset) const {
    if (m_verts.empty()) {
        return *this;
    }

    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(m_verts.size());
    for (const Vector3d& vert : m_verts) {
        vertsNew.push_back(vert.add(offset));
    }

    return Winding{std::move(vertsNew)};
}

Winding Winding::addBackface() const {
    if (m_verts.empty()) {
        return *this;
    }

    const std::size_t size = m_verts.size();
    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(size * 2);

    for (std::size_t i = 0; i < size; ++i) {
        if (i != 0) {
            vertsNew.push_back(m_verts[i]);
        }
        if (i != size) {
            vertsNew.push_back(m_verts[i]);
        }
    }

    return Winding{std::move(vertsNew)};
}

bool Winding::matches(const Winding& that) const {
    if (m_verts.size() != that.m_verts.size()) {
        return false;
    }

    for (const Vector3d& v1 : m_verts) {
        double minDist = 1e6f;
        for (const Vector3d& v2 : that.m_verts) {
            minDist = std::min(minDist, v1.sub(v2).length());
        }
        if (minDist > EPS_COMP) {
            return false;
        }
    }

    return true;
}

bool Winding::isInside(const Vector3d& pt) const {
    if (m_verts.size() < 2) {
        return false;
    }

    Vector3d toPt = pt.sub(m_verts[0]);
    Vector3d edge = m_verts[1].sub(m_verts[0]);
    const Vector3d testCross = edge.cross(toPt).normalize();

    const std::size_t size = m_verts.size();
    for (std::size_t i = 1; i < size; ++i) {
        toPt = pt.sub(m_verts[i]);
        edge = m_verts[(i + 1) % size].sub(m_verts[i]);
        const Vector3d cross = edge.cross(toPt).normalize();

        if (cross.dot(testCross) < 0.0) {
            return false;
        }
    }

    return true;
}

AABB Winding::getBounds() const {
    Vector3d mins = Vector3d::kMaxValue;
    Vector3d maxs = Vector3d::kMinValue;

    for (const Vector3d& vert : m_verts) {
        mins = mins.min(vert);
        maxs = maxs.max(vert);
    }

    return AABB{mins, maxs};
}

Vector3d Winding::getCenter() const {
    if (m_verts.empty()) {
        return Vector3d::kNull;
    }

    Vector3d sum = Vector3d::kNull;
    for (const Vector3d& vert : m_verts) {
        sum = sum.add(vert);
    }

    return sum.scalar(static_cast<double>(1.0f / static_cast<float>(m_verts.size())));
}

std::array<Vector3d, 3> Winding::buildPlane() const {
    if (m_verts.size() < 3) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const Vector3d invalid{nan, nan, nan};
        return {invalid, invalid, invalid};
    }

    const std::size_t size = m_verts.size();
    std::array<Vector3d, 3> plane{};
    plane[0] = m_verts[0];

    std::vector<Vector3d> vertsNew;
    vertsNew.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        vertsNew.push_back(m_verts[i].sub(plane[0]));
    }

    double maxmcp = -1.0;
    int imax = -1;
    int jmax = -1;

    for (std::size_t i = 1; i < size; ++i) {
        for (std::size_t j = i + 1; j < size; ++j) {
            const double mcp = vertsNew[i].cross(vertsNew[j]).length();
            if (mcp > maxmcp) {
                maxmcp = mcp;
                imax = static_cast<int>(i);
                jmax = static_cast<int>(j);
            }
        }
    }

    if (imax < 0 || jmax < 0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const Vector3d invalid{nan, nan, nan};
        return {invalid, invalid, invalid};
    }

    plane[1] = m_verts[static_cast<std::size_t>(imax)];
    plane[2] = m_verts[static_cast<std::size_t>(jmax)];

    return plane;
}

bool Winding::hasDuplicates() const {
    const std::size_t size = m_verts.size();
    for (std::size_t i = 0; i < size; ++i) {
        for (std::size_t j = 0; j < size; ++j) {
            if (i == j) {
                continue;
            }
            if (m_verts[i] == m_verts[j]) {
                return true;
            }
        }
    }
    return false;
}

double Winding::getArea() const {
    double total = 0.0;
    const std::size_t size = m_verts.size();
    for (std::size_t i = 2; i < size; ++i) {
        const Vector3d v1 = m_verts[i - 1].sub(m_verts[0]);
        const Vector3d v2 = m_verts[i].sub(m_verts[0]);
        total += v1.cross(v2).length();
    }
    return total * 0.5f;
}

QString Winding::toString() const {
    QStringList parts;
    parts.reserve(static_cast<qsizetype>(m_verts.size()));
    for (const Vector3d& v : m_verts) {
        parts.append(QStringLiteral("(%1, %2, %3)")
                         .arg(QString::number(v.x), QString::number(v.y), QString::number(v.z)));
    }
    return QStringLiteral("[") + parts.join(QStringLiteral(", ")) + QStringLiteral("]");
}

// ---------------------------------------------------------------------------
// WindingFactory
// ---------------------------------------------------------------------------

std::size_t WindingFactory::BrushSideKeyHash::operator()(const BrushSideKey& key) const noexcept {
    const auto h1 = std::hash<const DBrush*>{}(key.brush);
    const auto h2 = std::hash<const DBrushSide*>{}(key.side);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

WindingFactory::WindingFactory(int coordSize)
    : maxLen(static_cast<int>(std::ceil(std::sqrt(3.0) * coordSize)))
    , maxCoord(coordSize) {}

WindingFactory WindingFactory::forAppId(int appId) {
    const int coordSize = (appId == SourceAppId::kStrataSource) ? 131072 : 32768;
    return WindingFactory{coordSize};
}

Winding WindingFactory::fromFace(const BspData& bsp, int faceIndex, bool isOrigFace) {
    const auto& faceList = isOrigFace ? bsp.origFaces : bsp.faces;
    if (faceIndex < 0 || static_cast<std::size_t>(faceIndex) >= faceList.size()) {
        return Winding{};
    }
    return fromFace(bsp, faceList[static_cast<std::size_t>(faceIndex)]);
}

Winding WindingFactory::fromFace(const BspData& bsp, const DFace& face) {
    if (auto it = m_faceCache.find(&face); it != m_faceCache.end()) {
        return it->second;
    }

    if (face.numedge <= 0 || face.fstedge < 0
        || static_cast<std::size_t>(face.fstedge) + static_cast<std::size_t>(face.numedge) > bsp.surfEdges.size()) {
        return Winding{};
    }

    std::vector<Vector3d> verts;
    verts.reserve(static_cast<std::size_t>(face.numedge));

    for (int i = 0; i < face.numedge; ++i) {
        const int sedge = bsp.surfEdges[static_cast<std::size_t>(face.fstedge + i)];
        const int edgeIndex = (sedge < 0) ? -sedge : sedge;
        if (edgeIndex < 0 || static_cast<std::size_t>(edgeIndex) >= bsp.edges.size()) {
            return Winding{};
        }

        const int v = (sedge < 0)
            ? bsp.edges[static_cast<std::size_t>(edgeIndex)].v[1]
            : bsp.edges[static_cast<std::size_t>(edgeIndex)].v[0];
        if (v < 0 || static_cast<std::size_t>(v) >= bsp.verts.size()) {
            return Winding{};
        }

        verts.push_back(bsp.verts[static_cast<std::size_t>(v)].point.toDouble());
    }

    Winding w{std::move(verts)};
    m_faceCache.emplace(&face, w);
    return w;
}

Winding WindingFactory::fromSide(const BspData& bsp, int brushIndex, int sideIndex) {
    if (brushIndex < 0 || static_cast<std::size_t>(brushIndex) >= bsp.brushes.size()) {
        return Winding{};
    }
    return fromSide(bsp, bsp.brushes[static_cast<std::size_t>(brushIndex)], sideIndex);
}

Winding WindingFactory::fromSide(const BspData& bsp, const DBrush& brush, int sideIndex) {
    if (sideIndex < 0 || sideIndex >= brush.numside || brush.fstside < 0) {
        return Winding{};
    }
    const std::size_t ibside = static_cast<std::size_t>(brush.fstside + sideIndex);
    if (ibside >= bsp.brushSides.size()) {
        return Winding{};
    }
    return fromSide(bsp, brush, bsp.brushSides[ibside]);
}

Winding WindingFactory::fromSide(const BspData& bsp, const DBrush& brush, const DBrushSide& bside) {
    const BrushSideKey key{&brush, &bside};
    if (auto it = m_brushSideCache.find(key); it != m_brushSideCache.end()) {
        return it->second;
    }

    if (bside.pnum < 0 || static_cast<std::size_t>(bside.pnum) >= bsp.planes.size()) {
        return Winding{};
    }
    if (brush.fstside < 0 || brush.numside < 0
        || static_cast<std::size_t>(brush.fstside) + static_cast<std::size_t>(brush.numside) > bsp.brushSides.size()) {
        return Winding{};
    }

    Winding w = fromPlane(bsp.planes[static_cast<std::size_t>(bside.pnum)]);
    bool hasSide = false;

    for (int i = 0; i < brush.numside; ++i) {
        const std::size_t ibside2 = static_cast<std::size_t>(brush.fstside + i);
        const DBrushSide& bside2 = bsp.brushSides[ibside2];

        if (&bside2 == &bside) {
            hasSide = true;
            continue;
        }

        if (bside2.bevel) {
            continue;
        }

        if (bside2.pnum < 0 || static_cast<std::size_t>(bside2.pnum) >= bsp.planes.size()) {
            continue;
        }

        w = w.clipPlane(bsp.planes[static_cast<std::size_t>(bside2.pnum)], true);
    }

    if (!hasSide) {
        return Winding{};
    }

    m_brushSideCache.emplace(key, w);
    return w;
}

Winding WindingFactory::fromAreaportal(const BspData& bsp, const DAreaportal& ap) {
    if (auto it = m_areaportalCache.find(&ap); it != m_areaportalCache.end()) {
        return it->second;
    }

    if (ap.clipPortalVerts <= 0 || ap.firstClipPortalVert < 0
        || static_cast<std::size_t>(ap.firstClipPortalVert) + static_cast<std::size_t>(ap.clipPortalVerts)
               > bsp.clipPortalVerts.size()) {
        return Winding{};
    }

    std::vector<Vector3d> verts;
    verts.reserve(static_cast<std::size_t>(ap.clipPortalVerts));
    for (int i = 0; i < ap.clipPortalVerts; ++i) {
        verts.push_back(bsp.clipPortalVerts[static_cast<std::size_t>(ap.firstClipPortalVert + i)].point.toDouble());
    }

    Winding w{std::move(verts)};
    m_areaportalCache.emplace(&ap, w);
    return w;
}

Winding WindingFactory::fromOccluder(const BspData& bsp, const DOccluderPolyData& opd) {
    if (auto it = m_occluderCache.find(&opd); it != m_occluderCache.end()) {
        return it->second;
    }

    if (opd.vertexcount <= 0 || opd.firstvertexindex < 0
        || static_cast<std::size_t>(opd.firstvertexindex) + static_cast<std::size_t>(opd.vertexcount)
               > bsp.occluderVerts.size()) {
        return Winding{};
    }

    std::vector<Vector3d> verts;
    verts.reserve(static_cast<std::size_t>(opd.vertexcount));
    for (int k = 0; k < opd.vertexcount; ++k) {
        const int pvi = bsp.occluderVerts[static_cast<std::size_t>(opd.firstvertexindex + k)];
        if (pvi < 0 || static_cast<std::size_t>(pvi) >= bsp.verts.size()) {
            return Winding{};
        }
        verts.push_back(bsp.verts[static_cast<std::size_t>(pvi)].point.toDouble());
    }

    Winding w{std::move(verts)};
    m_occluderCache.emplace(&opd, w);
    return w;
}

Winding WindingFactory::fromPlane(const DPlane& pl) {
    if (auto it = m_planeCache.find(&pl); it != m_planeCache.end()) {
        return it->second;
    }

    const Vector3d plNormal = pl.normal.toDouble();

    double dmax = -1.0;
    int idir = -1;

    for (int i = 0; i < 3; ++i) {
        const double dc = std::abs(plNormal.get(i));
        if (dc <= dmax) {
            continue;
        }
        dmax = dc;
        idir = i;
    }

    if (idir == -1) {
        return Winding{};
    }

    Vector3d vup = Vector3d::kNull;
    switch (idir) {
        case Winding::SIDE_FRONT:
        case Winding::SIDE_BACK:
            vup = Vector3d{0.0, 0.0, 1.0};
            break;
        case Winding::SIDE_ON:
            vup = Vector3d{1.0, 0.0, 0.0};
            break;
        default:
            break;
    }

    const double vdot = vup.dot(plNormal);
    vup = vup.add(plNormal.scalar(-vdot));
    vup = vup.normalize();

    const Vector3d org = plNormal.scalar(pl.dist);
    Vector3d vrt = vup.cross(plNormal);

    vup = vup.scalar(static_cast<double>(maxLen));
    vrt = vrt.scalar(static_cast<double>(maxLen));

    std::vector<Vector3d> verts;
    verts.reserve(4);
    verts.push_back(org.sub(vrt).add(vup));
    verts.push_back(org.add(vrt).add(vup));
    verts.push_back(org.add(vrt).sub(vup));
    verts.push_back(org.sub(vrt).sub(vup));

    Winding w{std::move(verts)};
    m_planeCache.emplace(&pl, w);
    return w;
}

bool WindingFactory::isHuge(const Winding& winding) const noexcept {
    for (const Vector3d& point : winding) {
        if (isHuge(point)) {
            return true;
        }
    }
    return false;
}

bool WindingFactory::isHuge(const Vector3f& v) const noexcept {
    const float limit = static_cast<float>(maxCoord);
    return std::abs(v.x) > limit || std::abs(v.y) > limit || std::abs(v.z) > limit;
}

bool WindingFactory::isHuge(const Vector3d& v) const noexcept {
    const double limit = static_cast<double>(maxCoord);
    return std::abs(v.x) > limit || std::abs(v.y) > limit || std::abs(v.z) > limit;
}

// ---------------------------------------------------------------------------
// BspTreeStats
// ---------------------------------------------------------------------------

BspTreeStats::BspTreeStats(const BspData& bsp)
    : m_bsp(bsp) {
    reset();
}

void BspTreeStats::reset() noexcept {
    m_bmin = std::numeric_limits<int>::max();
    m_bmax = -1;
    m_fmin = std::numeric_limits<int>::max();
    m_fmax = -1;
    m_nmax = -1;
}

void BspTreeStats::walk(int nodeIndex) {
    if (nodeIndex < 0) {
        const int ileaf = -1 - nodeIndex;
        if (ileaf < 0 || static_cast<std::size_t>(ileaf) >= m_bsp.leaves.size()) {
            return;
        }
        const DLeaf& leaf = m_bsp.leaves[static_cast<std::size_t>(ileaf)];

        if (leaf.fstleafface >= 0 && leaf.numleafface > 0) {
            const std::size_t endFace = std::min(
                static_cast<std::size_t>(leaf.fstleafface) + static_cast<std::size_t>(leaf.numleafface),
                m_bsp.leafFaces.size());
            for (std::size_t i = static_cast<std::size_t>(leaf.fstleafface); i < endFace; ++i) {
                const int iface = m_bsp.leafFaces[i];
                m_fmax = std::max(m_fmax, iface);
                m_fmin = std::min(m_fmin, iface);
            }
        }

        if (leaf.fstleafbrush >= 0 && leaf.numleafbrush > 0) {
            const std::size_t endBrush = std::min(
                static_cast<std::size_t>(leaf.fstleafbrush) + static_cast<std::size_t>(leaf.numleafbrush),
                m_bsp.leafBrushes.size());
            for (std::size_t i = static_cast<std::size_t>(leaf.fstleafbrush); i < endBrush; ++i) {
                const int ibrush = m_bsp.leafBrushes[i];
                m_bmax = std::max(m_bmax, ibrush);
                m_bmin = std::min(m_bmin, ibrush);
            }
        }
    } else {
        if (static_cast<std::size_t>(nodeIndex) >= m_bsp.nodes.size()) {
            return;
        }
        m_nmax = std::max(m_nmax, nodeIndex);

        const DNode& node = m_bsp.nodes[static_cast<std::size_t>(nodeIndex)];
        walk(node.children[0]);
        walk(node.children[1]);
    }
}

// ---------------------------------------------------------------------------
// BrushBounds
// ---------------------------------------------------------------------------

BrushBounds::BrushBounds(WindingFactory& wf)
    : m_wf(wf) {}

AABB BrushBounds::getBounds(const BspData& bsp, const DBrush& brush) {
    if (auto it = m_aabbCache.find(&brush); it != m_aabbCache.end()) {
        return it->second;
    }

    AABB bounds = AABB::kZero;
    for (int i = 0; i < brush.numside; ++i) {
        bounds = bounds.include(m_wf.fromSide(bsp, brush, i).getBounds());
    }

    m_aabbCache.emplace(&brush, bounds);
    return bounds;
}

} // namespace Domain::Bsp::Internal
