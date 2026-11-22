#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <QFlags>
#include <QString>
#include <QStringList>
#include <QStringView>

#include "Domain/Bsp/Internal/BspMath.h"

namespace Domain::Bsp::Internal {

enum class LumpType : int {
    LUMP_UNKNOWN = -1,

    LUMP_ENTITIES = 0,
    LUMP_PLANES = 1,
    LUMP_TEXDATA = 2,
    LUMP_VERTEXES = 3,
    LUMP_VISIBILITY = 4,
    LUMP_NODES = 5,
    LUMP_TEXINFO = 6,
    LUMP_FACES = 7,
    LUMP_LIGHTING = 8,
    LUMP_OCCLUSION = 9,
    LUMP_LEAFS = 10,
    LUMP_FACEIDS = 11,
    LUMP_UNDEFINED = 11,
    LUMP_EDGES = 12,
    LUMP_SURFEDGES = 13,
    LUMP_MODELS = 14,
    LUMP_WORLDLIGHTS = 15,
    LUMP_LEAFFACES = 16,
    LUMP_LEAFBRUSHES = 17,
    LUMP_BRUSHES = 18,
    LUMP_BRUSHSIDES = 19,
    LUMP_AREAS = 20,
    LUMP_AREAPORTALS = 21,
    LUMP_PROPCOLLISION = 22,
    LUMP_UNUSED0 = 22,
    LUMP_PORTALS = 22,
    LUMP_PROPHULLS = 23,
    LUMP_UNUSED1 = 23,
    LUMP_CLUSTERS = 23,
    LUMP_PROPHULLVERTS = 24,
    LUMP_UNUSED2 = 24,
    LUMP_PORTALVERTS = 24,
    LUMP_PROPTRIS = 25,
    LUMP_UNUSED3 = 25,
    LUMP_CLUSTERPORTALS = 25,
    LUMP_DISPINFO = 26,
    LUMP_ORIGINALFACES = 27,
    LUMP_PHYSDISP = 28,
    LUMP_UNUSED = 28,
    LUMP_PHYSCOLLIDE = 29,
    LUMP_VERTNORMALS = 30,
    LUMP_VERTNORMALINDICES = 31,
    LUMP_DISP_LIGHTMAP_ALPHAS = 32,
    LUMP_DISP_VERTS = 33,
    LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS = 34,
    LUMP_GAME_LUMP = 35,
    LUMP_LEAFWATERDATA = 36,
    LUMP_PRIMITIVES = 37,
    LUMP_PRIMVERTS = 38,
    LUMP_PRIMINDICES = 39,
    LUMP_PAKFILE = 40,
    LUMP_CLIPPORTALVERTS = 41,
    LUMP_CUBEMAPS = 42,
    LUMP_TEXDATA_STRING_DATA = 43,
    LUMP_TEXDATA_STRING_TABLE = 44,
    LUMP_OVERLAYS = 45,
    LUMP_LEAFMINDISTTOWATER = 46,
    LUMP_FACE_MACRO_TEXTURE_INFO = 47,
    LUMP_DISP_TRIS = 48,
    LUMP_PROP_BLOB = 49,
    LUMP_PHYSCOLLIDESURFACE = 49,
    LUMP_WATEROVERLAYS = 50,
    LUMP_LEAF_AMBIENT_INDEX_HDR = 51,
    LUMP_LEAF_AMBIENT_INDEX = 52,
    LUMP_LIGHTING_HDR = 53,
    LUMP_WORLDLIGHTS_HDR = 54,
    LUMP_LEAF_AMBIENT_LIGHTING_HDR = 55,
    LUMP_LEAF_AMBIENT_LIGHTING = 56,
    LUMP_XZIPPAKFILE = 57,
    LUMP_FACES_HDR = 58,
    LUMP_MAP_FLAGS = 59,
    LUMP_OVERLAY_FADES = 60,
    LUMP_OVERLAY_SYSTEM_LEVELS = 61,
    LUMP_PHYSLEVEL = 62,
    LUMP_DISP_MULTIBLEND = 63,
};

struct LumpInfo {
    int index = -1;
    QString name;
    int bspVersion = -1;
};

inline const std::vector<LumpInfo>& lumpInfoTable() {
    static const std::vector<LumpInfo> table = {
        // v21
        {22, QStringLiteral("LUMP_PROPCOLLISION"), 21},
        {23, QStringLiteral("LUMP_PROPHULLS"), 21},
        {24, QStringLiteral("LUMP_PROPHULLVERTS"), 21},
        {25, QStringLiteral("LUMP_PROPTRIS"), 21},
        {49, QStringLiteral("LUMP_PROP_BLOB"), 21},
        {62, QStringLiteral("LUMP_PHYSLEVEL"), 21},
        {63, QStringLiteral("LUMP_DISP_MULTIBLEND"), 21},
        // v20
        {11, QStringLiteral("LUMP_FACEIDS"), 20},
        {22, QStringLiteral("LUMP_UNUSED0"), 20},
        {23, QStringLiteral("LUMP_UNUSED1"), 20},
        {24, QStringLiteral("LUMP_UNUSED2"), 20},
        {25, QStringLiteral("LUMP_UNUSED3"), 20},
        {28, QStringLiteral("LUMP_PHYSDISP"), 20},
        {50, QStringLiteral("LUMP_WATEROVERLAYS"), 20},
        {51, QStringLiteral("LUMP_LEAF_AMBIENT_INDEX_HDR"), 20},
        {52, QStringLiteral("LUMP_LEAF_AMBIENT_INDEX"), 20},
        {53, QStringLiteral("LUMP_LIGHTING_HDR"), 20},
        {54, QStringLiteral("LUMP_WORLDLIGHTS_HDR"), 20},
        {55, QStringLiteral("LUMP_LEAF_AMBIENT_LIGHTING_HDR"), 20},
        {56, QStringLiteral("LUMP_LEAF_AMBIENT_LIGHTING"), 20},
        {57, QStringLiteral("LUMP_XZIPPAKFILE"), 20},
        {58, QStringLiteral("LUMP_FACES_HDR"), 20},
        {59, QStringLiteral("LUMP_MAP_FLAGS"), 20},
        {60, QStringLiteral("LUMP_OVERLAY_FADES"), 20},
        {61, QStringLiteral("LUMP_OVERLAY_SYSTEM_LEVELS"), 20},
        // v19 and previous
        {0, QStringLiteral("LUMP_ENTITIES"), -1},
        {1, QStringLiteral("LUMP_PLANES"), -1},
        {2, QStringLiteral("LUMP_TEXDATA"), -1},
        {3, QStringLiteral("LUMP_VERTEXES"), -1},
        {4, QStringLiteral("LUMP_VISIBILITY"), -1},
        {5, QStringLiteral("LUMP_NODES"), -1},
        {6, QStringLiteral("LUMP_TEXINFO"), -1},
        {7, QStringLiteral("LUMP_FACES"), -1},
        {8, QStringLiteral("LUMP_LIGHTING"), -1},
        {9, QStringLiteral("LUMP_OCCLUSION"), -1},
        {10, QStringLiteral("LUMP_LEAFS"), -1},
        {11, QStringLiteral("LUMP_UNDEFINED"), -1},
        {12, QStringLiteral("LUMP_EDGES"), -1},
        {13, QStringLiteral("LUMP_SURFEDGES"), -1},
        {14, QStringLiteral("LUMP_MODELS"), -1},
        {15, QStringLiteral("LUMP_WORLDLIGHTS"), -1},
        {16, QStringLiteral("LUMP_LEAFFACES"), -1},
        {17, QStringLiteral("LUMP_LEAFBRUSHES"), -1},
        {18, QStringLiteral("LUMP_BRUSHES"), -1},
        {19, QStringLiteral("LUMP_BRUSHSIDES"), -1},
        {20, QStringLiteral("LUMP_AREAS"), -1},
        {21, QStringLiteral("LUMP_AREAPORTALS"), -1},
        {22, QStringLiteral("LUMP_PORTALS"), -1},
        {23, QStringLiteral("LUMP_CLUSTERS"), -1},
        {24, QStringLiteral("LUMP_PORTALVERTS"), -1},
        {25, QStringLiteral("LUMP_CLUSTERPORTALS"), -1},
        {26, QStringLiteral("LUMP_DISPINFO"), -1},
        {27, QStringLiteral("LUMP_ORIGINALFACES"), -1},
        {28, QStringLiteral("LUMP_UNUSED"), -1},
        {29, QStringLiteral("LUMP_PHYSCOLLIDE"), -1},
        {30, QStringLiteral("LUMP_VERTNORMALS"), -1},
        {31, QStringLiteral("LUMP_VERTNORMALINDICES"), -1},
        {32, QStringLiteral("LUMP_DISP_LIGHTMAP_ALPHAS"), -1},
        {33, QStringLiteral("LUMP_DISP_VERTS"), -1},
        {34, QStringLiteral("LUMP_DISP_LIGHTMAP_SAMPLE_POSITIONS"), -1},
        {35, QStringLiteral("LUMP_GAME_LUMP"), -1},
        {36, QStringLiteral("LUMP_LEAFWATERDATA"), -1},
        {37, QStringLiteral("LUMP_PRIMITIVES"), -1},
        {38, QStringLiteral("LUMP_PRIMVERTS"), -1},
        {39, QStringLiteral("LUMP_PRIMINDICES"), -1},
        {40, QStringLiteral("LUMP_PAKFILE"), -1},
        {41, QStringLiteral("LUMP_CLIPPORTALVERTS"), -1},
        {42, QStringLiteral("LUMP_CUBEMAPS"), -1},
        {43, QStringLiteral("LUMP_TEXDATA_STRING_DATA"), -1},
        {44, QStringLiteral("LUMP_TEXDATA_STRING_TABLE"), -1},
        {45, QStringLiteral("LUMP_OVERLAYS"), -1},
        {46, QStringLiteral("LUMP_LEAFMINDISTTOWATER"), -1},
        {47, QStringLiteral("LUMP_FACE_MACRO_TEXTURE_INFO"), -1},
        {48, QStringLiteral("LUMP_DISP_TRIS"), -1},
        {49, QStringLiteral("LUMP_PHYSCOLLIDESURFACE"), -1},
    };
    return table;
}

inline LumpInfo lumpInfoForIndex(int index, int bspVersion = -1) {
    for (const auto& entry : lumpInfoTable()) {
        if (entry.index == index && entry.bspVersion <= bspVersion) {
            return entry;
        }
    }
    return LumpInfo{-1, QStringLiteral("LUMP_UNKNOWN"), -1};
}

inline LumpType lumpTypeForIndex(int index, int bspVersion = -1) {
    const LumpInfo info = lumpInfoForIndex(index, bspVersion);
    return info.index >= 0 ? static_cast<LumpType>(info.index) : LumpType::LUMP_UNKNOWN;
}

inline bool canReadLump(LumpType type, int bspVersion) {
    if (type == LumpType::LUMP_UNKNOWN) {
        return false;
    }
    const LumpInfo info = lumpInfoForIndex(static_cast<int>(type), bspVersion);
    return info.index >= 0 && info.bspVersion <= bspVersion;
}

enum class SurfaceFlag : std::uint32_t {
    SURF_LIGHT = 1u << 0,
    SURF_SKY2D = 1u << 1,
    SURF_SKY = 1u << 2,
    SURF_WARP = 1u << 3,
    SURF_TRANS = 1u << 4,
    SURF_NOPORTAL = 1u << 5,
    SURF_TRIGGER = 1u << 6,
    SURF_NODRAW = 1u << 7,
    SURF_HINT = 1u << 8,
    SURF_SKIP = 1u << 9,
    SURF_NOLIGHT = 1u << 10,
    SURF_BUMPLIGHT = 1u << 11,
    SURF_NOSHADOWS = 1u << 12,
    SURF_NODECALS = 1u << 13,
    SURF_NOCHOP = 1u << 14,
    SURF_HITBOX = 1u << 15,
};
Q_DECLARE_FLAGS(SurfaceFlags, SurfaceFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(SurfaceFlags)

enum class BrushFlag : std::uint32_t {
    CONTENTS_SOLID = 1u << 0,
    CONTENTS_WINDOW = 1u << 1,
    CONTENTS_AUX = 1u << 2,
    CONTENTS_GRATE = 1u << 3,
    CONTENTS_SLIME = 1u << 4,
    CONTENTS_WATER = 1u << 5,
    CONTENTS_BLOCKLOS = 1u << 6,
    CONTENTS_OPAQUE = 1u << 7,
    CONTENTS_TESTFOGVOLUME = 1u << 8,
    CONTENTS_UNUSED = 1u << 9,
    CONTENTS_UNUSED6 = 1u << 10,
    CONTENTS_TEAM1 = 1u << 11,
    CONTENTS_TEAM2 = 1u << 12,
    CONTENTS_IGNORE_NODRAW_OPAQUE = 1u << 13,
    CONTENTS_MOVEABLE = 1u << 14,
    CONTENTS_AREAPORTAL = 1u << 15,
    CONTENTS_PLAYERCLIP = 1u << 16,
    CONTENTS_MONSTERCLIP = 1u << 17,
    CONTENTS_CURRENT_0 = 1u << 18,
    CONTENTS_CURRENT_90 = 1u << 19,
    CONTENTS_CURRENT_180 = 1u << 20,
    CONTENTS_CURRENT_270 = 1u << 21,
    CONTENTS_CURRENT_UP = 1u << 22,
    CONTENTS_CURRENT_DOWN = 1u << 23,
    CONTENTS_ORIGIN = 1u << 24,
    CONTENTS_MONSTER = 1u << 25,
    CONTENTS_DEBRIS = 1u << 26,
    CONTENTS_DETAIL = 1u << 27,
    CONTENTS_TRANSLUCENT = 1u << 28,
    CONTENTS_LADDER = 1u << 29,
    CONTENTS_HITBOX = 1u << 30,
};
Q_DECLARE_FLAGS(BrushFlags, BrushFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(BrushFlags)

enum class LevelFlag : std::uint32_t {
    LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_NONHDR = 1u << 0,
    LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR = 1u << 1,
};
Q_DECLARE_FLAGS(LevelFlags, LevelFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(LevelFlags)

constexpr SurfaceFlags surfaceFlagsFromInt(std::int32_t raw) noexcept {
    return SurfaceFlags::fromInt(static_cast<SurfaceFlags::Int>(raw));
}

constexpr std::int32_t surfaceFlagsToInt(SurfaceFlags flags) noexcept {
    return static_cast<std::int32_t>(flags.toInt());
}

constexpr BrushFlags brushFlagsFromInt(std::int32_t raw) noexcept {
    return BrushFlags::fromInt(static_cast<BrushFlags::Int>(raw));
}

constexpr std::int32_t brushFlagsToInt(BrushFlags flags) noexcept {
    return static_cast<std::int32_t>(flags.toInt());
}

constexpr LevelFlags levelFlagsFromInt(std::int32_t raw) noexcept {
    return LevelFlags::fromInt(static_cast<LevelFlags::Int>(raw));
}

constexpr std::int32_t levelFlagsToInt(LevelFlags flags) noexcept {
    return static_cast<std::int32_t>(flags.toInt());
}

namespace detail {

template <typename Flags, typename Enum, std::size_t N>
inline QString formatFlagTable(
    Flags flags,
    const std::array<std::pair<Enum, QLatin1StringView>, N>& table,
    QLatin1StringView separator,
    bool bracketed)
{
    QString out;
    if (bracketed) {
        out += QLatin1Char('[');
    }
    bool first = true;
    for (const auto& [flag, name] : table) {
        if (flags.testFlag(flag)) {
            if (!first) {
                out += separator;
            }
            out += name;
            first = false;
        }
    }
    if (bracketed) {
        out += QLatin1Char(']');
    }
    return out;
}

inline constexpr std::array<std::pair<SurfaceFlag, QLatin1StringView>, 16> kSurfaceFlagNames{{
    {SurfaceFlag::SURF_LIGHT, QLatin1StringView("SURF_LIGHT")},
    {SurfaceFlag::SURF_SKY2D, QLatin1StringView("SURF_SKY2D")},
    {SurfaceFlag::SURF_SKY, QLatin1StringView("SURF_SKY")},
    {SurfaceFlag::SURF_WARP, QLatin1StringView("SURF_WARP")},
    {SurfaceFlag::SURF_TRANS, QLatin1StringView("SURF_TRANS")},
    {SurfaceFlag::SURF_NOPORTAL, QLatin1StringView("SURF_NOPORTAL")},
    {SurfaceFlag::SURF_TRIGGER, QLatin1StringView("SURF_TRIGGER")},
    {SurfaceFlag::SURF_NODRAW, QLatin1StringView("SURF_NODRAW")},
    {SurfaceFlag::SURF_HINT, QLatin1StringView("SURF_HINT")},
    {SurfaceFlag::SURF_SKIP, QLatin1StringView("SURF_SKIP")},
    {SurfaceFlag::SURF_NOLIGHT, QLatin1StringView("SURF_NOLIGHT")},
    {SurfaceFlag::SURF_BUMPLIGHT, QLatin1StringView("SURF_BUMPLIGHT")},
    {SurfaceFlag::SURF_NOSHADOWS, QLatin1StringView("SURF_NOSHADOWS")},
    {SurfaceFlag::SURF_NODECALS, QLatin1StringView("SURF_NODECALS")},
    {SurfaceFlag::SURF_NOCHOP, QLatin1StringView("SURF_NOCHOP")},
    {SurfaceFlag::SURF_HITBOX, QLatin1StringView("SURF_HITBOX")},
}};

inline constexpr std::array<std::pair<BrushFlag, QLatin1StringView>, 29> kBrushFlagNames{{
    {BrushFlag::CONTENTS_SOLID, QLatin1StringView("CONTENTS_SOLID")},
    {BrushFlag::CONTENTS_WINDOW, QLatin1StringView("CONTENTS_WINDOW")},
    {BrushFlag::CONTENTS_AUX, QLatin1StringView("CONTENTS_AUX")},
    {BrushFlag::CONTENTS_GRATE, QLatin1StringView("CONTENTS_GRATE")},
    {BrushFlag::CONTENTS_SLIME, QLatin1StringView("CONTENTS_SLIME")},
    {BrushFlag::CONTENTS_WATER, QLatin1StringView("CONTENTS_WATER")},
    {BrushFlag::CONTENTS_BLOCKLOS, QLatin1StringView("CONTENTS_BLOCKLOS")},
    {BrushFlag::CONTENTS_OPAQUE, QLatin1StringView("CONTENTS_OPAQUE")},
    {BrushFlag::CONTENTS_TESTFOGVOLUME, QLatin1StringView("CONTENTS_TESTFOGVOLUME")},
    {BrushFlag::CONTENTS_TEAM1, QLatin1StringView("CONTENTS_TEAM1")},
    {BrushFlag::CONTENTS_TEAM2, QLatin1StringView("CONTENTS_TEAM2")},
    {BrushFlag::CONTENTS_IGNORE_NODRAW_OPAQUE, QLatin1StringView("CONTENTS_IGNORE_NODRAW_OPAQUE")},
    {BrushFlag::CONTENTS_MOVEABLE, QLatin1StringView("CONTENTS_MOVEABLE")},
    {BrushFlag::CONTENTS_AREAPORTAL, QLatin1StringView("CONTENTS_AREAPORTAL")},
    {BrushFlag::CONTENTS_PLAYERCLIP, QLatin1StringView("CONTENTS_PLAYERCLIP")},
    {BrushFlag::CONTENTS_MONSTERCLIP, QLatin1StringView("CONTENTS_MONSTERCLIP")},
    {BrushFlag::CONTENTS_CURRENT_0, QLatin1StringView("CONTENTS_CURRENT_0")},
    {BrushFlag::CONTENTS_CURRENT_90, QLatin1StringView("CONTENTS_CURRENT_90")},
    {BrushFlag::CONTENTS_CURRENT_180, QLatin1StringView("CONTENTS_CURRENT_180")},
    {BrushFlag::CONTENTS_CURRENT_270, QLatin1StringView("CONTENTS_CURRENT_270")},
    {BrushFlag::CONTENTS_CURRENT_UP, QLatin1StringView("CONTENTS_CURRENT_UP")},
    {BrushFlag::CONTENTS_CURRENT_DOWN, QLatin1StringView("CONTENTS_CURRENT_DOWN")},
    {BrushFlag::CONTENTS_ORIGIN, QLatin1StringView("CONTENTS_ORIGIN")},
    {BrushFlag::CONTENTS_MONSTER, QLatin1StringView("CONTENTS_MONSTER")},
    {BrushFlag::CONTENTS_DEBRIS, QLatin1StringView("CONTENTS_DEBRIS")},
    {BrushFlag::CONTENTS_DETAIL, QLatin1StringView("CONTENTS_DETAIL")},
    {BrushFlag::CONTENTS_TRANSLUCENT, QLatin1StringView("CONTENTS_TRANSLUCENT")},
    {BrushFlag::CONTENTS_LADDER, QLatin1StringView("CONTENTS_LADDER")},
    {BrushFlag::CONTENTS_HITBOX, QLatin1StringView("CONTENTS_HITBOX")},
}};

} // namespace detail

inline QString formatSurfaceFlags(SurfaceFlags flags) {
    return detail::formatFlagTable(flags, detail::kSurfaceFlagNames, QLatin1StringView(" | "), false);
}

inline QString formatSurfaceFlagsBracketed(SurfaceFlags flags) {
    return detail::formatFlagTable(flags, detail::kSurfaceFlagNames, QLatin1StringView(", "), true);
}

inline QString formatBrushFlags(BrushFlags flags) {
    return detail::formatFlagTable(flags, detail::kBrushFlagNames, QLatin1StringView(" | "), false);
}

inline QString formatBrushFlagsBracketed(BrushFlags flags) {
    return detail::formatFlagTable(flags, detail::kBrushFlagNames, QLatin1StringView(", "), true);
}

struct Color32 {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
    std::int32_t rgba = 0;

    constexpr Color32() noexcept = default;

    explicit constexpr Color32(std::int32_t value) noexcept
        : r(value & 0xFF)
        , g((value >> 8) & 0xFF)
        , b((value >> 16) & 0xFF)
        , a((value >> 24) & 0xFF)
        , rgba(value) {}

    constexpr Color32(int red, int green, int blue, int alpha) noexcept
        : r(red & 0xFF)
        , g(green & 0xFF)
        , b(blue & 0xFF)
        , a(alpha & 0xFF)
        , rgba(static_cast<std::int32_t>(
              (static_cast<std::uint32_t>(a) << 24)
              | (static_cast<std::uint32_t>(b) << 16)
              | (static_cast<std::uint32_t>(g) << 8)
              | static_cast<std::uint32_t>(r))) {}
};

struct DPlane {
    Vector3f normal;
    float dist = 0.0f;
    int type = 0;
};

struct DVertex {
    Vector3f point;
};

struct DEdge {
    std::array<int, 2> v{0, 0};
};

struct DFace {
    static constexpr int kMaxLightmaps = 4;

    int pnum = 0;
    std::uint8_t side = 0;
    std::uint8_t onnode = 0;
    int fstedge = 0;
    int numedge = 0;
    int texinfo = -1;
    int dispInfo = -1;
    int surfaceFogVolumeID = 0;
    std::array<std::uint8_t, 4> styles{};
    int lightofs = 0;
    float area = 0.0f;
    std::array<int, 2> LightmapTextureMinsInLuxels{};
    std::array<int, 2> LightmapTextureSizeInLuxels{};
    int origFace = -1;
    int numPrims = 0;
    int firstPrimID = 0;
    int smoothingGroups = 0;
};

struct DBrush {
    int fstside = 0;
    int numside = 0;
    BrushFlags contents;

    bool hasContentFlag(BrushFlag flag) const noexcept {
        return contents.testFlag(flag);
    }

    bool isSolid() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_SOLID); }
    bool isDetail() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_DETAIL); }
    bool isOpaque() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_OPAQUE); }
    bool isTranslucent() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_TRANSLUCENT); }
    bool isAreaportal() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_AREAPORTAL); }
    bool isLadder() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_LADDER); }
    bool isPlayerClip() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_PLAYERCLIP); }
    bool isMonsterClip() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_MONSTERCLIP); }
    bool isNpcClip() const noexcept { return isMonsterClip(); }
    bool isBlockLos() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_BLOCKLOS); }
    bool isGrate() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_GRATE); }
    bool isWindow() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_WINDOW); }
    bool isWater() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_WATER); }
    bool isSlime() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_SLIME); }
    bool isIgnoreNodrawOpaque() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_IGNORE_NODRAW_OPAQUE); }
    bool isCurrent90() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_CURRENT_90); }
    bool isCurrent180() const noexcept { return hasContentFlag(BrushFlag::CONTENTS_CURRENT_180); }
};

