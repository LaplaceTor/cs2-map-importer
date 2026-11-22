#pragma once

#include <optional>
#include <vector>

#include <QHash>
#include <QSet>
#include <QString>

#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspFileReader.h"
#include "Domain/Bsp/Internal/BspMath.h"
#include "Domain/Bsp/Internal/BspStructs.h"

namespace Domain::Bsp::Internal {

/**
 * @brief UV texture axis definition.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.TextureAxis.
 */
struct TextureAxis {
    Vector3d axis;
    int shift = 0;
    double tw = 0.25;

    TextureAxis() = default;
    TextureAxis(const Vector3d& axisIn, int shiftIn, double twIn)
        : axis(axisIn)
        , shift(shiftIn)
        , tw(twIn) {}
    explicit TextureAxis(const Vector3d& axisIn)
        : axis(axisIn)
        , shift(0)
        , tw(0.25) {}
    TextureAxis(double x, double y, double z, int shiftIn = 0, double twIn = 0.25)
        : axis{x, y, z}
        , shift(shiftIn)
        , tw(twIn) {}
};

/**
 * @brief VMF brush side / face texture descriptor.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.Texture.
 */
struct Texture {
    QString texture = QStringLiteral("tools/toolsskip");
    TextureAxis u{Vector3d{1.0, 0.0, 0.0}};
    TextureAxis v{Vector3d{0.0, 1.0, 0.0}};
    int lmscale = 16;
};

/**
 * @brief Standard Source engine tool texture material paths.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.ToolTexture.
 */
namespace ToolTexture {
inline const QString kNodraw = QStringLiteral("tools/toolsnodraw");
inline const QString kWhite = QStringLiteral("skybox/sky_fake_white");
inline const QString kBlack = QStringLiteral("tools/toolsblack");
inline const QString kInvisible = QStringLiteral("tools/toolsinvisible");
inline const QString kOrange = QStringLiteral("dev/dev_measuregeneric01");
inline const QString kSkip = QStringLiteral("tools/toolsskip");
inline const QString kHint = QStringLiteral("tools/toolshint");
inline const QString kClip = QStringLiteral("tools/toolsclip");
inline const QString kPlayerClip = QStringLiteral("tools/toolsplayerclip");
inline const QString kNpcClip = QStringLiteral("tools/toolsnpcclip");
inline const QString kAreaportal = QStringLiteral("tools/toolsareaportal");
inline const QString kBlockLight = QStringLiteral("tools/toolsblocklight");
inline const QString kBlockBullets = QStringLiteral("tools/toolsblockbullets");
inline const QString kBlockLos = QStringLiteral("tools/toolsblock_los");
inline const QString kOrigin = QStringLiteral("tools/toolsorigin");
inline const QString kLadder = QStringLiteral("tools/toolsladder");
inline const QString kInvisibleLadder = QStringLiteral("tools/toolsinvisibleladder");
inline const QString kDotted = QStringLiteral("tools/toolsdotted");
inline const QString kOccluder = QStringLiteral("tools/toolsoccluder");
inline const QString kTrigger = QStringLiteral("tools/toolstrigger");
inline const QString kFog = QStringLiteral("tools/toolsfog");
inline const QString kSkybox = QStringLiteral("tools/toolsskybox");
inline const QString kSkybox2d = QStringLiteral("tools/toolsskybox2d");

inline const QString kCsgoGrenadeClip = QStringLiteral("tools/toolsgrenadeclip");
inline const QString kCsgoDroneClip = QStringLiteral("tools/toolsdroneclip");

inline const QString kClipConcrete = QStringLiteral("tools/toolsclip_concrete");
inline const QString kClipDirt = QStringLiteral("tools/toolsclip_dirt");
inline const QString kClipGlass = QStringLiteral("tools/toolsclip_glass");
inline const QString kClipGrass = QStringLiteral("tools/toolsclip_grass");
inline const QString kClipGravel = QStringLiteral("tools/toolsclip_gravel");
inline const QString kClipMetal = QStringLiteral("tools/toolsclip_metal");
inline const QString kClipMetalSandBarrel = QStringLiteral("tools/toolsclip_metal_sand_barrel");
inline const QString kClipMetalGrate = QStringLiteral("tools/toolsclip_metalgrate");
inline const QString kClipMetalVehicle = QStringLiteral("tools/toolsclip_metalvehicle");
inline const QString kClipPlastic = QStringLiteral("tools/toolsclip_plastic");
inline const QString kClipRubber = QStringLiteral("tools/toolsclip_rubber");
inline const QString kClipRubberTire = QStringLiteral("tools/toolsclip_rubbertire");
inline const QString kClipSand = QStringLiteral("tools/toolsclip_sand");
inline const QString kClipSnow = QStringLiteral("tools/toolsclip_snow");
inline const QString kClipTile = QStringLiteral("tools/toolsclip_tile");
inline const QString kClipWood = QStringLiteral("tools/toolsclip_wood");
inline const QString kClipWoodBasket = QStringLiteral("tools/toolsclip_wood_basket");
inline const QString kClipWoodCrate = QStringLiteral("tools/toolsclip_wood_crate");

inline const QString kNodrawMetal = QStringLiteral("tools/toolsnodraw_metal");
inline const QString kInvisMetal = QStringLiteral("tools/invismetal");
inline const QString kClimb = QStringLiteral("tools/climb");
} // namespace ToolTexture

/**
 * @brief Tool texture definition used by ToolTextureMatcher.
 */
struct ToolTextureDefinition {
    std::optional<QString> surfaceProperty;
    QHash<BrushFlag, bool> brushFlagRequirements;
    QHash<SurfaceFlag, bool> surfaceFlagRequirements;
};

/**
 * @brief Reverses stripped tool texture names from brush/surface flags and surface properties.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.ToolTextureMatcher.
 */
class ToolTextureMatcher {
public:
    static ToolTextureMatcher forAppId(int appId);

