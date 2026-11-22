#include "Domain/Bsp/Internal/TextureSource.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QRegularExpression>

#include "Domain/Bsp/Internal/SourceAppDb.h"

namespace Domain::Bsp::Internal {

namespace {

constexpr float kEpsPerp = 0.02f;

const QRegularExpression& wvtPatchRegex() {
    static const QRegularExpression re{QStringLiteral("maps/[^/]*/(.+)_wvt_patch")};
    return re;
}

const QRegularExpression& waterPatchRegex() {
    static const QRegularExpression re{QStringLiteral("maps/[^/]*/(.+)_depth_-?\\d+")};
    return re;
}

const QRegularExpression& originPatchRegex() {
    static const QRegularExpression re{QStringLiteral("maps/[^/]*/(.+)_(-?\\d+)_(-?\\d+)_(-?\\d+)")};
    return re;
}

class ToolTextureDefBuilder {
public:
    ToolTextureDefBuilder() = default;
    explicit ToolTextureDefBuilder(QString surfaceProperty)
        : m_def{std::move(surfaceProperty), {}, {}} {}
    explicit ToolTextureDefBuilder(const ToolTextureDefinition& base)
        : m_def(base) {}

    ToolTextureDefBuilder& setSurfaceProperty(QString prop) {
        m_def.surfaceProperty = std::move(prop);
        return *this;
    }

    template <typename... Flags>
    ToolTextureDefBuilder& setRequiredBrushFlags(Flags... flags) {
        ((m_def.brushFlagRequirements.insert(flags, true)), ...);
        return *this;
    }

    template <typename... Flags>
    ToolTextureDefBuilder& setForbiddenBrushFlags(Flags... flags) {
        ((m_def.brushFlagRequirements.insert(flags, false)), ...);
        return *this;
    }

    template <typename... Flags>
    ToolTextureDefBuilder& setRequiredSurfaceFlags(Flags... flags) {
        ((m_def.surfaceFlagRequirements.insert(flags, true)), ...);
        return *this;
    }

    ToolTextureDefBuilder& clearSurfaceFlagRequirements() {
        m_def.surfaceFlagRequirements.clear();
        return *this;
    }

    ToolTextureDefinition build() const { return m_def; }

private:
    ToolTextureDefinition m_def;
};

QHash<QString, ToolTextureDefinition> buildSourceToolTextures() {
    using enum BrushFlag;
    using enum SurfaceFlag;
    QHash<QString, ToolTextureDefinition> map;

    map.insert(ToolTexture::kAreaportal,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_AREAPORTAL)
            .setRequiredSurfaceFlags(SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kBlockBullets,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_WINDOW, CONTENTS_TRANSLUCENT)
            .setRequiredSurfaceFlags(SURF_TRANS, SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kBlockLight,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_OPAQUE, CONTENTS_DETAIL)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kBlockLos,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_BLOCKLOS, CONTENTS_DETAIL)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kInvisible,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_GRATE, CONTENTS_TRANSLUCENT)
            .setForbiddenBrushFlags(CONTENTS_SOLID)
            .setRequiredSurfaceFlags(SURF_TRANS, SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kInvisibleLadder,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_GRATE, CONTENTS_TRANSLUCENT, CONTENTS_LADDER)
            .setForbiddenBrushFlags(CONTENTS_SOLID)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kNodraw,
        ToolTextureDefBuilder{}
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kTrigger,
        ToolTextureDefBuilder{}
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT, SURF_TRIGGER)
            .build());
    map.insert(ToolTexture::kHint,
        ToolTextureDefBuilder{}
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_HINT, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kSkip,
        ToolTextureDefBuilder{}
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_SKIP, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kClip,
        ToolTextureDefBuilder{QStringLiteral("default_silent")}
            .setRequiredBrushFlags(CONTENTS_PLAYERCLIP, CONTENTS_MONSTERCLIP, CONTENTS_DETAIL)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kNpcClip,
        ToolTextureDefBuilder{QStringLiteral("default_silent")}
            .setRequiredBrushFlags(CONTENTS_MONSTERCLIP, CONTENTS_DETAIL)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kPlayerClip,
        ToolTextureDefBuilder{}
            .setRequiredBrushFlags(CONTENTS_PLAYERCLIP, CONTENTS_DETAIL)
            .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kSkybox,
        ToolTextureDefBuilder{QStringLiteral("default_silent")}
            .setRequiredBrushFlags(CONTENTS_SOLID)
            .setRequiredSurfaceFlags(SURF_SKY, SURF_NOLIGHT)
            .build());
    map.insert(ToolTexture::kSkybox2d,
        ToolTextureDefBuilder{QStringLiteral("default_silent")}
            .setRequiredBrushFlags(CONTENTS_SOLID)
            .setRequiredSurfaceFlags(SURF_SKY, SURF_SKY2D, SURF_NOLIGHT)
            .build());

    return map;
}