struct DBrushSide {
    int pnum = 0;
    int texinfo = -1;
    int dispinfo = 0;
    bool bevel = false;
    bool thin = false;
};

struct DModel {
    Vector3f mins;
    Vector3f maxs;
    Vector3f origin;
    int headnode = 0;
    int fstface = 0;
    int numface = 0;
};

struct DNode {
    int planenum = 0;
    std::array<int, 2> children{0, 0};
    Vector3f mins;
    Vector3f maxs;
    int fstface = 0;
    int numface = 0;
    int area = 0;
};

struct DLeaf {
    int contents = 0;
    int cluster = -1;
    int areaFlags = 0;
    Vector3f mins;
    Vector3f maxs;
    int fstleafface = 0;
    int numleafface = 0;
    int fstleafbrush = 0;
    int numleafbrush = 0;
    int leafWaterDataID = -1;
};

struct DTexInfo {
    static constexpr int kTexInfoNode = -1;

    float textureVecsTexels[2][4]{};
    float lightmapVecsLuxels[2][4]{};
    SurfaceFlags flags;
    int texdata = -1;

    bool hasSurfaceFlag(SurfaceFlag flag) const noexcept {
        return flags.testFlag(flag);
    }
};

struct DTexData {
    Vector3f reflectivity;
    int texname = -1;
    int width = 0;
    int height = 0;
    int viewWidth = 0;
    int viewHeight = 0;
};

