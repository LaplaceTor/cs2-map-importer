#include "Domain/Bsp/Internal/BspMappers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <utility>

#include <QCoreApplication>

namespace Domain::Bsp::Internal {

namespace {

constexpr float kAreaEps = 1.0f;

struct FaceIndexKey {
    int pnum = 0;
    int texinfo = 0;
    int dispInfo = 0;

    bool operator==(const FaceIndexKey& other) const noexcept = default;

    static FaceIndexKey fromFace(const DFace& face) noexcept {
        return FaceIndexKey{face.pnum, face.texinfo, face.dispInfo};
    }

    static FaceIndexKey fromBrushSide(const DBrushSide& side) noexcept {
        return FaceIndexKey{side.pnum, side.texinfo, static_cast<int>(static_cast<std::int16_t>(side.dispinfo - 1))};
    }
};

struct FaceIndexKeyHash {
    std::size_t operator()(const FaceIndexKey& key) const noexcept {
        std::size_t h = std::hash<int>{}(key.pnum);
        h ^= std::hash<int>{}(key.texinfo) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(key.dispInfo) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

double matchingOccluderAreaPercentage(
    int occluderPolyIndex,
    int brushIndex,
    int brushSideIndex,
    const BspData& bsp,
    WindingFactory& windingFactory) {
    const DOccluderPolyData& occluderPolyData = bsp.occluderPolyDatas[static_cast<std::size_t>(occluderPolyIndex)];
    const DBrushSide& brushSide = bsp.brushSides[static_cast<std::size_t>(brushSideIndex)];
    if (occluderPolyData.planenum != brushSide.pnum) {
        return 0.0;
    }
    if (brushSide.pnum < 0 || static_cast<std::size_t>(brushSide.pnum) >= bsp.planes.size()) {
        return 0.0;
    }

    const Winding& w1 = windingFactory.fromOccluder(bsp, occluderPolyIndex);
    const Winding& w2 = windingFactory.fromSide(bsp, brushIndex, brushSideIndex);
    const Winding clipped = w1.clipWinding(w2, bsp.planes[static_cast<std::size_t>(brushSide.pnum)].normal.toDouble());
    return clipped.area() / w1.area();
}

double matchingAreaportalAreaPercentage(
    int areaportalIndex,
    int brushIndex,
    int brushSideIndex,
    const BspData& bsp,
    WindingFactory& windingFactory) {
    const DAreaportal& areaportal = bsp.areaportals[static_cast<std::size_t>(areaportalIndex)];
    const DBrushSide& brushSide = bsp.brushSides[static_cast<std::size_t>(brushSideIndex)];
    if (areaportal.planenum != brushSide.pnum) {
        return 0.0;
    }
    if (brushSide.pnum < 0 || static_cast<std::size_t>(brushSide.pnum) >= bsp.planes.size()) {
        return 0.0;
    }

    const Winding& w1 = windingFactory.fromAreaportal(bsp, areaportalIndex);
    const Winding& w2 = windingFactory.fromSide(bsp, brushIndex, brushSideIndex);
    const Winding clipped = w1.clipWinding(w2, bsp.planes[static_cast<std::size_t>(brushSide.pnum)].normal.toDouble());
    return clipped.area() / w1.area();
}

HungarianAlgorithm::Result hungarianImpl(
    const HungarianAlgorithm::WeightsAccessor& weightsAccessor,
    int jobs,
    int workers) {
    const int nj = jobs;
    const int nw = workers;

    if (nj <= 0) {
        return HungarianAlgorithm::Result{{}, 0.0};
    }

    std::vector<int> jw(static_cast<std::size_t>(nj), -1);
    std::vector<int> wj(static_cast<std::size_t>(nw), -1);

    std::vector<double> lj(static_cast<std::size_t>(nj), 0.0);
    for (int j = 0; j < nj; ++j) {
        double maxVal = weightsAccessor(j, 0);
        for (int w = 1; w < nw; ++w) {
            maxVal = std::max(maxVal, weightsAccessor(j, w));
        }
        lj[static_cast<std::size_t>(j)] = maxVal;
    }
    std::vector<double> lw(static_cast<std::size_t>(nw), 0.0);

    std::vector<std::uint8_t> s(static_cast<std::size_t>(nj), 0);
    std::vector<int> alternating(static_cast<std::size_t>(nw), -1);
    std::vector<double> slack(static_cast<std::size_t>(nw), 0.0);
    std::vector<int> slackj(static_cast<std::size_t>(nw), 0);

    for (int root = 0; root < nj; ++root) {
        std::fill(alternating.begin(), alternating.end(), -1);
        std::fill(s.begin(), s.end(), 0);
        s[static_cast<std::size_t>(root)] = 1;

        for (int w = 0; w < nw; ++w) {
            slack[static_cast<std::size_t>(w)] =
                lj[static_cast<std::size_t>(root)] + lw[static_cast<std::size_t>(w)] - weightsAccessor(root, w);
        }
        std::fill(slackj.begin(), slackj.end(), root);

        int wOut = -1;
        while (true) {
            double delta = std::numeric_limits<double>::infinity();
            int j = 0;
            int w = 0;

            for (int ww = 0; ww < nw; ++ww) {
                const std::size_t idx = static_cast<std::size_t>(ww);
                if (alternating[idx] < 0 && slack[idx] < delta) {
                    delta = slack[idx];
                    j = slackj[idx];
                    w = ww;
                }
            }

            if (!std::isfinite(delta)) {
                break;
            }

            if (delta > 0.0) {
                for (int i = 0; i < nj; ++i) {
                    if (s[static_cast<std::size_t>(i)] != 0) {
                        lj[static_cast<std::size_t>(i)] -= delta;
                    }
                }
                for (int i = 0; i < nw; ++i) {
                    const std::size_t idx = static_cast<std::size_t>(i);
                    if (alternating[idx] >= 0) {
                        lw[idx] += delta;
                    } else {
                        slack[idx] -= delta;
                    }
                }
            }

            alternating[static_cast<std::size_t>(w)] = j;
            if (wj[static_cast<std::size_t>(w)] < 0) {
                wOut = w;
                break;
            }

            j = wj[static_cast<std::size_t>(w)];
            s[static_cast<std::size_t>(j)] = 1;

            for (int i = 0; i < nw; ++i) {
                const std::size_t idx = static_cast<std::size_t>(i);
                if (alternating[idx] < 0) {
                    const double alternateSlack =
                        lj[static_cast<std::size_t>(j)] + lw[idx] - weightsAccessor(j, i);
                    if (slack[idx] > alternateSlack) {
                        slack[idx] = alternateSlack;
                        slackj[idx] = j;
                    }
                }
            }
        }

        while (wOut >= 0) {
            const int j = alternating[static_cast<std::size_t>(wOut)];
            const int prec = jw[static_cast<std::size_t>(j)];
            wj[static_cast<std::size_t>(wOut)] = j;
            jw[static_cast<std::size_t>(j)] = wOut;
            wOut = prec;
        }
    }

    const double sum = std::accumulate(lj.begin(), lj.end(), 0.0)
        + std::accumulate(lw.begin(), lw.end(), 0.0);
    return HungarianAlgorithm::Result{std::move(jw), sum};
}

} // namespace

// ---------------------------------------------------------------------------
// HungarianAlgorithm
// ---------------------------------------------------------------------------

HungarianAlgorithm::Result HungarianAlgorithm::hungarian(
    const WeightsAccessor& accessor,
    int jobs,
    int workers) {
    if (jobs <= 0) {
        return Result{{}, 0.0};
    }

    const int paddedWorkers = std::max(workers, jobs);
    Result res = hungarianImpl(
        [&accessor, workers](int j, int w) -> double {
            if (w >= workers) {
                return 0.0;
            }
            return accessor(j, w);
        },
        jobs,
        paddedWorkers);

    for (int& assigned : res.jobToWorker) {
        if (assigned >= workers) {
            assigned = -1;
        }
    }
    return res;
}

// ---------------------------------------------------------------------------
// BrushSideFaceMapper
// ---------------------------------------------------------------------------

BrushSideFaceMapper::BrushSideFaceMapper(
    BspFileReader& reader,
    WindingFactory& windingFactory,
    const BspDecompileConfig& config)
    : m_reader(reader)
    , m_windingFactory(windingFactory)
    , m_config(config) {}

void BrushSideFaceMapper::load() {
    m_reader.loadOriginalFaces();
    m_reader.loadFaces();
    m_reader.loadBrushSides();
    m_reader.loadSurfaceEdges();
    m_reader.loadEdges();
    m_reader.loadVertices();

    m_brushSideToOrigFace.assign(m_reader.data().brushSides.size(), -1);
    m_origFaceToBrushSide.clear();

    identifyExactMatches();
    identifyMergedMatches();
}

std::optional<int> BrushSideFaceMapper::origFaceIndex(int brushSideI) const {
    if (brushSideI < 0 || static_cast<std::size_t>(brushSideI) >= m_brushSideToOrigFace.size()) {
        return std::nullopt;
    }
    const int origFace = m_brushSideToOrigFace[static_cast<std::size_t>(brushSideI)];
    if (origFace < 0) {
        return std::nullopt;
    }
    return origFace;
}

std::vector<int> BrushSideFaceMapper::brushSideIndices(int origFaceI) const {
    const auto it = m_origFaceToBrushSide.constFind(origFaceI);
    if (it == m_origFaceToBrushSide.constEnd()) {
        return {};
    }
    std::vector<int> result;
    result.reserve(static_cast<std::size_t>(it.value().size()));
    for (int idx : it.value()) {
        result.push_back(idx);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void BrushSideFaceMapper::identifyExactMatches() {
    const BspData& bsp = m_reader.data();

    std::unordered_map<FaceIndexKey, std::vector<int>, FaceIndexKeyHash> origFaceIndex;
    origFaceIndex.reserve(bsp.origFaces.size());
    for (std::size_t i = 0; i < bsp.origFaces.size(); ++i) {
        origFaceIndex[FaceIndexKey::fromFace(bsp.origFaces[i])].push_back(static_cast<int>(i));
    }

    int exactMatchesFound = 0;
    for (std::size_t brushIdx = 0; brushIdx < bsp.brushes.size(); ++brushIdx) {
        const DBrush& brush = bsp.brushes[brushIdx];
        if (brush.fstside < 0 || brush.numside <= 0) {
            continue;
        }
        for (int i = 0; i < brush.numside; ++i) {
            const int brushSideIndex = brush.fstside + i;
            if (brushSideIndex < 0 || static_cast<std::size_t>(brushSideIndex) >= bsp.brushSides.size()) {
                continue;
            }
            const DBrushSide& brushSide = bsp.brushSides[static_cast<std::size_t>(brushSideIndex)];
            const Winding& brushSideWinding =
                m_windingFactory.fromSide(bsp, static_cast<int>(brushIdx), brushSideIndex);

            auto it = origFaceIndex.find(FaceIndexKey::fromBrushSide(brushSide));
            if (it == origFaceIndex.end()) {
                continue;
            }

            std::vector<int>& potentialFaces = it->second;
            for (auto faceIt = potentialFaces.begin(); faceIt != potentialFaces.end(); ++faceIt) {
                const int origFaceI = *faceIt;
                if (m_windingFactory.fromFace(bsp, origFaceI, true).matches(brushSideWinding)) {
                    if (m_brushSideToOrigFace[static_cast<std::size_t>(brushSideIndex)] < 0) {
                        ++exactMatchesFound;
                    }
                    m_brushSideToOrigFace[static_cast<std::size_t>(brushSideIndex)] = origFaceI;
                    m_origFaceToBrushSide[origFaceI].insert(brushSideIndex);
                    potentialFaces.erase(faceIt);
                    break;
                }
            }
        }
    }

    const int exactMatchesPossible = static_cast<int>(bsp.brushSides.size());
    const double exactPct = exactMatchesPossible > 0
        ? 100.0 * static_cast<double>(exactMatchesFound) / static_cast<double>(exactMatchesPossible)
        : 0.0;
    m_config.log(
        BspLogLevel::Info,
        QStringLiteral("%1 (%2%) exact brushside->origface matches")
            .arg(exactMatchesFound)
            .arg(QString::number(exactPct, 'f', 1)));

    int origFacesNonDispRemaining = 0;
    int origFacesDispRemaining = 0;
    for (const auto& [key, remaining] : origFaceIndex) {
        for (int origFaceI : remaining) {
            if (bsp.origFaces[static_cast<std::size_t>(origFaceI)].dispInfo >= 0) {
                ++origFacesDispRemaining;
            } else {
                ++origFacesNonDispRemaining;
            }
        }
    }

    int origFacesNonDispTotal = 0;
    int origFacesDispTotal = 0;
    for (const DFace& face : bsp.origFaces) {
        if (face.dispInfo >= 0) {
            ++origFacesDispTotal;
        } else {
            ++origFacesNonDispTotal;
        }
    }

    const double nonDispPct = origFacesNonDispTotal > 0
        ? 100.0 * static_cast<double>(origFacesNonDispRemaining) / static_cast<double>(origFacesNonDispTotal)
        : 0.0;
    const double dispPct = origFacesDispTotal > 0
        ? 100.0 * static_cast<double>(origFacesDispRemaining) / static_cast<double>(origFacesDispTotal)
        : 0.0;

    m_config.log(
        BspLogLevel::Info,
        QStringLiteral("%1 (%2%) nondisp and %3 (%4%) disp original faces left after exact brushside->origface matching")
            .arg(origFacesNonDispRemaining)
            .arg(QString::number(nonDispPct, 'f', 1))
            .arg(origFacesDispRemaining)
            .arg(QString::number(dispPct, 'f', 1)));
}

void BrushSideFaceMapper::identifyMergedMatches() {
    const BspData& bsp = m_reader.data();

    std::unordered_map<FaceIndexKey, std::vector<int>, FaceIndexKeyHash> faceIndex;
    faceIndex.reserve(bsp.faces.size());
    for (std::size_t i = 0; i < bsp.faces.size(); ++i) {
        faceIndex[FaceIndexKey::fromFace(bsp.faces[i])].push_back(static_cast<int>(i));
    }

    int newMatchesCount = 0;

    for (std::size_t brushIdx = 0; brushIdx < bsp.brushes.size(); ++brushIdx) {
        const DBrush& brush = bsp.brushes[brushIdx];
        if (brush.fstside < 0 || brush.numside <= 0) {
            continue;
        }
        for (int i = 0; i < brush.numside; ++i) {
            const int brushSideIndex = brush.fstside + i;
            if (brushSideIndex < 0 || static_cast<std::size_t>(brushSideIndex) >= bsp.brushSides.size()) {
                continue;
            }
            if (m_brushSideToOrigFace[static_cast<std::size_t>(brushSideIndex)] >= 0) {
                continue;
            }

            const DBrushSide& brushSide = bsp.brushSides[static_cast<std::size_t>(brushSideIndex)];
            if (brushSide.pnum < 0 || static_cast<std::size_t>(brushSide.pnum) >= bsp.planes.size()) {
                continue;
            }

            const auto it = faceIndex.find(FaceIndexKey::fromBrushSide(brushSide));
            if (it == faceIndex.end()) {
                continue;
            }

            const Winding& brushSideWinding =
                m_windingFactory.fromSide(bsp, static_cast<int>(brushIdx), brushSideIndex);
            const Vector3d normal = bsp.planes[static_cast<std::size_t>(brushSide.pnum)].normal.toDouble();

            std::unordered_map<int, double> areaByOrigFace;
            for (int faceI : it->second) {
                const DFace& dFace = bsp.faces[static_cast<std::size_t>(faceI)];
                if (dFace.origFace < 0) {
                    continue;
                }
                const double area = m_windingFactory.fromFace(bsp, faceI, false)
                                        .clipWinding(brushSideWinding, normal)
                                        .area();
                areaByOrigFace[dFace.origFace] += area;
            }

            int bestOrigFace = -1;
            double bestArea = static_cast<double>(kAreaEps);
            for (const auto& [origFaceI, totalArea] : areaByOrigFace) {
                if (totalArea > bestArea) {
                    bestArea = totalArea;
                    bestOrigFace = origFaceI;
                }
            }

            if (bestOrigFace >= 0) {
                m_brushSideToOrigFace[static_cast<std::size_t>(brushSideIndex)] = bestOrigFace;
                m_origFaceToBrushSide[bestOrigFace].insert(brushSideIndex);
                ++newMatchesCount;
            }
        }
    }

    const double partialPct = !bsp.brushSides.empty()
        ? 100.0 * static_cast<double>(newMatchesCount) / static_cast<double>(bsp.brushSides.size())
        : 0.0;
    m_config.log(
        BspLogLevel::Info,
        QStringLiteral("%1 (%2%) partial brushside->origface matches")
            .arg(newMatchesCount)
            .arg(QString::number(partialPct, 'f', 1)));
}

// ---------------------------------------------------------------------------
// AreaportalMapper
// ---------------------------------------------------------------------------

namespace {

bool checkAreaportalsInvalid(const BspData& bsp) {
    for (const DAreaportal& ap : bsp.areaportals) {
        if (ap.portalKey != 0 && ap.clipPortalVerts == 0) {
            return true;
        }
    }
    return false;
}

std::vector<int> getApBrushes(const BspData& bsp) {
    std::vector<int> result;
    for (std::size_t i = 0; i < bsp.brushes.size(); ++i) {
        if (bsp.brushes[i].isAreaportal()) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

std::vector<int> getPortalKeys(const BspData& bsp) {
    std::vector<int> result;
    QSet<int> seen;
    for (const DAreaportal& ap : bsp.areaportals) {
        if (ap.portalKey > 0 && !seen.contains(ap.portalKey)) {
            seen.insert(ap.portalKey);
            result.push_back(ap.portalKey);
        }
    }
    return result;
}

AreaportalMapper::ReallocationData orderedApMapping(
    std::vector<int> portalKeys,
    const std::vector<int>& areaportalIBrushes) {
    std::sort(portalKeys.begin(), portalKeys.end());
    const std::size_t maxMappingCount = std::min(portalKeys.size(), areaportalIBrushes.size());

    AreaportalMapper::ReallocationData data;
    for (std::size_t i = 0; i < maxMappingCount; ++i) {
        data.mapping.insert(portalKeys[i], areaportalIBrushes[i]);
    }
    return data;
}

AreaportalMapper::ReallocationData manualApMapping(
    const BspData& bsp,
    const BspDecompileConfig& config,
    const std::vector<int>& areaportalIBrushes,
    WindingFactory& windingFactory) {
    using PortalGroup = std::pair<int, std::vector<int>>;

    std::unordered_map<int, std::vector<int>> grouped;
    for (std::size_t i = 0; i < bsp.areaportals.size(); ++i) {
        const int key = bsp.areaportals[i].portalKey;
        if (key != 0) {
            grouped[key].push_back(static_cast<int>(i));
        }
    }

    std::vector<PortalGroup> areaportalsByPortalKey;
    areaportalsByPortalKey.reserve(grouped.size());
    for (auto& [key, indices] : grouped) {
        areaportalsByPortalKey.emplace_back(key, std::move(indices));
    }
    std::sort(
        areaportalsByPortalKey.begin(),
        areaportalsByPortalKey.end(),
        [](const PortalGroup& a, const PortalGroup& b) { return a.first < b.first; });

    const std::size_t numPortals = areaportalsByPortalKey.size();
    const std::size_t numBrushes = areaportalIBrushes.size();

    std::vector<std::vector<double>> scores(numPortals, std::vector<double>(numBrushes, 0.0));
    for (std::size_t portal = 0; portal < numPortals; ++portal) {
        for (std::size_t apBrush = 0; apBrush < numBrushes; ++apBrush) {
            const int areaportalIBrush = areaportalIBrushes[apBrush];
            const DBrush& brush = bsp.brushes[static_cast<std::size_t>(areaportalIBrush)];

            double bestScore = 0.0;
            for (int iAreaportal : areaportalsByPortalKey[portal].second) {
                if (brush.fstside < 0 || brush.numside <= 0) {
                    continue;
                }
                const std::size_t endSide = std::min(
                    static_cast<std::size_t>(brush.fstside) + static_cast<std::size_t>(brush.numside),
                    bsp.brushSides.size());
                for (std::size_t s = static_cast<std::size_t>(brush.fstside); s < endSide; ++s) {
                    const double newScore = matchingAreaportalAreaPercentage(
                        iAreaportal,
                        areaportalIBrush,
                        static_cast<int>(s),
                        bsp,
                        windingFactory);
                    if (std::isfinite(newScore)) {
                        bestScore = std::max(bestScore, newScore);
                    }
                }
            }

            if (!std::isfinite(bestScore)) {
                bestScore = 0.0;
            }
            scores[portal][apBrush] = bestScore;
        }
    }

    const HungarianAlgorithm::Result mappingResult = HungarianAlgorithm::hungarian(
        [&scores](int j, int w) { return scores[static_cast<std::size_t>(j)][static_cast<std::size_t>(w)]; },
        static_cast<int>(numPortals),
        static_cast<int>(numBrushes));

    std::vector<int> assignments = mappingResult.jobToWorker;
    std::vector<std::vector<int>> groups;

    for (std::size_t portal = 0; portal < assignments.size(); ++portal) {
        const int assignedBrush = assignments[portal];
        if (assignedBrush <= 0 || scores[portal][static_cast<std::size_t>(assignedBrush)] == 0.0) {
            continue;
        }

        bool added = false;
        for (std::vector<int>& group : groups) {
            const bool canAddToGroup = std::all_of(group.begin(), group.end(), [&](int p) {
                const std::size_t pIdx = static_cast<std::size_t>(p);
                const std::size_t pAssign = static_cast<std::size_t>(assignments[pIdx]);
                const std::size_t curAssign = static_cast<std::size_t>(assignedBrush);
                return scores[pIdx][pAssign] == scores[portal][pAssign]
                    && scores[pIdx][curAssign] == scores[portal][curAssign];
            });
            if (canAddToGroup) {
                group.push_back(static_cast<int>(portal));
                added = true;
                break;
            }
        }
        if (!added) {
            groups.push_back({static_cast<int>(portal)});
        }
    }

    for (const std::vector<int>& group : groups) {
        if (group.size() <= 1) {
            continue;
        }

        std::vector<int> sortedPortals = group;
        std::sort(sortedPortals.begin(), sortedPortals.end(), [&](int a, int b) {
            return areaportalsByPortalKey[static_cast<std::size_t>(a)].first
                < areaportalsByPortalKey[static_cast<std::size_t>(b)].first;
        });

        std::vector<int> sortedApBrushes;
        sortedApBrushes.reserve(sortedPortals.size());
        for (int job : sortedPortals) {
            sortedApBrushes.push_back(assignments[static_cast<std::size_t>(job)]);
        }
        std::sort(sortedApBrushes.begin(), sortedApBrushes.end(), [&](int a, int b) {
            return areaportalIBrushes[static_cast<std::size_t>(a)]
                < areaportalIBrushes[static_cast<std::size_t>(b)];
        });

        for (std::size_t i = 0; i < sortedPortals.size(); ++i) {
            assignments[static_cast<std::size_t>(sortedPortals[i])] = sortedApBrushes[i];
        }
    }

    AreaportalMapper::ReallocationData data;
    for (std::size_t portal = 0; portal < assignments.size(); ++portal) {
        const int portalKey = areaportalsByPortalKey[portal].first;
        const int apBrush = assignments[portal];
        if (apBrush < 0 || scores[portal][static_cast<std::size_t>(apBrush)] == 0.0) {
            config.log(
                BspLogLevel::Warning,
                QCoreApplication::translate(
                    "Domain::Bsp::Internal::AreaportalMapper",
                    "Could not reallocate brush for portalKey %1.")
                    .arg(portalKey));
            continue;
        }
        data.mapping.insert(portalKey, areaportalIBrushes[static_cast<std::size_t>(apBrush)]);
    }

    return data;
}

} // namespace

AreaportalMapper::ReallocationData AreaportalMapper::createReallocationData(
    const BspData& bsp,
    const BspDecompileConfig& config,
    WindingFactory& windingFactory) {
    if (!config.writeAreaportals) {
        return ReallocationData{};
    }

    if (checkAreaportalsInvalid(bsp)) {
        config.log(
            BspLogLevel::Warning,
            QCoreApplication::translate(
                "Domain::Bsp::Internal::AreaportalMapper",
                "Invalid areaportals, map was probably compiled with errors! Errors should be expected"));
    }

    const std::vector<int> areaportalIBrushes = getApBrushes(bsp);
    const std::vector<int> portalKeys = getPortalKeys(bsp);

    if (portalKeys.empty()) {
        config.log(BspLogLevel::Info, QStringLiteral("No areaportals to reallocate..."));
        return ReallocationData{};
    }

    bool useManual = false;
    if (config.apForceManualMapping) {
        config.log(BspLogLevel::Info, QStringLiteral("Forced manual areaportal mapping method"));
        useManual = true;
    } else if (portalKeys.size() == areaportalIBrushes.size()) {
        config.log(
            BspLogLevel::Info,
            QStringLiteral("Equal amount of areaportal entities and areaportal brushes. Using 'Ordered' method"));
        useManual = false;
    } else {
        config.log(
            BspLogLevel::Info,
            QStringLiteral("Unequal amount of areaportal entities and areaportal brushes. Falling back to 'Manual' method"));
        useManual = true;
    }

    if (useManual) {
        return manualApMapping(bsp, config, areaportalIBrushes, windingFactory);
    }
    return orderedApMapping(portalKeys, areaportalIBrushes);
}

// ---------------------------------------------------------------------------
// OccluderMapper
// ---------------------------------------------------------------------------

bool OccluderMapper::ReallocationData::isOccluderBrush(int iBrush) const {
    return brushToSideMapping.contains(iBrush);
}

bool OccluderMapper::ReallocationData::isOccluderBrushSide(int iBrush, int side) const {
    const auto it = brushToSideMapping.constFind(iBrush);
    if (it == brushToSideMapping.constEnd()) {
        return false;
    }
    return it.value().contains(side);
}

namespace {

struct OccluderPoly {
    int iOccluder = 0;
    int side = 0;
};

int getFirstNonWorldBrushId(const BspData& bsp) {
    BspTreeStats tree{bsp};
    tree.walk(0);
    return tree.maxBrushLeaf() + 1;
}

} // namespace

OccluderMapper::ReallocationData OccluderMapper::createReallocationData(
    const BspData& bsp,
    const BspDecompileConfig& config,
    WindingFactory& windingFactory) {
    if (!config.writeOccluders) {
        return ReallocationData{};
    }

    if (bsp.occluderDatas.empty()) {
        config.log(BspLogLevel::Info, QStringLiteral("No occluders to reallocate..."));
        return ReallocationData{};
    }

    config.log(BspLogLevel::Info, QStringLiteral("Reallocating occluders..."));

    const int firstNonWorldIBrush = std::min(
        std::max(getFirstNonWorldBrushId(bsp), 0),
        static_cast<int>(bsp.brushes.size()));

    std::vector<int> occluderPolyIndices;
    occluderPolyIndices.reserve(bsp.occluderDatas.size());
    int polySum = 0;
    for (const DOccluderData& occluder : bsp.occluderDatas) {
        polySum += std::max(occluder.polycount, 0);
        occluderPolyIndices.push_back(polySum);
    }

    std::vector<int> brushSideIndices;
    brushSideIndices.reserve(bsp.brushes.size() - static_cast<std::size_t>(firstNonWorldIBrush));
    int sideSum = 0;
    for (std::size_t i = static_cast<std::size_t>(firstNonWorldIBrush); i < bsp.brushes.size(); ++i) {
        sideSum += std::max(bsp.brushes[i].numside, 0);
        brushSideIndices.push_back(sideSum);
    }

    const int occluderSidesCount = !occluderPolyIndices.empty() ? occluderPolyIndices.back() : 0;
    const int brushSideCount = !brushSideIndices.empty() ? brushSideIndices.back() : 0;

    std::vector<std::vector<double>> scores(
        static_cast<std::size_t>(occluderSidesCount),
        std::vector<double>(static_cast<std::size_t>(brushSideCount), 0.0));

    for (std::size_t iOccluder = 0; iOccluder < bsp.occluderDatas.size(); ++iOccluder) {
        const DOccluderData& occluder = bsp.occluderDatas[iOccluder];
        for (int sideOfOccluder = 0; sideOfOccluder < occluder.polycount; ++sideOfOccluder) {
            const int polyIdx = occluder.firstpoly + sideOfOccluder;
            if (polyIdx < 0 || static_cast<std::size_t>(polyIdx) >= bsp.occluderPolyDatas.size()) {
                continue;
            }
            const int flattenedOccluderPoly =
                (iOccluder > 0 ? occluderPolyIndices[iOccluder - 1] : 0) + sideOfOccluder;

            for (std::size_t iBrush = static_cast<std::size_t>(firstNonWorldIBrush); iBrush < bsp.brushes.size(); ++iBrush) {
                const DBrush& brush = bsp.brushes[iBrush];
                if (brush.fstside < 0 || brush.numside <= 0) {
                    continue;
                }
                const std::size_t brushIndex = iBrush - static_cast<std::size_t>(firstNonWorldIBrush);
                const int baseBrushSide = brushIndex > 0 ? brushSideIndices[brushIndex - 1] : 0;

                for (int sideOfBrush = 0; sideOfBrush < brush.numside; ++sideOfBrush) {
                    const int bsIdx = brush.fstside + sideOfBrush;
                    if (bsIdx < 0 || static_cast<std::size_t>(bsIdx) >= bsp.brushSides.size()) {
                        continue;
                    }
                    const int flattenedBrushSide = baseBrushSide + sideOfBrush;

                    double score = matchingOccluderAreaPercentage(
                        polyIdx,
                        static_cast<int>(iBrush),
                        bsIdx,
                        bsp,
                        windingFactory);
                    if (!std::isfinite(score)) {
                        score = 0.0;
                    }
                    scores[static_cast<std::size_t>(flattenedOccluderPoly)][static_cast<std::size_t>(flattenedBrushSide)] = score;
                }
            }
        }
    }

    const HungarianAlgorithm::Result mappingResult = HungarianAlgorithm::hungarian(
        [&scores](int j, int w) { return scores[static_cast<std::size_t>(j)][static_cast<std::size_t>(w)]; },
        occluderSidesCount,
        brushSideCount);

    std::unordered_map<int, std::unordered_map<int, OccluderPoly>> collectedBrushes;
    for (std::size_t iOccluder = 0; iOccluder < bsp.occluderDatas.size(); ++iOccluder) {
        const DOccluderData& occluder = bsp.occluderDatas[iOccluder];
        const int offset = iOccluder > 0 ? occluderPolyIndices[iOccluder - 1] : 0;

        for (int sideOfOccluder = 0; sideOfOccluder < occluder.polycount; ++sideOfOccluder) {
            const int index = offset + sideOfOccluder;
            const int flattenedBrushSide = mappingResult.jobToWorker[static_cast<std::size_t>(index)];
            if (flattenedBrushSide < 0
                || scores[static_cast<std::size_t>(index)][static_cast<std::size_t>(flattenedBrushSide)] == 0.0) {
                config.log(
                    BspLogLevel::Warning,
                    QCoreApplication::translate(
                        "Domain::Bsp::Internal::OccluderMapper",
                        "Couldn't reallocate side %1 of occluder %2.")
                        .arg(sideOfOccluder)
                        .arg(static_cast<int>(iOccluder)));
                continue;
            }

            const auto ub = std::upper_bound(brushSideIndices.begin(), brushSideIndices.end(), flattenedBrushSide);
            const int apBrush = static_cast<int>(std::distance(brushSideIndices.begin(), ub));
            const int sideOfBrush = flattenedBrushSide - (apBrush > 0 ? brushSideIndices[static_cast<std::size_t>(apBrush - 1)] : 0);
            const int iBrush = firstNonWorldIBrush + apBrush;

            collectedBrushes[iBrush][sideOfBrush] = OccluderPoly{static_cast<int>(iOccluder), sideOfOccluder};
        }
    }

    ReallocationData data;
    for (const auto& [iBrush, sideMapping] : collectedBrushes) {
        const DBrush& dBrush = bsp.brushes[static_cast<std::size_t>(iBrush)];

        std::unordered_map<int, int> counts;
        for (const auto& [sideOfBrush, poly] : sideMapping) {
            counts[poly.iOccluder]++;
        }

        std::optional<int> mostCommonIOccluder;
        int maxCount = 0;
        for (const auto& [occIdx, cnt] : counts) {
            if (cnt > maxCount) {
                maxCount = cnt;
                mostCommonIOccluder = occIdx;
            }
        }

        if (!mostCommonIOccluder.has_value()) {
            continue;
        }

        QHash<int, int> newMapping;
        for (int sideOfBrush = 0; sideOfBrush < dBrush.numside; ++sideOfBrush) {
            const auto it = sideMapping.find(sideOfBrush);
            if (it == sideMapping.end()) {
                continue;
            }
            const OccluderPoly& occluderPoly = it->second;
            if (occluderPoly.iOccluder != *mostCommonIOccluder) {
                config.log(
                    BspLogLevel::Warning,
                    QCoreApplication::translate(
                        "Domain::Bsp::Internal::OccluderMapper",
                        "Couldn't reallocate side %1 of occluder %2, because reallocated brush belongs to different occluder %3")
                        .arg(occluderPoly.side)
                        .arg(occluderPoly.iOccluder)
                        .arg(*mostCommonIOccluder));
                continue;
            }
            newMapping.insert(sideOfBrush, occluderPoly.side);
        }

        data.occluderToBrushes[*mostCommonIOccluder].insert(iBrush);
        data.brushToSideMapping.insert(iBrush, std::move(newMapping));
    }

    return data;
}

} // namespace Domain::Bsp::Internal