    std::optional<QString> fixToolTexture(
        const QString& originalTextureName,
        const QSet<BrushFlag>& brushFlags,
        const QSet<SurfaceFlag>& surfaceFlags) const;

    std::optional<QString> fixToolTextureOptional(
        const QString& originalTextureName,
        const QSet<BrushFlag>& brushFlags,
        const QSet<SurfaceFlag>* surfaceFlags) const;

private:
    ToolTextureMatcher(
        QHash<QString, ToolTextureDefinition> definitions,
        bool clipOptimization);

    bool matchesSurfaceProperty(
        const ToolTextureDefinition& definition,
        const QString& originalTextureName) const;

    QHash<QString, ToolTextureDefinition> m_definitions;
    bool m_clipOptimization = true;
};

/**
 * @brief Builder helpers for constructing Texture structs from BSP texinfo/texdata.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.TextureBuilder.
 */
class TextureBuilder {
public:
    struct Axes {
        TextureAxis u;
        TextureAxis v;
    };

    static const DTexInfo* lookupTexinfo(
        int texinfoId,
        const std::vector<DTexInfo>& texinfos,
        const BspDecompileConfig& config);

    static const DTexData* lookupTexdata(
        int texdataId,
        const std::vector<DTexData>& texdatas,
        const BspDecompileConfig& config);

    static std::optional<QString> lookupTexname(
        int texnameId,
        const std::vector<QString>& texnames,
        const BspDecompileConfig& config);

    static Texture buildFromTexinfo(
        const DTexInfo& texinfo,
        const DTexData& texdata,
        const QString& material,
        const std::optional<Vector3d>& origin,
        const std::optional<Vector3d>& angles,
        const Vector3d& normal);

    static Texture buildFromNormal(const Vector3d& normal, const QString& material);

    static Axes defaultUV(const Vector3d& normal);

    static Axes buildUV(
        const DTexInfo& texinfo,
        const DTexData& texdata,
        const std::optional<Vector3d>& origin,
        const std::optional<Vector3d>& angles);

    static int buildLightmapScale(const DTexInfo& texinfo);

    static bool isPerpendicular(const Vector3d& uAxis, const Vector3d& vAxis, const Vector3d& normal);

    static std::optional<QString> fixToolTexture(
        const QString& originalTextureName,
        bool isOccluderBrush,
        bool isOccluderBrushSide,
        const QSet<BrushFlag>& brushFlags,
        const QSet<SurfaceFlag>* surfaceFlags,
        const ToolTextureMatcher& matcher);
};

/**
 * @brief Decompiling module for texture name canonization, cubemap unpatching, and side mapping.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.texture.TextureSource.
 */
class TextureSource {
public:
    TextureSource(BspFileReader& reader, const BspDecompileConfig& config);

    const ToolTextureMatcher& getToolTextureMatcher() const noexcept { return m_toolTextureMatcher; }

    void addBrushSideID(int itexname, int side);
    std::vector<int> getBrushSidesForCubemap(int icubemap) const;

    static QString canonizeTextureName(QString textureNew);
    const std::vector<QString>& getFixedTextureNames() const noexcept { return m_texnamesFixed; }

    static std::optional<QString> getTextureName(
        int itexinfo,
        const std::vector<DTexInfo>& texinfos,
        const std::vector<DTexData>& texdatas,
        const std::vector<QString>& texnames);

    static bool isPatchedMaterial(const QString& fileName);

private:
    void processTextureNames();
    void setCubemapForTexname(int itexname, int cx, int cy, int cz);

    BspFileReader& m_reader;
    const BspDecompileConfig& m_config;
    ToolTextureMatcher m_toolTextureMatcher;

    QHash<int, QSet<int>> m_cubemapToSideList;
    QHash<int, int> m_texnameToCubemap;
    std::vector<QString> m_texnamesFixed;
};

} // namespace Domain::Bsp::Internal
