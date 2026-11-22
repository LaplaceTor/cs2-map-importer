#pragma once

#include <QHash>
#include <QSet>
#include <QString>

#include <optional>
#include <vector>

#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspFileReader.h"
#include "Domain/Bsp/Internal/BspMappers.h"
#include "Domain/Bsp/Internal/BspMath.h"
#include "Domain/Bsp/Internal/BspProtection.h"
#include "Domain/Bsp/Internal/BspStructs.h"
#include "Domain/Bsp/Internal/TextureSource.h"
#include "Domain/Bsp/Internal/VmfWriter.h"
#include "Domain/Bsp/Internal/Winding.h"

namespace Domain::Bsp::Internal {

/**
 * @brief Decompiling module to rebuild brushes from LUMP_BRUSHES and LUMP_BRUSHSIDES.
 */
class BrushSource {
public:
    BrushSource(
        BspFileReader& reader,
        VmfWriter& writer,
        const BspDecompileConfig& config,
        TextureSource& texsrc,
        const BspProtection& bspprot,
        VmfMeta& vmfmeta,
        const BrushSideFaceMapper& brushSideFaceMapper,
        WindingFactory& windingFactory,
        const OccluderMapper::ReallocationData& occReallocationData);

    bool isFuncDetail(const DBrush& dBrush) const noexcept;

    int brushSideIdForIndex(int ibrushside) const;
    int brushIdForIndex(int ibrush) const;

    void writeBrushes();
    bool writeBrush(
        int ibrush,
        const std::optional<Vector3d>& origin = std::nullopt,
        const std::optional<Vector3d>& angles = std::nullopt);
    bool writeModel(
        int imodel,
        const std::optional<Vector3d>& origin = std::nullopt,
        const std::optional<Vector3d>& angles = std::nullopt);

    int worldBrushCount() const noexcept { return m_worldbrushes; }

private:
    struct DBrushModel {
        int fstbrush = 0;
        int numbrush = 0;
    };

    void assignBrushes();
    bool writeSide(
        int ibrushside,
        int ibrush,
        const Winding& wind,
        const std::optional<Vector3d>& origin,
        const std::optional<Vector3d>& angles);

    BspFileReader& m_reader;
    BspFile& m_bspFile;
    BspData& m_bsp;
    VmfWriter& m_writer;
    const BspDecompileConfig& m_config;
    TextureSource& m_texsrc;
    const BspProtection& m_bspprot;
    VmfMeta& m_vmfmeta;
    const BrushSideFaceMapper& m_brushSideFaceMapper;
    WindingFactory& m_windingFactory;
    const OccluderMapper::ReallocationData& m_occReallocationData;

    std::vector<DBrushModel> m_models;
    int m_worldbrushes = 0;

    std::vector<int> m_brushSideToId;
    std::vector<int> m_brushIndexToId;
};

/**
 * @brief Decompiling module to write brushes, displacements, areaportals, and occluders from LUMP_FACES.
 */
class FaceSource {
public:
    FaceSource(
        BspFileReader& reader,
        VmfWriter& writer,
        const BspDecompileConfig& config,
        TextureSource& texsrc,
        VmfMeta& vmfmeta,
        WindingFactory& windingFactory,
        const OccluderMapper::ReallocationData& occReallocationData);

    void writeFaces();
    void writeOrigFaces();
    void writeOrigFacesPlus();
    void writeDispFaces();

    void writeModel(
        int imodel,
        const std::optional<Vector3d>& origin = std::nullopt,
        const std::optional<Vector3d>& angles = std::nullopt);

    void writeFace(
        int iface,
        bool orig,
        const std::optional<Vector3d>& origin = std::nullopt,
        const std::optional<Vector3d>& angles = std::nullopt);

    void writeAreaportal(int portalKey);

    void writeOccluder(int occluderKey);
    void writeOccluder(const DOccluderData& od);

    void writePolygon(
        const Winding& wind,
        const QString& frontMaterial,
        const QString& backMaterial,
        bool prism,
        double depth);
    void writePolygon(
        const Winding& wind,
        const QString& frontMaterial,
        const QString& backMaterial,
        bool prism);
    void writePolygon(
        const Winding& wind,
        const QString& material,
        bool prism,
        double depth);
    void writePolygon(
        const Winding& wind,
        const QString& material,
        bool prism);

    void writeDisplacement(int idispinfo);

private:
    void writePrismBack(const Winding& wind, Texture texture, double depth);
    void writePrismBack(const Winding& wind, Texture texture);
    void writePyramBack(const Winding& wind, const Texture& texture, double depth);
    void writePyramBack(const Winding& wind, const Texture& texture);
    void writeBackSide(const Texture& texture, const Vector3d& e1, const Vector3d& e2, const Vector3d& e3);
    void createFaceMapping();

    BspFileReader& m_reader;
    BspFile& m_bspFile;
    BspData& m_bsp;
    VmfWriter& m_writer;
    const BspDecompileConfig& m_config;
    TextureSource& m_texsrc;
    VmfMeta& m_vmfmeta;
    WindingFactory& m_windingFactory;
    const OccluderMapper::ReallocationData& m_occReallocationData;

    QHash<int, QSet<int>> m_origFaceToSplitFace;
    QSet<int> m_undersizedFaces;
    int m_multiblendOffset = 0;
};

/**
 * @brief Decompiling module to write point and brush entities converted from various lumps.
 */
class EntitySource {
public:
    EntitySource(
        BspFileReader& reader,
        VmfWriter& writer,
        const BspDecompileConfig& config,
        BrushSource& brushsrc,
        FaceSource& facesrc,
        TextureSource& texsrc,
        const BspProtection& bspprot,
        VmfMeta& vmfmeta,
        const BrushSideFaceMapper& brushSideFaceMapper,
        WindingFactory& windingFactory,
        BrushBounds& brushBounds,
        const AreaportalMapper::ReallocationData& areaportalReallocationData,
        const OccluderMapper::ReallocationData& occluderReallocationData);

    void writeEntities();
    void writeDetails();
    void writeVisClusters();
    void writeOverlays();
    void writeStaticProps();
    void writeCubemaps();
    void writeLadders();

    void setNmo(const NmoData& nmoData);

    static void writeDebugPortals(
        const BspData& bsp,
        FaceSource& faceSource,
        WindingFactory& windingFactory,
        VmfWriter& writer,
        VmfMeta& vmfMeta);

    static void writeDebugOccluders(
        const BspData& bsp,
        FaceSource& faceSource,
        WindingFactory& windingFactory,
        VmfWriter& writer,
        VmfMeta& vmfMeta);

private:
    void processEntities();
    int hammerId(const BspEntity& ent) const;
    void fixLightEntity(BspEntity& ent) const;
    void createCamera(const BspEntity& ent);

    BspFileReader& m_reader;
    BspFile& m_bspFile;
    BspData& m_bsp;
    VmfWriter& m_writer;
    const BspDecompileConfig& m_config;
    BrushSource& m_brushsrc;
    FaceSource& m_facesrc;
    TextureSource& m_texsrc;
    const BspProtection& m_bspprot;
    VmfMeta& m_vmfmeta;
    const BrushSideFaceMapper& m_brushSideFaceMapper;
    WindingFactory& m_windingFactory;
    BrushBounds& m_brushBounds;
    const AreaportalMapper::ReallocationData& m_areaportalReallocationData;
    const OccluderMapper::ReallocationData& m_occluderReallocationData;

    std::optional<NmoData> m_nmo;
    QHash<int, QString> m_overlayNames;
};

} // namespace Domain::Bsp::Internal