QHash<QString, ToolTextureDefinition> buildCsgoClipVariants(const ToolTextureDefinition& clipBase) {
    QHash<QString, ToolTextureDefinition> map;
    map.insert(ToolTexture::kClipConcrete,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("concrete")).build());
    map.insert(ToolTexture::kClipDirt,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("dirt")).build());
    map.insert(ToolTexture::kClipGlass,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("glassfloor")).build());
    map.insert(ToolTexture::kClipGrass,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("grass")).build());
    map.insert(ToolTexture::kClipGravel,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("gravel")).build());
    map.insert(ToolTexture::kClipMetal,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("metal")).build());
    map.insert(ToolTexture::kClipMetalSandBarrel,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("metal_sand_barrel")).build());
    map.insert(ToolTexture::kClipMetalGrate,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("metalgrate")).build());
    map.insert(ToolTexture::kClipMetalVehicle,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("metalvehicle")).build());
    map.insert(ToolTexture::kClipPlastic,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("plastic")).build());
    map.insert(ToolTexture::kClipRubber,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("rubber")).build());
    map.insert(ToolTexture::kClipRubberTire,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("rubbertire")).build());
    map.insert(ToolTexture::kClipSand,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("sand")).build());
    map.insert(ToolTexture::kClipSnow,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("snow")).build());
    map.insert(ToolTexture::kClipTile,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("tile")).build());
    map.insert(ToolTexture::kClipWood,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("wood")).build());
    map.insert(ToolTexture::kClipWoodBasket,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("Wood_Basket")).build());
    map.insert(ToolTexture::kClipWoodCrate,
        ToolTextureDefBuilder{clipBase}.setSurfaceProperty(QStringLiteral("Wood_Crate")).build());
    return map;
}

QHash<QString, ToolTextureDefinition> buildToolTextureSetForApp(int appId) {
    QHash<QString, ToolTextureDefinition> defs = buildSourceToolTextures();

    if (appId == SourceAppId::kCounterStrikeSource) {
        defs.insert(ToolTexture::kClip,
            ToolTextureDefBuilder{defs.value(ToolTexture::kClip)}.clearSurfaceFlagRequirements().build());
        defs.insert(ToolTexture::kNpcClip,
            ToolTextureDefBuilder{defs.value(ToolTexture::kNpcClip)}.clearSurfaceFlagRequirements().build());
        defs.insert(ToolTexture::kPlayerClip,
            ToolTextureDefBuilder{defs.value(ToolTexture::kPlayerClip)}.clearSurfaceFlagRequirements().build());
    } else if (appId == SourceAppId::kCounterStrikeGo) {
        using enum BrushFlag;
        using enum SurfaceFlag;
        defs.insert(ToolTexture::kCsgoGrenadeClip,
            ToolTextureDefBuilder{}
                .setRequiredBrushFlags(CONTENTS_CURRENT_90, CONTENTS_DETAIL)
                .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
                .build());
        defs.insert(ToolTexture::kCsgoDroneClip,
            ToolTextureDefBuilder{}
                .setRequiredBrushFlags(CONTENTS_CURRENT_180, CONTENTS_DETAIL)
                .setRequiredSurfaceFlags(SURF_NODRAW, SURF_NOLIGHT)
                .build());

        const auto clips = buildCsgoClipVariants(defs.value(ToolTexture::kClip));
        for (auto it = clips.cbegin(); it != clips.cend(); ++it) {
            defs.insert(it.key(), it.value());
        }
    } else if (appId == SourceAppId::kLeft4Dead2) {
        using enum BrushFlag;
        using enum SurfaceFlag;
        defs.insert(ToolTexture::kNodrawMetal,
            ToolTextureDefBuilder{defs.value(ToolTexture::kNodraw)}
                .setSurfaceProperty(QStringLiteral("metal"))
                .build());
        defs.insert(ToolTexture::kClimb,
            ToolTextureDefBuilder{}
                .setRequiredBrushFlags(CONTENTS_TEAM2, CONTENTS_GRATE, CONTENTS_TRANSLUCENT, CONTENTS_LADDER)
                .setRequiredSurfaceFlags(SURF_TRANS, SURF_NODRAW, SURF_NOLIGHT)
                .build());
        defs.insert(ToolTexture::kInvisMetal,
            ToolTextureDefBuilder{QStringLiteral("metal")}
                .setRequiredBrushFlags(CONTENTS_WINDOW, CONTENTS_TRANSLUCENT)
                .setRequiredSurfaceFlags(SURF_TRANS, SURF_NODRAW, SURF_NOLIGHT)
                .build());
    } else if (appId == SourceAppId::kGarrysMod) {
        using enum BrushFlag;
        defs.insert(ToolTexture::kInvisibleLadder,
            ToolTextureDefBuilder{defs.value(ToolTexture::kInvisibleLadder)}
                .setRequiredBrushFlags(CONTENTS_DETAIL)
                .build());

        auto clips = buildCsgoClipVariants(defs.value(ToolTexture::kClip));
        clips.remove(ToolTexture::kClipSnow); // GMod doesn't have CLIP_SNOW
        for (auto it = clips.cbegin(); it != clips.cend(); ++it) {
            defs.insert(it.key(), it.value());
        }
    }

    return defs;
}