struct DDispInfo {
    static constexpr std::uint32_t kDispInfoFlagHasMultiBlend = 0x40000000u;
    static constexpr std::uint32_t kDispInfoFlagMagic = 0x80000000u;

    Vector3f startPos;
    int dispVertStart = 0;
    int dispTriStart = 0;
    int power = 0;
    int minTess = 0;
    float smoothingAngle = 0.0f;
    int contents = 0;
    int mapFace = 0;
    int lightmapAlphaStart = 0;
    int lightmapSamplePositionStart = 0;
    std::array<int, 10> allowedVerts{};

    int powerSize() const noexcept { return 1 << power; }

    int vertexCount() const noexcept {
        const int s = powerSize() + 1;
        return s * s;
    }

    int triangleTagCount() const noexcept {
        const int s = powerSize();
        return 2 * s * s;
    }

    bool hasMultiBlend() const noexcept {
        return ((static_cast<std::uint32_t>(minTess) + kDispInfoFlagMagic) & kDispInfoFlagHasMultiBlend) != 0u;
    }

    int surfaceFlags() const noexcept {
        const std::uint32_t u = static_cast<std::uint32_t>(minTess);
        if ((u & kDispInfoFlagMagic) != 0u) {
            return static_cast<int>(u & ~(kDispInfoFlagMagic | kDispInfoFlagHasMultiBlend));
        }
        return 0;
    }
};

