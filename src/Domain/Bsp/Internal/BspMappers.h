#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QHash>
#include <QSet>

#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspFileReader.h"
#include "Domain/Bsp/Internal/BspStructs.h"
#include "Domain/Bsp/Internal/Winding.h"

namespace Domain::Bsp::Internal {

/**
 * @brief Maximum weight bipartite matching via the Kuhn-Munkres (Hungarian) algorithm.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.HungarianAlgorithm.
 */
class HungarianAlgorithm {
public:
    struct Result {
        std::vector<int> jobToWorker;
        double sum = 0.0;
    };

    using WeightsAccessor = std::function<double(int j, int w)>;

    static Result hungarian(const WeightsAccessor& accessor, int jobs, int workers);
};

/**
 * @brief Maps BSP brush sides to their corresponding original faces (exact and merged).
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.geom.BrushSideFaceMapper.
 */
class BrushSideFaceMapper {
public:
    BrushSideFaceMapper(BspFileReader& reader, WindingFactory& windingFactory, const BspDecompileConfig& config);

    void load();

    std::optional<int> origFaceIndex(int brushSideI) const;
    std::vector<int> brushSideIndices(int origFaceI) const;

private:
    void identifyExactMatches();
    void identifyMergedMatches();

    BspFileReader& m_reader;
    WindingFactory& m_windingFactory;
    const BspDecompileConfig& m_config;

    std::vector<int> m_brushSideToOrigFace;
    QHash<int, QSet<int>> m_origFaceToBrushSide;
};

/**
 * @brief Maps areaportal entities to their original areaportal brushes.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.AreaportalMapper.
 */
class AreaportalMapper {
public:
    struct ReallocationData {
        QHash<int, int> mapping;
    };

    static ReallocationData createReallocationData(
        const BspData& bsp,
        const BspDecompileConfig& config,
        WindingFactory& windingFactory);
};

/**
 * @brief Maps occluder entities to their original non-world brushes and sides.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.util.OccluderMapper.
 */
class OccluderMapper {
public:
    struct ReallocationData {
        QHash<int, QSet<int>> occluderToBrushes;
        QHash<int, QHash<int, int>> brushToSideMapping;

        bool isOccluderBrush(int iBrush) const;
        bool isOccluderBrushSide(int iBrush, int side) const;
    };

    static ReallocationData createReallocationData(
        const BspData& bsp,
        const BspDecompileConfig& config,
        WindingFactory& windingFactory);
};

} // namespace Domain::Bsp::Internal