template <typename Enum>
bool matchesRequirements(const QHash<Enum, bool>& requirements, const QSet<Enum>* set) {
    if (set == nullptr) {
        return true;
    }
    for (auto it = requirements.cbegin(); it != requirements.cend(); ++it) {
        if (set->contains(it.key()) != it.value()) {
            return false;
        }
    }
    return true;
}

int ttDefinitionScore(const ToolTextureDefinition& definition, bool ignoreSurfaceFlags) {
    const int brushFlagRequirements = definition.brushFlagRequirements.size();
    const int surfaceFlagRequirements = definition.surfaceFlagRequirements.size();
    return brushFlagRequirements + (ignoreSurfaceFlags ? 0 : surfaceFlagRequirements);
}

int javaRoundToInt(double v) noexcept {
    if (!std::isfinite(v)) {
        return 0;
    }
    return static_cast<int>(std::floor(v + 0.5));
}

} // namespace

// ---------------------------------------------------------------------------
// ToolTextureMatcher
// ---------------------------------------------------------------------------

ToolTextureMatcher::ToolTextureMatcher(
    QHash<QString, ToolTextureDefinition> definitions,
    bool clipOptimization)
    : m_definitions(std::move(definitions))
    , m_clipOptimization(clipOptimization) {}

ToolTextureMatcher ToolTextureMatcher::forAppId(int appId) {
    const bool clipOpt = (appId != SourceAppId::kCounterStrikeGo && appId != SourceAppId::kGarrysMod);
    return ToolTextureMatcher{buildToolTextureSetForApp(appId), clipOpt};
}

std::optional<QString> ToolTextureMatcher::fixToolTexture(
    const QString& originalTextureName,
    const QSet<BrushFlag>& brushFlags,
    const QSet<SurfaceFlag>& surfaceFlags) const {
    return fixToolTextureOptional(originalTextureName, brushFlags, &surfaceFlags);
}