struct DDispVert {
    Vector3f vector;
    float dist = 0.0f;
    float alpha = 0.0f;
};

struct DDispTri {
    int tags = 0;
};

struct DDispMultiBlend {
    static constexpr int kMaxMultiBlendChannels = 4;

    Vector4f multiblend;
    Vector4f alphablend;
    std::array<Vector3f, kMaxMultiBlendChannels> multiblendcolors{};
};

struct DOverlay {
    static constexpr int kOverlayBspFaceCount = 64;
    static constexpr int kOverlayRenderOrderNumBits = 2;
    static constexpr int kOverlayRenderOrderMask = 0xC000;

    int id = 0;
    int texinfo = -1;
    int faceCountAndRenderOrder = 0;
    std::array<int, 64> ofaces{};
    std::array<float, 2> u{};
    std::array<float, 2> v{};
    std::array<Vector3f, 4> uvpoints{};
    Vector3f origin;
    Vector3f basisNormal;

    int faceCount() const noexcept { return faceCountAndRenderOrder & 0x3fff; }
    int renderOrder() const noexcept { return (faceCountAndRenderOrder >> 14) & 0x3; }
};

struct DOverlayFade {
    float fadeDistMinSq = 0.0f;
    float fadeDistMaxSq = 0.0f;
};

