#include "Domain/Bsp/Internal/Winding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QStringList>

#include "Domain/Bsp/Internal/SourceAppDb.h"

namespace Domain::Bsp::Internal {

namespace {

const Winding& emptyWinding() noexcept {
    static const Winding kEmpty;
    return kEmpty;
}

} // namespace

// ---------------------------------------------------------------------------
// Winding
// ---------------------------------------------------------------------------

Winding::Winding(QVarLengthArray<Vector3d, 16> verts)
    : m_verts(std::move(verts)) {}

Winding::Winding(std::span<const Vector3d> verts)
    : m_verts(verts.begin(), verts.end()) {}

Winding Winding::clipEpsilon(const Vector3d& normal, double dist, double eps, bool back) const {
    const qsizetype count = m_verts.size();
    if (count == 0) {
        return Winding{};
    }

    int counts[3] = {0, 0, 0};
    QVarLengthArray<double, 17> dists(count + 1, 0.0);
    QVarLengthArray<int, 17> sides(count + 1, 0);

    for (qsizetype i = 0; i < count; ++i) {
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

    sides[count] = sides[0];
    dists[count] = dists[0];

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

    QVarLengthArray<Vector3d, 16> vertsNew;
    vertsNew.reserve(count + 4);

    for (qsizetype i = 0; i < count; ++i) {
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

        const Vector3d& p2 = (i == count - 1) ? m_verts[0] : m_verts[i + 1];
        const double dot = dists[i] / (dists[i] - dists[i + 1]);

        Vector3d mv = Vector3d::kNull;
        for (int j = 0; j < 3; ++j) {
            if (normal[j] == 1.0) {
                mv = mv.with(j, dist);
            } else if (normal[j] == -1.0) {
                mv = mv.with(j, -dist);
            } else {
                mv = mv.with(j, p1[j] + dot * (p2[j] - p1[j]));
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
        const Vector3d edge = other[(i + 1) % otherSize] - other[i];
        const Vector3d normal = edge.cross(projNormal).normalize();
        const double dist = normal.dot(other[i]);

        result = result.clipEpsilon(normal, dist, EPS_SPLIT, false);
    }
    return result;
}

Winding& Winding::removeDegeneratedInPlace() {
    if (m_verts.isEmpty()) {
        return *this;
    }

    const qsizetype count = m_verts.size();
    QVarLengthArray<Vector3d, 16> vertsNew;
    vertsNew.reserve(count);

    for (qsizetype i = 0; i < count; ++i) {
        const qsizetype j = (i + 1) % count;
        if ((m_verts[i] - m_verts[j]).length() > EPS_DEGEN) {
            vertsNew.push_back(m_verts[i]);
        }
    }

    m_verts = std::move(vertsNew);
    return *this;
}

Winding& Winding::removeCollinearInPlace() {
    if (m_verts.isEmpty()) {
        return *this;
    }

    const qsizetype count = m_verts.size();
    QVarLengthArray<Vector3d, 16> vertsNew;
    vertsNew.reserve(count);

    for (qsizetype i = 0; i < count; ++i) {
        const qsizetype j = (i + 1) % count;
        const qsizetype k = (i + count - 1) % count;
        const Vector3d v1 = (m_verts[j] - m_verts[i]).normalize();
        const Vector3d v2 = (m_verts[i] - m_verts[k]).normalize();

        if (v1.dot(v2) < 0.999) {
            vertsNew.push_back(m_verts[i]);
        }
    }

    m_verts = std::move(vertsNew);
    return *this;
}

Winding& Winding::rotateInPlace(const Vector3d& angles) {
    if (m_verts.isEmpty() || (angles.x == 0.0 && angles.y == 0.0 && angles.z == 0.0)) {
        return *this;
    }

    for (Vector3d& vert : m_verts) {
        vert = vert.rotate(angles);
    }

    return *this;
}

Winding& Winding::translateInPlace(const Vector3d& offset) {
    if (m_verts.isEmpty()) {
        return *this;
    }

    for (Vector3d& vert : m_verts) {
        vert += offset;
    }

    return *this;
}

Winding Winding::removeDegenerated() const {
    Winding copy = *this;
    copy.removeDegeneratedInPlace();
    return copy;
}

Winding Winding::removeCollinear() const {
    Winding copy = *this;
    copy.removeCollinearInPlace();
    return copy;
}

Winding Winding::rotate(const Vector3d& angles) const {
    Winding copy = *this;
    copy.rotateInPlace(angles);
    return copy;
}

Winding Winding::translate(const Vector3d& offset) const {
    Winding copy = *this;
    copy.translateInPlace(offset);
    return copy;
}

Winding Winding::addBackface() const {
    if (m_verts.isEmpty()) {
        return *this;
    }

    const qsizetype count = m_verts.size();
    QVarLengthArray<Vector3d, 16> vertsNew;
    vertsNew.reserve(count * 2);

    for (qsizetype i = 0; i < count; ++i) {
        if (i != 0) {
            vertsNew.push_back(m_verts[i]);
        }
        vertsNew.push_back(m_verts[i]);
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
            minDist = std::min(minDist, (v1 - v2).length());
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

    Vector3d toPt = pt - m_verts[0];
    Vector3d edge = m_verts[1] - m_verts[0];
    const Vector3d testCross = edge.cross(toPt).normalize();

    const qsizetype count = m_verts.size();
    for (qsizetype i = 1; i < count; ++i) {
        toPt = pt - m_verts[i];
        edge = m_verts[(i + 1) % count] - m_verts[i];
        const Vector3d cross = edge.cross(toPt).normalize();

        if (cross.dot(testCross) < 0.0) {
            return false;
        }
    }

    return true;
}

AABB Winding::bounds() const {
    Vector3d mins = Vector3d::kMaxValue;
    Vector3d maxs = Vector3d::kMinValue;

    for (const Vector3d& vert : m_verts) {
        mins = mins.min(vert);
        maxs = maxs.max(vert);
    }

    return AABB{mins, maxs};
}

Vector3d Winding::center() const {
    if (m_verts.isEmpty()) {
        return Vector3d::kNull;
    }

    Vector3d sum = Vector3d::kNull;
    for (const Vector3d& vert : m_verts) {
        sum += vert;
    }

    return sum * static_cast<double>(1.0f / static_cast<float>(m_verts.size()));
}

std::array<Vector3d, 3> Winding::buildPlane() const {
    if (m_verts.size() < 3) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const Vector3d invalid{nan, nan, nan};
        return {invalid, invalid, invalid};
    }

    const qsizetype count = m_verts.size();
    std::array<Vector3d, 3> plane{};
    plane[0] = m_verts[0];

    QVarLengthArray<Vector3d, 16> vertsNew;
    vertsNew.reserve(count);
    for (qsizetype i = 0; i < count; ++i) {
        vertsNew.push_back(m_verts[i] - plane[0]);
    }

    double maxmcp = -1.0;
    qsizetype imax = -1;
    qsizetype jmax = -1;

    for (qsizetype i = 1; i < count; ++i) {
        for (qsizetype j = i + 1; j < count; ++j) {
            const double mcp = vertsNew[i].cross(vertsNew[j]).length();
            if (mcp > maxmcp) {
                maxmcp = mcp;
                imax = i;
                jmax = j;
            }
        }
    }

    if (imax < 0 || jmax < 0) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const Vector3d invalid{nan, nan, nan};
        return {invalid, invalid, invalid};
    }

    plane[1] = m_verts[imax];
    plane[2] = m_verts[jmax];

    return plane;
}

bool Winding::hasDuplicates() const {
    const qsizetype count = m_verts.size();
    for (qsizetype i = 0; i < count; ++i) {
        for (qsizetype j = 0; j < count; ++j) {
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

double Winding::area() const {
    double total = 0.0;
    const qsizetype count = m_verts.size();
    for (qsizetype i = 2; i < count; ++i) {
        const Vector3d v1 = m_verts[i - 1] - m_verts[0];
        const Vector3d v2 = m_verts[i] - m_verts[0];
        total += v1.cross(v2).length();
    }
    return total * 0.5f;
}

QString Winding::toString() const {
    QStringList parts;
    parts.reserve(m_verts.size());
    for (const Vector3d& v : m_verts) {
        parts.append(QStringLiteral("(%1, %2, %3)")
                         .arg(QString::number(v.x), QString::number(v.y), QString::number(v.z)));
    }
    return QStringLiteral("[") + parts.join(QStringLiteral(", ")) + QStringLiteral("]");
}

// ---------------------------------------------------------------------------
// WindingFactory
// ---------------------------------------------------------------------------

WindingFactory::WindingFactory(int coordSize)
    : maxLen(static_cast<int>(std::ceil(std::sqrt(3.0) * coordSize)))
    , maxCoord(coordSize) {}

WindingFactory WindingFactory::forAppId(int appId) {
    const int coordSize = (appId == SourceAppId::kStrataSource) ? 131072 : 32768;
    return WindingFactory{coordSize};
}

const Winding& WindingFactory::fromFace(const BspData& bsp, int faceIndex, bool isOrigFace) {
    const auto& faceList = isOrigFace ? bsp.origFaces : bsp.faces;
    if (faceIndex < 0 || static_cast<std::size_t>(faceIndex) >= faceList.size()) {
        return emptyWinding();
    }

    auto& cache = isOrigFace ? m_origFaceCache : m_faceCache;
    if (cache.size() < faceList.size()) {
        cache.resize(faceList.size());
    }

    const auto idx = static_cast<std::size_t>(faceIndex);
    if (cache[idx].has_value()) {
        return *cache[idx];
    }

    const DFace& face = faceList[idx];
    if (face.numedge <= 0 || face.fstedge < 0
        || static_cast<std::size_t>(face.fstedge) + static_cast<std::size_t>(face.numedge) > bsp.surfEdges.size()) {
        return emptyWinding();
    }

    QVarLengthArray<Vector3d, 16> verts;
    verts.reserve(face.numedge);

    for (int i = 0; i < face.numedge; ++i) {
        const int sedge = bsp.surfEdges[static_cast<std::size_t>(face.fstedge + i)];
        const int edgeIndex = (sedge < 0) ? -sedge : sedge;
        if (edgeIndex < 0 || static_cast<std::size_t>(edgeIndex) >= bsp.edges.size()) {
            return emptyWinding();
        }

        const int v = (sedge < 0)
            ? bsp.edges[static_cast<std::size_t>(edgeIndex)].v[1]
            : bsp.edges[static_cast<std::size_t>(edgeIndex)].v[0];
        if (v < 0 || static_cast<std::size_t>(v) >= bsp.verts.size()) {
            return emptyWinding();
        }

        verts.push_back(bsp.verts[static_cast<std::size_t>(v)].point.toDouble());
    }

    cache[idx].emplace(std::move(verts));
    return *cache[idx];
}

const Winding& WindingFactory::fromSide(const BspData& bsp, int brushIndex, int brushSideIndex) {
    if (brushSideIndex < 0 || static_cast<std::size_t>(brushSideIndex) >= bsp.brushSides.size()) {
        return emptyWinding();
    }
    if (m_brushSideCache.size() < bsp.brushSides.size()) {
        m_brushSideCache.resize(bsp.brushSides.size());
    }

    const auto bsIdx = static_cast<std::size_t>(brushSideIndex);
    if (m_brushSideCache[bsIdx].has_value()) {
        return *m_brushSideCache[bsIdx];
    }

    if (brushIndex < 0 || static_cast<std::size_t>(brushIndex) >= bsp.brushes.size()) {
        return emptyWinding();
    }
    const DBrush& brush = bsp.brushes[static_cast<std::size_t>(brushIndex)];
    if (brush.fstside < 0 || brush.numside < 0
        || static_cast<std::size_t>(brush.fstside) + static_cast<std::size_t>(brush.numside) > bsp.brushSides.size()) {
        return emptyWinding();
    }
    if (brushSideIndex < brush.fstside || brushSideIndex >= brush.fstside + brush.numside) {
        return emptyWinding();
    }

    const DBrushSide& bside = bsp.brushSides[bsIdx];
    if (bside.pnum < 0 || static_cast<std::size_t>(bside.pnum) >= bsp.planes.size()) {
        return emptyWinding();
    }

    Winding w = fromPlane(bsp, bside.pnum);

    for (int i = 0; i < brush.numside; ++i) {
        const int otherIdx = brush.fstside + i;
        if (otherIdx == brushSideIndex) {
            continue;
        }

        const DBrushSide& bside2 = bsp.brushSides[static_cast<std::size_t>(otherIdx)];
        if (bside2.bevel) {
            continue;
        }
        if (bside2.pnum < 0 || static_cast<std::size_t>(bside2.pnum) >= bsp.planes.size()) {
            continue;
        }

        w = w.clipPlane(bsp.planes[static_cast<std::size_t>(bside2.pnum)], true);
    }

    m_brushSideCache[bsIdx] = std::move(w);
    return *m_brushSideCache[bsIdx];
}

const Winding& WindingFactory::fromAreaportal(const BspData& bsp, int areaportalIndex) {
    if (areaportalIndex < 0 || static_cast<std::size_t>(areaportalIndex) >= bsp.areaportals.size()) {
        return emptyWinding();
    }
    if (m_areaportalCache.size() < bsp.areaportals.size()) {
        m_areaportalCache.resize(bsp.areaportals.size());
    }

    const auto idx = static_cast<std::size_t>(areaportalIndex);
    if (m_areaportalCache[idx].has_value()) {
        return *m_areaportalCache[idx];
    }

    const DAreaportal& ap = bsp.areaportals[idx];
    if (ap.clipPortalVerts <= 0 || ap.firstClipPortalVert < 0
        || static_cast<std::size_t>(ap.firstClipPortalVert) + static_cast<std::size_t>(ap.clipPortalVerts)
               > bsp.clipPortalVerts.size()) {
        return emptyWinding();
    }

    QVarLengthArray<Vector3d, 16> verts;
    verts.reserve(ap.clipPortalVerts);
    for (int i = 0; i < ap.clipPortalVerts; ++i) {
        verts.push_back(bsp.clipPortalVerts[static_cast<std::size_t>(ap.firstClipPortalVert + i)].point.toDouble());
    }

    m_areaportalCache[idx].emplace(std::move(verts));
    return *m_areaportalCache[idx];
}

const Winding& WindingFactory::fromOccluder(const BspData& bsp, int occluderPolyIndex) {
    if (occluderPolyIndex < 0 || static_cast<std::size_t>(occluderPolyIndex) >= bsp.occluderPolyDatas.size()) {
        return emptyWinding();
    }
    if (m_occluderCache.size() < bsp.occluderPolyDatas.size()) {
        m_occluderCache.resize(bsp.occluderPolyDatas.size());
    }

    const auto idx = static_cast<std::size_t>(occluderPolyIndex);
    if (m_occluderCache[idx].has_value()) {
        return *m_occluderCache[idx];
    }

    const DOccluderPolyData& opd = bsp.occluderPolyDatas[idx];
    if (opd.vertexcount <= 0 || opd.firstvertexindex < 0
        || static_cast<std::size_t>(opd.firstvertexindex) + static_cast<std::size_t>(opd.vertexcount)
               > bsp.occluderVerts.size()) {
        return emptyWinding();
    }

    QVarLengthArray<Vector3d, 16> verts;
    verts.reserve(opd.vertexcount);
    for (int k = 0; k < opd.vertexcount; ++k) {
        const int pvi = bsp.occluderVerts[static_cast<std::size_t>(opd.firstvertexindex + k)];
        if (pvi < 0 || static_cast<std::size_t>(pvi) >= bsp.verts.size()) {
            return emptyWinding();
        }
        verts.push_back(bsp.verts[static_cast<std::size_t>(pvi)].point.toDouble());
    }

    m_occluderCache[idx].emplace(std::move(verts));
    return *m_occluderCache[idx];
}

const Winding& WindingFactory::fromPlane(const BspData& bsp, int planeIndex) {
    if (planeIndex < 0 || static_cast<std::size_t>(planeIndex) >= bsp.planes.size()) {
        return emptyWinding();
    }
    if (m_planeCache.size() < bsp.planes.size()) {
        m_planeCache.resize(bsp.planes.size());
    }

    const auto idx = static_cast<std::size_t>(planeIndex);
    if (m_planeCache[idx].has_value()) {
        return *m_planeCache[idx];
    }

    Winding w = fromPlane(bsp.planes[idx]);
    if (w.isEmpty()) {
        return emptyWinding();
    }

    m_planeCache[idx] = std::move(w);
    return *m_planeCache[idx];
}

Winding WindingFactory::fromPlane(const DPlane& pl) const {
    const Vector3d plNormal = pl.normal.toDouble();

    double dmax = -1.0;
    int idir = -1;

    for (int i = 0; i < 3; ++i) {
        const double dc = std::abs(plNormal[i]);
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
    vup = (vup + plNormal * (-vdot)).normalize();

    const Vector3d org = plNormal * pl.dist;
    Vector3d vrt = vup.cross(plNormal);

    vup *= static_cast<double>(maxLen);
    vrt *= static_cast<double>(maxLen);

    QVarLengthArray<Vector3d, 16> verts;
    verts.reserve(4);
    verts.push_back(org - vrt + vup);
    verts.push_back(org + vrt + vup);
    verts.push_back(org + vrt - vup);
    verts.push_back(org - vrt - vup);

    return Winding{std::move(verts)};
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

AABB BrushBounds::bounds(const BspData& bsp, int brushIndex) {
    if (brushIndex < 0 || static_cast<std::size_t>(brushIndex) >= bsp.brushes.size()) {
        return AABB::kZero;
    }
    if (m_aabbCache.size() < bsp.brushes.size()) {
        m_aabbCache.resize(bsp.brushes.size());
    }

    const auto idx = static_cast<std::size_t>(brushIndex);
    if (m_aabbCache[idx].has_value()) {
        return *m_aabbCache[idx];
    }

    const DBrush& brush = bsp.brushes[idx];
    AABB result = AABB::kZero;
    for (int i = 0; i < brush.numside; ++i) {
        const int brushSideIndex = brush.fstside + i;
        result = result.include(m_wf.fromSide(bsp, brushIndex, brushSideIndex).bounds());
    }

    m_aabbCache[idx] = result;
    return result;
}

} // namespace Domain::Bsp::Internal