std::optional<QString> ToolTextureMatcher::fixToolTextureOptional(
    const QString& originalTextureName,
    const QSet<BrushFlag>& brushFlags,
    const QSet<SurfaceFlag>* surfaceFlags) const {
    const bool isClip = brushFlags.contains(BrushFlag::CONTENTS_PLAYERCLIP)
        || brushFlags.contains(BrushFlag::CONTENTS_MONSTERCLIP);
    const bool ignoreSurfaceFlags = (surfaceFlags == nullptr);

    std::optional<QString> bestName;
    int bestScore = 0;

    for (auto it = m_definitions.cbegin(); it != m_definitions.cend(); ++it) {
        const ToolTextureDefinition& def = it.value();

        if (!((isClip && m_clipOptimization) || matchesSurfaceProperty(def, originalTextureName))) {
            continue;
        }
        if (!matchesRequirements(def.brushFlagRequirements, &brushFlags)) {
            continue;
        }
        if (!((isClip && m_clipOptimization) || matchesRequirements(def.surfaceFlagRequirements, surfaceFlags))) {
            continue;
        }

        const int score = ttDefinitionScore(def, ignoreSurfaceFlags);
        if (score > bestScore) {
            bestScore = score;
            bestName = it.key();
        }
    }

    return bestName;
}

bool ToolTextureMatcher::matchesSurfaceProperty(
    const ToolTextureDefinition& definition,
    const QString& originalTextureName) const {
    if (originalTextureName.isEmpty()) {
        return true;
    }

    const auto it = m_definitions.constFind(originalTextureName);
    if (it == m_definitions.constEnd()) {
        return true;
    }

    const std::optional<QString>& origProp = it.value().surfaceProperty;
    const std::optional<QString>& candProp = definition.surfaceProperty;
    if (!origProp.has_value() && !candProp.has_value()) {
        return true;
    }
    if (!origProp.has_value() || !candProp.has_value()) {
        return false;
    }
    return origProp->compare(*candProp, Qt::CaseInsensitive) == 0;
}

// ---------------------------------------------------------------------------
// TextureBuilder
// ---------------------------------------------------------------------------

const DTexInfo* TextureBuilder::lookupTexinfo(
    int texinfoId,
    const std::vector<DTexInfo>& texinfos,
    const BspDecompileConfig& config) {
    if (texinfoId == DTexInfo::kTexInfoNode) {
        return nullptr;
    }
    if (texinfoId < 0 || static_cast<std::size_t>(texinfoId) >= texinfos.size()) {
        config.log(
            BspLogLevel::Warning,
            QCoreApplication::translate("Domain::Bsp::Internal::TextureBuilder", "Invalid texinfo index: %1")
                .arg(texinfoId));
        return nullptr;
    }
    return &texinfos[static_cast<std::size_t>(texinfoId)];
}

const DTexData* TextureBuilder::lookupTexdata(
    int texdataId,
    const std::vector<DTexData>& texdatas,
    const BspDecompileConfig& config) {
    if (texdataId < 0 || static_cast<std::size_t>(texdataId) >= texdatas.size()) {
        config.log(
            BspLogLevel::Warning,
            QCoreApplication::translate("Domain::Bsp::Internal::TextureBuilder", "Invalid texdata index: %1")
                .arg(texdataId));
        return nullptr;
    }
    return &texdatas[static_cast<std::size_t>(texdataId)];
}

std::optional<QString> TextureBuilder::lookupTexname(
    int texnameId,
    const std::vector<QString>& texnames,
    const BspDecompileConfig& config) {
    if (texnameId < 0 || static_cast<std::size_t>(texnameId) >= texnames.size()) {
        config.log(
            BspLogLevel::Warning,
            QCoreApplication::translate("Domain::Bsp::Internal::TextureBuilder", "Invalid texname index: %1")
                .arg(texnameId));
        return std::nullopt;
    }
    return texnames[static_cast<std::size_t>(texnameId)];
}

Texture TextureBuilder::buildFromTexinfo(
    const DTexInfo& texinfo,
    const DTexData& texdata,
    const QString& material,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles,
    const Vector3d& normal) {
    const int lightmapScale = buildLightmapScale(texinfo);
    Axes uv = buildUV(texinfo, texdata, origin, angles);
    if (isPerpendicular(uv.u.axis, uv.v.axis, normal)) {
        uv = defaultUV(normal);
    }

    Texture texture;
    texture.texture = material;
    texture.u = uv.u;
    texture.v = uv.v;
    texture.lmscale = lightmapScale;
    return texture;
}

Texture TextureBuilder::buildFromNormal(const Vector3d& normal, const QString& material) {
    const Axes uv = defaultUV(normal);
    Texture texture;
    texture.texture = material;
    texture.u = uv.u;
    texture.v = uv.v;
    return texture;
}