struct DOverlaySystemLevel {
    int minCPULevel = 0;
    int maxCPULevel = 0;
    int minGPULevel = 0;
    int maxGPULevel = 0;
};

struct DAreaportal {
    int portalKey = 0;
    int otherportal = 0;
    int firstClipPortalVert = 0;
    int clipPortalVerts = 0;
    int planenum = 0;
};

struct DOccluderData {
    int flags = 0;
    int firstpoly = 0;
    int polycount = 0;
    Vector3f mins;
    Vector3f maxs;
    int area = 0;
};

struct DOccluderPolyData {
    int firstvertexindex = 0;
    int vertexcount = 0;
    int planenum = 0;
};

struct DCubemapSample {
    std::array<int, 3> origin{0, 0, 0};
    int size = 0;
};

struct DPrimitive {
    int type = 0;
    int firstIndex = 0;
    int indexCount = 0;
    int firstVert = 0;
    int vertCount = 0;
};

struct DStaticProp {
    // Base (V4)
    Vector3f origin;
    Vector3f angles;
    int propType = 0;
    int firstLeaf = 0;
    int leafCount = 0;
    int solid = 0;
    std::uint32_t flagsRaw = 0;
    int skin = 0;
    float fademin = 0.0f;
    float fademax = 0.0f;
    Vector3f lightingOrigin;
    bool disableLightingOrigin = false;