TextureBuilder::Axes TextureBuilder::defaultUV(const Vector3d& normal) {
    const double dotX = std::abs(Vector3d::kBaseVectorX.dot(normal));
    const double dotY = std::abs(Vector3d::kBaseVectorY.dot(normal));
    const double dotZ = std::abs(Vector3d::kBaseVectorZ.dot(normal));

    const Vector3d vdir = (dotZ > dotX && dotZ > dotY)
        ? Vector3d::kBaseVectorY
        : Vector3d::kBaseVectorZ;

    const Vector3d tv1 = normal.cross(vdir).normalize();
    const Vector3d tv2 = normal.cross(tv1).normalize();

    return Axes{
        TextureAxis{tv1, 0, 0.25},
        TextureAxis{tv2, 0, 0.25}};
}

TextureBuilder::Axes TextureBuilder::buildUV(
    const DTexInfo& texinfo,
    const DTexData& texdata,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles) {
    const auto& tvec = texinfo.textureVecsTexels;
    Vector3d uaxis{tvec[0][0], tvec[0][1], tvec[0][2]};
    Vector3d vaxis{tvec[1][0], tvec[1][1], tvec[1][2]};

    const double utw = 1.0 / uaxis.length();
    const double vtw = 1.0 / vaxis.length();

    uaxis = uaxis.scalar(utw);
    vaxis = vaxis.scalar(vtw);

    double ushift = static_cast<double>(tvec[0][3]);
    double vshift = static_cast<double>(tvec[1][3]);

    if (origin.has_value()) {
        ushift -= origin->dot(uaxis) / utw;
        vshift -= origin->dot(vaxis) / vtw;
    }

    if (angles.has_value()) {
        uaxis = uaxis.rotate(*angles);
        vaxis = vaxis.rotate(*angles);

        Vector3d shift = Vector3d::kNull;
        if (origin.has_value()) {
            shift = shift.sub(*origin);
        }
        shift = shift.rotate(*angles);
        if (origin.has_value()) {
            shift = shift.add(*origin);
        }

        ushift -= shift.dot(uaxis) / utw;
        vshift -= shift.dot(vaxis) / vtw;
    }

    if (texdata.width != 0) {
        ushift = std::fmod(ushift, static_cast<double>(texdata.width));
    }
    if (texdata.height != 0) {
        vshift = std::fmod(vshift, static_cast<double>(texdata.height));
    }

    return Axes{
        TextureAxis{uaxis, javaRoundToInt(ushift), utw},
        TextureAxis{vaxis, javaRoundToInt(vshift), vtw}};
}

int TextureBuilder::buildLightmapScale(const DTexInfo& texinfo) {
    const auto& lvec = texinfo.lightmapVecsLuxels;
    const Vector3d uaxis{lvec[0][0], lvec[0][1], lvec[0][2]};
    const Vector3d vaxis{lvec[1][0], lvec[1][1], lvec[1][2]};

    const double ls = (uaxis.length() + vaxis.length()) / 2.0;
    return ls > 0.001 ? javaRoundToInt(1.0 / ls) : 16;
}

bool TextureBuilder::isPerpendicular(const Vector3d& uAxis, const Vector3d& vAxis, const Vector3d& normal) {
    const Vector3d texNorm = uAxis.cross(vAxis);
    return std::abs(normal.dot(texNorm)) < kEpsPerp;
}

std::optional<QString> TextureBuilder::fixToolTexture(
    const QString& originalTextureName,
    bool isOccluderBrush,
    bool isOccluderBrushSide,
    const QSet<BrushFlag>& brushFlags,
    const QSet<SurfaceFlag>* surfaceFlags,
    const ToolTextureMatcher& matcher) {
    if (isOccluderBrush) {
        return isOccluderBrushSide ? ToolTexture::kOccluder : ToolTexture::kNodraw;
    }
    return matcher.fixToolTextureOptional(originalTextureName, brushFlags, surfaceFlags);
}

// ---------------------------------------------------------------------------
// TextureSource
// ---------------------------------------------------------------------------

TextureSource::TextureSource(BspFileReader& reader, const BspDecompileConfig& config)
    : m_reader(reader)
    , m_config(config)
    , m_toolTextureMatcher(ToolTextureMatcher::forAppId(reader.getBspFile().appId())) {
    m_reader.loadTexInfo();
    m_reader.loadTexData();
    m_reader.loadCubemaps();

    processTextureNames();
}

void TextureSource::processTextureNames() {
    const BspData& bsp = m_reader.getData();
    m_texnamesFixed.clear();
    m_texnamesFixed.reserve(bsp.texnames.size());

    for (std::size_t i = 0; i < bsp.texnames.size(); ++i) {
        const QString& textureOld = bsp.texnames[i];
        QString textureNew = canonizeTextureName(textureOld);

        if (const QRegularExpressionMatch m = wvtPatchRegex().match(textureNew); m.hasMatch()) {
            textureNew.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1));
        }
        if (const QRegularExpressionMatch m = waterPatchRegex().match(textureNew); m.hasMatch()) {
            textureNew.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1));
        }
        if (const QRegularExpressionMatch m = originPatchRegex().match(textureNew); m.hasMatch()) {
            bool okX = false;
            bool okY = false;
            bool okZ = false;
            const int cx = m.capturedView(2).toInt(&okX);
            const int cy = m.capturedView(3).toInt(&okY);
            const int cz = m.capturedView(4).toInt(&okZ);

            if (okX && okY && okZ) {
                setCubemapForTexname(static_cast<int>(i), cx, cy, cz);
            } else {
                m_config.log(
                    BspLogLevel::Warning,
                    QCoreApplication::translate(
                        "Domain::Bsp::Internal::TextureSource",
                        "Error parsing cubemap position from regex, input: %1")
                        .arg(textureNew));
            }

            textureNew.replace(m.capturedStart(0), m.capturedLength(0), m.captured(1));
        }

        m_texnamesFixed.push_back(std::move(textureNew));
    }
}

void TextureSource::setCubemapForTexname(int itexname, int cx, int cy, int cz) {
    const BspData& bsp = m_reader.getData();
    for (std::size_t i = 0; i < bsp.cubemaps.size(); ++i) {
        const auto& origin = bsp.cubemaps[i].origin;
        if (cx == origin[0] || cy == origin[1] || cz == origin[2]) {
            m_texnameToCubemap.insert(itexname, static_cast<int>(i));
            return;
        }
    }
}

void TextureSource::addBrushSideID(int itexname, int side) {
    const auto it = m_texnameToCubemap.constFind(itexname);
    if (it == m_texnameToCubemap.constEnd()) {
        return;
    }
    m_cubemapToSideList[it.value()].insert(side);
}

std::vector<int> TextureSource::getBrushSidesForCubemap(int icubemap) const {
    const auto it = m_cubemapToSideList.constFind(icubemap);
    if (it == m_cubemapToSideList.constEnd()) {
        return {};
    }
    std::vector<int> sides;
    sides.reserve(static_cast<std::size_t>(it.value().size()));
    for (int side : it.value()) {
        sides.push_back(side);
    }
    std::sort(sides.begin(), sides.end());
    return sides;
}

QString TextureSource::canonizeTextureName(QString textureNew) {
    return std::move(textureNew).toLower().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

std::optional<QString> TextureSource::getTextureName(
    int itexinfo,
    const std::vector<DTexInfo>& texinfos,
    const std::vector<DTexData>& texdatas,
    const std::vector<QString>& texnames) {
    if (itexinfo < 0 || static_cast<std::size_t>(itexinfo) >= texinfos.size()) {
        return std::nullopt;
    }
    const int ti = texinfos[static_cast<std::size_t>(itexinfo)].texdata;
    if (ti < 0 || static_cast<std::size_t>(ti) >= texdatas.size()) {
        return std::nullopt;
    }
    const int td = texdatas[static_cast<std::size_t>(ti)].texname;
    if (td < 0 || static_cast<std::size_t>(td) >= texnames.size()) {
        return std::nullopt;
    }
    return texnames[static_cast<std::size_t>(td)];
}

bool TextureSource::isPatchedMaterial(const QString& fileName) {
    const QString canonizedName = canonizeTextureName(fileName);
    return originPatchRegex().match(canonizedName).hasMatch()
        || wvtPatchRegex().match(canonizedName).hasMatch()
        || waterPatchRegex().match(canonizedName).hasMatch();
}

} // namespace Domain::Bsp::Internal