    // V5+
    bool hasV5 = false;
    float forcedFadeScale = 0.0f;

    // V5Ship / V6BGT
    bool hasTargetName = false;
    QString targetname;

    // V6+
    bool hasV6 = false;
    int minDXLevel = 0;
    int maxDXLevel = 0;

    // V7L4D / V8 / V11lite
    bool hasDiffuseModulation = false;
    bool isV11Lite = false;
    Color32 diffuseModulation;

    // V8+
    bool hasV8 = false;
    int minCPULevel = 0;
    int maxCPULevel = 0;
    int minGPULevel = 0;
    int maxGPULevel = 0;

    // V9+
    bool hasV9 = false;
    bool disableX360 = false;

    // V10+
    bool hasV10 = false;
    std::uint32_t flagsExRaw = 0;
    int lightmapResolutionX = 0;
    int lightmapResolutionY = 0;

    // V10CSGO+
    bool hasV10Csgo = false;

    // V11CSGO
    bool hasV11Csgo = false;
    float uniformScale = 1.0f;

    // V13
    bool hasV13 = false;
    Vector3f scale{1.0f, 1.0f, 1.0f};

    // VinScaling
    bool hasVinScaling = false;
    Vector3f vinScaling{1.0f, 1.0f, 1.0f};

    bool hasFade() const noexcept { return (flagsRaw & 1u) != 0u; }
    bool usesLightingOrigin() const noexcept { return !disableLightingOrigin && (flagsRaw & 2u) != 0u; }
    bool hasNoDraw() const noexcept { return (flagsRaw & 4u) != 0u; }
    bool hasIgnoreNormals() const noexcept { return (flagsRaw & 8u) != 0u; }
    bool hasNoShadowing() const noexcept { return (flagsRaw & 16u) != 0u; }
    bool hasScreenSpaceFadeInPixels() const noexcept { return !hasV10Csgo && (flagsRaw & 32u) != 0u; }
    bool hasNoPerVertexLighting() const noexcept { return (flagsRaw & 64u) != 0u; }
    bool hasNoSelfShadowing() const noexcept { return (flagsRaw & 128u) != 0u; }
    bool hasNoPerTexelLighting() const noexcept { return (flagsRaw & 256u) != 0u; }
    bool hasRenderInFastReflection() const noexcept { return hasV10Csgo && (flagsRaw & 32u) != 0u; }
    bool hasDisableShadowDepth() const noexcept { return (flagsExRaw & 1u) != 0u; }
    bool hasDisableCsm() const noexcept { return (flagsExRaw & 2u) != 0u; }
    bool hasEnableLightBounce() const noexcept { return (flagsExRaw & 4u) != 0u; }
};

struct EntityKeyValue {
    QString key;
    QString value;
};

class BspEntity {
public:
    BspEntity() = default;

    explicit BspEntity(QString className)
        : m_className(className.isEmpty() ? QStringLiteral("unknown_entity") : std::move(className)) {}

    explicit BspEntity(const std::vector<EntityKeyValue>& rawKeyValues) {
        for (const auto& kv : rawKeyValues) {
            if (kv.key == QLatin1StringView("classname")) {
                if (m_className.isEmpty()) {
                    m_className = kv.value;
                }
                continue;
            }
            if (isEntityIoValue(kv.value)) {
                m_io.push_back(kv);
            } else {
                setValue(kv.key, kv.value);
            }
        }
        if (m_className.isEmpty()) {
            m_className = QStringLiteral("unknown_entity");
        }
    }

    static bool isEntityIoValue(QStringView value) noexcept {
        if (value.count(QChar(0x1b)) == 4) {
            return true;
        }
        const qsizetype commaCount = value.count(u',');
        return commaCount == 4 || commaCount == 6;
    }

    const QString& className() const noexcept { return m_className; }
    void setClassName(const QString& name) { m_className = name; }

    std::optional<QString> value(QStringView key) const {
        for (const auto& kv : m_keyValues) {
            if (kv.key == key) {
                return kv.value;
            }
        }
        return std::nullopt;
    }

    void setValue(const QString& key, const QString& val) {
        for (auto& kv : m_keyValues) {
            if (kv.key == key) {
                kv.value = val;
                return;
            }
        }
        m_keyValues.push_back(EntityKeyValue{key, val});
    }

    void setValue(const QString& key, int val) {
        setValue(key, QString::number(val));
    }

    void removeValue(QStringView key) {
        for (auto it = m_keyValues.begin(); it != m_keyValues.end(); ++it) {
            if (it->key == key) {
                m_keyValues.erase(it);
                return;
            }
        }
    }

    bool hasKey(QStringView key) const {
        return value(key).has_value();
    }

    void clear() {
        m_keyValues.clear();
        m_io.clear();
    }

    const std::vector<EntityKeyValue>& keyValues() const noexcept { return m_keyValues; }
    std::vector<EntityKeyValue>& mutableKeyValues() noexcept { return m_keyValues; }
    const std::vector<EntityKeyValue>& io() const noexcept { return m_io; }
    std::vector<EntityKeyValue>& mutableIo() noexcept { return m_io; }

    void addIo(const QString& key, const QString& val) {
        m_io.push_back(EntityKeyValue{key, val});
    }

    std::optional<QString> targetName() const {
        return value(QStringLiteral("targetname"));
    }

    std::optional<Vector3f> vector3f(QStringView key) const {
        auto strOpt = value(key);
        if (!strOpt.has_value()) {
            return std::nullopt;
        }
        const QStringList parts = strOpt->split(u' ', Qt::KeepEmptyParts);
        auto parsePart = [&](qsizetype idx, float& out) -> bool {
            if (idx >= parts.size()) {
                out = 0.0f;
                return true;
            }
            bool ok = false;
            out = parts[idx].toFloat(&ok);
            return ok;
        };
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!parsePart(0, x) || !parsePart(1, y) || !parsePart(2, z)) {
            return std::nullopt;
        }
        return Vector3f{x, y, z};
    }

    std::optional<Vector3f> origin() const {
        return vector3f(QStringLiteral("origin"));
    }

    std::optional<Vector3f> angles() const {
        auto a = vector3f(QStringLiteral("angles"));
        if (!a.has_value()) {
            return std::nullopt;
        }
        // Reorder Source (Pitch, Yaw, Roll) to (Roll, Pitch, Yaw) aligned with (X, Y, Z)
        return Vector3f{a->z, a->x, a->y};
    }

    int modelNum() const {
        auto modelOpt = value(QStringLiteral("model"));
        if (!modelOpt.has_value()) {
            return -1;
        }
        const QString& model = *modelOpt;
        if (model.startsWith(u'*')) {
            bool ok = false;
            const int id = QStringView(model).sliced(1).toInt(&ok);
            return ok ? id : -2;
        }
        if (model.isEmpty()) {
            return 0;
        }
        return -2;
    }

    void setModelNum(int num) {
        setValue(QStringLiteral("model"), QStringLiteral("*") + QString::number(num));
    }

private:
    QString m_className;
    std::vector<EntityKeyValue> m_keyValues;
    std::vector<EntityKeyValue> m_io;
};

struct NmoObjective {
    int id = 0;
    QString name;
    QString comment;
    QString entityName;
    std::vector<QString> entities;
    std::vector<int> children;
};

struct NmoAntiObjective {
    int id = 0;
    QString name;
    QString comment;
    std::vector<QString> entities;
};

struct NmoExtraction {
    int id = 0;
    QString name;
};

struct NmoData {
    std::vector<NmoObjective> nodes;
    std::vector<NmoAntiObjective> antiNodes;
    std::vector<NmoExtraction> extractions;
};

struct BspData {
    std::vector<BspEntity> entities;
    std::vector<DPlane> planes;
    std::vector<DBrush> brushes;
    std::vector<DBrushSide> brushSides;
    std::vector<DVertex> verts;
    std::vector<DVertex> clipPortalVerts;
    std::vector<DEdge> edges;
    std::vector<DFace> faces;
    std::vector<DFace> origFaces;
    std::vector<DModel> models;
    std::vector<int> surfEdges;
    std::vector<QString> staticPropName;
    std::vector<DStaticProp> staticProps;
    std::vector<int> staticPropLeaf;
    std::vector<DCubemapSample> cubemaps;
    std::vector<DDispInfo> dispinfos;
    std::vector<DDispVert> dispverts;
    std::vector<DDispTri> disptris;
    std::vector<DDispMultiBlend> dispmultiblend;
    std::vector<DTexInfo> texinfos;
    std::vector<DTexData> texdatas;
    std::vector<QString> texnames;
    std::vector<DNode> nodes;
    std::vector<DLeaf> leaves;
    std::vector<int> leafFaces;
    std::vector<int> leafBrushes;
    std::vector<DOverlay> overlays;
    std::vector<DOverlayFade> overlayFades;
    std::vector<DOverlaySystemLevel> overlaySysLevels;
    std::vector<DAreaportal> areaportals;
    std::vector<DOccluderData> occluderDatas;
    std::vector<DOccluderPolyData> occluderPolyDatas;
    std::vector<int> occluderVerts;
    LevelFlags mapFlags;
    std::vector<DPrimitive> prims;
    std::vector<int> primIndices;
    std::vector<DVertex> primVerts;
};

} // namespace Domain::Bsp::Internal
