#include "Domain/Bsp/Internal/BspFileReader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <utility>

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QSet>

#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Domain/Bsp/Internal/BspMath.h"
#include "Domain/Bsp/Internal/SourceAppDb.h"
#include "Domain/Package/Internal/Lzma1Decoder.h"

namespace Domain::Bsp::Internal {
namespace {

constexpr std::uint32_t kBspIdLE = 0x50534256U; // "VBSP" little-endian (or "PSBV" big-endian)
constexpr std::uint32_t kBspIdTfLE = 0x50534272U; // "rBSP" byte-swapped
constexpr std::uint32_t kGoldSrcVersion = 0x1EU;
constexpr std::uint32_t kZipLocalSigBE = 0x504B0304U;
constexpr std::uint32_t kZipEocdSigBE = 0x504B0506U;
constexpr std::uint32_t kZipSpannedSigBE = 0x504B0708U;
constexpr std::size_t kXorKeyOffset = 384;
constexpr std::size_t kXorKeyLength = 32;
constexpr int kLumpFileHeaderSize = 20;
constexpr int kNmoSignature = 118;
constexpr int kNmoVersion = 1;
constexpr qint64 kMaxBspFileBytes = 1024LL * 1024LL * 1024LL; // 1 GiB safety guard

std::uint32_t readU32BE(const std::byte* p) noexcept {
    return (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0])) << 24)
         | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1])) << 16)
         | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[2])) << 8)
         |  static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[3]));
}

std::uint32_t readU32LE(const std::byte* p) noexcept {
    return  static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]))
         | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1])) << 8)
         | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[2])) << 16)
         | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[3])) << 24);
}

std::int32_t peekInt32At(std::span<const std::byte> data, std::size_t offset, bool bigEndian) noexcept {
    const std::uint32_t u = bigEndian ? readU32BE(data.data() + offset) : readU32LE(data.data() + offset);
    return static_cast<std::int32_t>(u);
}

QString nameWithoutExtension(const Core::Path::FilesystemPath& file) {
    const QString fileName = file.fileName();
    const qsizetype dot = fileName.lastIndexOf(u'.');
    return dot == -1 ? fileName : fileName.left(dot);
}

QString fourCCToString(std::int32_t fourCC) {
    const auto u = static_cast<std::uint32_t>(fourCC);
    std::array<char, 4> chars{
        static_cast<char>((u >> 24) & 0xFFU),
        static_cast<char>((u >> 16) & 0xFFU),
        static_cast<char>((u >> 8) & 0xFFU),
        static_cast<char>(u & 0xFFU),
    };
    return QString::fromLatin1(chars.data(), 4);
}

bool isValidFourCCAscii(std::int32_t fourCC) noexcept {
    const auto u = static_cast<std::uint32_t>(fourCC);
    for (int shift = 0; shift <= 24; shift += 8) {
        const auto ch = static_cast<unsigned char>((u >> shift) & 0xFFU);
        if (!std::isalnum(ch)) {
            return false;
        }
    }
    return true;
}

Core::Result<std::vector<std::byte>> readAllFileBytes(const Core::Path::FilesystemPath& path) {
    QFile file(path.toString());
    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("BspFile", "Failed to open file for reading"),
            path.toString());
    }

    const qint64 size = file.size();
    if (size < 0 || size > kMaxBspFileBytes) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::OutOfMemory,
            QCoreApplication::translate("BspFile", "File size exceeds safety limit"),
            path.toString());
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::size_t totalRead = 0;
    while (totalRead < bytes.size()) {
        const qint64 chunk = file.read(
            reinterpret_cast<char*>(bytes.data() + totalRead),
            static_cast<qint64>(bytes.size() - totalRead));
        if (chunk <= 0) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::ReadFailed,
                QCoreApplication::translate("BspFile", "Failed to read file contents"),
                path.toString());
        }
        totalRead += static_cast<std::size_t>(chunk);
    }

    return Core::Result<std::vector<std::byte>>::success(std::move(bytes));
}

bool checkInvalidGameLumpHeaders(BinarySpanReader& in, bool vindictus) {
    const std::size_t savedPos = in.position();
    if (in.remaining() < 4) {
        in.setPosition(savedPos);
        return true;
    }

    const int glumps = in.readInt32();
    const std::size_t entrySize = vindictus ? 20U : 16U;
    if (glumps < 0 || static_cast<std::size_t>(glumps) > in.remaining() / entrySize) {
        in.setPosition(savedPos);
        return true;
    }

    for (int i = 0; i < glumps; ++i) {
        const std::int32_t fourCC = in.readInt32();
        if (!isValidFourCCAscii(fourCC)) {
            in.setPosition(savedPos);
            return true;
        }
        in.seek(static_cast<std::ptrdiff_t>(vindictus ? 16 : 12));
    }

    in.setPosition(savedPos);
    return false;
}

template <typename T, typename ReadFn>
std::vector<T> readStructChunks(
    const BspLumpBuffer& lump,
    bool bigEndian,
    std::size_t structSize,
    const BspDecompileConfig& config,
    ReadFn&& readOne)
{
    std::vector<T> result;
    if (lump.data.empty()) {
        return result;
    }

    try {
        BinarySpanReader reader(lump.data, bigEndian);
        if (structSize > 0) {
            result.reserve(lump.data.size() / structSize);
        }
        while (reader.hasRemaining()) {
            const std::size_t startPos = reader.position();
            result.push_back(readOne(reader));
            if (structSize > 0 && reader.position() - startPos != structSize) {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Struct size mismatch in lump %1: read %2, expected %3")
                        .arg(lump.index)
                        .arg(reader.position() - startPos)
                        .arg(structSize));
                break;
            }
        }
    } catch (const std::exception& ex) {
        config.log(BspLogLevel::Warning,
            QStringLiteral("Error reading lump %1: %2")
                .arg(lump.index)
                .arg(QString::fromUtf8(ex.what())));
        result.clear();
    }
    return result;
}

DPlane readDPlane(BinarySpanReader& in) {
    DPlane p;
    p.normal = in.readVector3f();
    p.dist = in.readFloat();
    p.type = in.readInt32();
    return p;
}

DBrush readDBrush(BinarySpanReader& in) {
    DBrush b;
    b.fstside = in.readInt32();
    b.numside = in.readInt32();
    b.contentsRaw = in.readInt32();
    b.contents = brushFlagsFromInt(b.contentsRaw);
    return b;
}

DBrushSide readDBrushSideDefault(BinarySpanReader& in) {
    DBrushSide s;
    s.pnum = in.readUInt16();
    s.texinfo = in.readInt16();
    s.dispinfo = in.readInt16();
    s.bevel = (in.readInt16() == 1);
    return s;
}

DBrushSide readDBrushSideV0New(BinarySpanReader& in) {
    DBrushSide s;
    s.pnum = in.readUInt16();
    s.texinfo = in.readInt16();
    s.dispinfo = in.readInt16();
    s.bevel = (in.readUInt8() != 0);
    s.thin = (in.readUInt8() != 0);
    return s;
}

DBrushSide readDBrushSideVin(BinarySpanReader& in) {
    DBrushSide s;
    s.pnum = in.readInt32();
    s.texinfo = static_cast<std::int16_t>(in.readInt32());
    s.dispinfo = static_cast<std::int16_t>(in.readInt32());
    s.bevel = (static_cast<std::int16_t>(in.readInt32()) == 1);
    return s;
}

DBrushSide readDBrushSideStrataV1(BinarySpanReader& in) {
    DBrushSide s;
    s.pnum = static_cast<int>(in.readUInt32());
    s.texinfo = in.readInt32();
    s.dispinfo = in.readInt32();
    s.bevel = (in.readUInt8() != 0);
    s.thin = (in.readUInt8() != 0);
    in.seek(2); // padding
    return s;
}

DVertex readDVertex(BinarySpanReader& in) {
    DVertex v;
    v.point = in.readVector3f();
    return v;
}

DEdge readDEdgeDefault(BinarySpanReader& in) {
    DEdge e;
    e.v[0] = in.readUInt16();
    e.v[1] = in.readUInt16();
    return e;
}

DEdge readDEdgeVin(BinarySpanReader& in) {
    DEdge e;
    e.v[0] = in.readInt32();
    e.v[1] = in.readInt32();
    return e;
}

DEdge readDEdgeStrataV1(BinarySpanReader& in) {
    DEdge e;
    e.v[0] = static_cast<int>(in.readUInt32());
    e.v[1] = static_cast<int>(in.readUInt32());
    return e;
}

DFace readDFaceDefault(BinarySpanReader& in) {
    DFace f;
    f.pnum = in.readUInt16();
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    f.fstedge = in.readInt32();
    f.numedge = in.readInt16();
    f.texinfo = in.readInt16();
    f.dispInfo = in.readInt16();
    f.surfaceFogVolumeID = in.readUInt16();
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.numPrims = in.readUInt16();
    f.firstPrimID = in.readUInt16();
    f.smoothingGroups = in.readInt32();
    return f;
}

DFace readDFaceBSP17(BinarySpanReader& in) {
    in.seek(16); // avgLightColor[4]
    DFace f;
    f.pnum = in.readUInt16();
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    f.fstedge = in.readInt32();
    f.numedge = in.readInt16();
    f.texinfo = in.readInt16();
    f.dispInfo = in.readInt16();
    f.surfaceFogVolumeID = in.readUInt16();
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.smoothingGroups = in.readInt32();
    return f;
}

DFace readDFaceBSP18(BinarySpanReader& in) {
    in.seek(16); // avgLightColor[4]
    DFace f;
    f.pnum = in.readUInt16();
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    f.fstedge = in.readInt32();
    f.numedge = in.readInt16();
    f.texinfo = in.readInt16();
    f.dispInfo = in.readInt16();
    f.surfaceFogVolumeID = in.readUInt16();
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.firstPrimID = in.readUInt16();
    f.numPrims = in.readUInt16();
    f.smoothingGroups = in.readInt32();
    return f;
}

DFace readDFaceVTMB(BinarySpanReader& in) {
    in.seek(32); // avgLightColor[8]
    DFace f;
    f.pnum = in.readUInt16();
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    f.fstedge = in.readInt32();
    f.numedge = in.readInt16();
    f.texinfo = in.readInt16();
    f.dispInfo = in.readInt16();
    f.surfaceFogVolumeID = in.readUInt16();
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    in.seek(4 + 8 + 8); // remaining styles[4], day[8], night[8]
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.smoothingGroups = in.readInt32();
    return f;
}

DFace readDFaceVin(BinarySpanReader& in, bool v2) {
    DFace f;
    f.pnum = in.readInt32();
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    in.seek(2); // unknown1
    f.fstedge = in.readInt32();
    f.numedge = static_cast<std::int16_t>(in.readInt32());
    f.texinfo = static_cast<std::int16_t>(in.readInt32());
    f.dispInfo = static_cast<std::int16_t>(in.readInt32());
    f.surfaceFogVolumeID = in.readInt32();
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    if (v2) {
        in.seek(4); // unknown2
    }
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.firstPrimID = in.readInt32();
    f.numPrims = in.readInt32();
    f.smoothingGroups = in.readInt32();
    return f;
}

DFace readDFaceStrataV2(BinarySpanReader& in) {
    DFace f;
    f.pnum = static_cast<int>(in.readUInt32());
    f.side = in.readUInt8();
    f.onnode = in.readUInt8();
    in.seek(2); // padding
    f.fstedge = in.readInt32();
    f.numedge = in.readInt32();
    f.texinfo = in.readInt32();
    f.dispInfo = in.readInt32();
    f.surfaceFogVolumeID = static_cast<int>(in.readUInt32());
    for (int i = 0; i < 4; ++i) {
        f.styles[i] = in.readUInt8();
    }
    f.lightofs = in.readInt32();
    f.area = in.readFloat();
    f.LightmapTextureMinsInLuxels[0] = in.readInt32();
    f.LightmapTextureMinsInLuxels[1] = in.readInt32();
    f.LightmapTextureSizeInLuxels[0] = in.readInt32();
    f.LightmapTextureSizeInLuxels[1] = in.readInt32();
    f.origFace = in.readInt32();
    f.numPrims = static_cast<int>(in.readUInt32());
    f.firstPrimID = static_cast<int>(in.readUInt32());
    f.smoothingGroups = in.readInt32();
    return f;
}

DModel readDModel(BinarySpanReader& in, bool darkMessiah) {
    DModel m;
    m.mins = in.readVector3f();
    m.maxs = in.readVector3f();
    m.origin = in.readVector3f();
    if (darkMessiah) {
        in.seek(4); // unknown
    }
    m.headnode = in.readInt32();
    m.fstface = in.readInt32();
    m.numface = in.readInt32();
    return m;
}

DCubemapSample readDCubemapSample(BinarySpanReader& in) {
    DCubemapSample c;
    c.origin[0] = in.readInt32();
    c.origin[1] = in.readInt32();
    c.origin[2] = in.readInt32();
    c.size = static_cast<std::int8_t>(in.readInt32());
    return c;
}

DDispInfo readDDispInfoDefault(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.dispTriStart = in.readInt32();
    d.power = in.readInt32();
    d.minTess = in.readInt32();
    d.smoothingAngle = in.readFloat();
    d.contents = in.readInt32();
    d.mapFace = in.readUInt16();
    in.seek(2); // padding
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(88); // neighbors
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispInfo readDDispInfoBSP17(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.power = in.readInt32();
    d.minTess = in.readInt32();
    d.smoothingAngle = in.readFloat();
    d.contents = in.readInt32();
    d.mapFace = in.readUInt16();
    in.seek(2); // padding
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(88); // neighbors
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispInfo readDDispInfoBSP22(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.dispTriStart = in.readInt32();
    d.power = in.readInt32();
    d.minTess = in.readInt32();
    d.smoothingAngle = in.readFloat();
    d.contents = in.readInt32();
    d.mapFace = in.readUInt16();
    in.seek(2); // padding
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(4); // unknown
    in.seek(88); // neighbors
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispInfo readDDispInfoBSP23(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.dispTriStart = in.readInt32();
    d.power = in.readInt32();
    d.minTess = in.readInt32();
    d.smoothingAngle = in.readFloat();
    d.contents = in.readInt32();
    in.seek(4); // unknown1
    d.mapFace = in.readUInt16();
    in.seek(2); // padding
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(4); // unknown2
    in.seek(88); // neighbors
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispInfo readDDispInfoVin(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.dispTriStart = in.readInt32();
    d.power = in.readInt32();
    d.smoothingAngle = in.readFloat();
    in.seek(4); // unknown
    d.contents = in.readInt32();
    d.mapFace = in.readUInt16();
    in.seek(2); // padding
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(144); // neighborsVin
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispInfo readDDispInfoStrataV1(BinarySpanReader& in) {
    DDispInfo d;
    d.startPos = in.readVector3f();
    d.dispVertStart = in.readInt32();
    d.dispTriStart = in.readInt32();
    d.power = in.readInt32();
    d.minTess = in.readInt32();
    d.smoothingAngle = in.readFloat();
    d.contents = in.readInt32();
    d.mapFace = static_cast<int>(in.readUInt32());
    d.lightmapAlphaStart = in.readInt32();
    d.lightmapSamplePositionStart = in.readInt32();
    in.seek(144); // neighbors
    for (int i = 0; i < 10; ++i) {
        d.allowedVerts[i] = in.readInt32();
    }
    return d;
}

DDispVert readDDispVert(BinarySpanReader& in) {
    DDispVert v;
    v.vector = in.readVector3f();
    v.dist = in.readFloat();
    v.alpha = in.readFloat();
    return v;
}

DDispTri readDDispTri(BinarySpanReader& in) {
    DDispTri t;
    t.tags = in.readUInt16();
    return t;
}

DDispMultiBlend readDDispMultiBlend(BinarySpanReader& in) {
    DDispMultiBlend mb;
    mb.multiblend = in.readVector4f();
    mb.alphablend = in.readVector4f();
    for (int i = 0; i < DDispMultiBlend::kMaxMultiBlendChannels; ++i) {
        mb.multiblendcolors[i] = in.readVector3f();
    }
    return mb;
}

DTexInfo readDTexInfo(BinarySpanReader& in, bool darkMessiah) {
    DTexInfo t;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
            t.textureVecsTexels[i][j] = in.readFloat();
        }
    }
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
            t.lightmapVecsLuxels[i][j] = in.readFloat();
        }
    }
    if (darkMessiah) {
        in.seek(24); // unknown
    }
    t.flagsRaw = in.readInt32();
    t.flags = surfaceFlagsFromInt(t.flagsRaw);
    t.texdata = in.readInt32();
    return t;
}

DTexData readDTexData(BinarySpanReader& in) {
    DTexData td;
    td.reflectivity = in.readVector3f();
    td.texname = in.readInt32();
    td.width = in.readInt32();
    td.height = in.readInt32();
    td.viewWidth = in.readInt32();
    td.viewHeight = in.readInt32();
    return td;
}

DNode readDNodeDefault(BinarySpanReader& in) {
    DNode n;
    n.planenum = in.readInt32();
    n.children[0] = in.readInt32();
    n.children[1] = in.readInt32();
    const auto minX = static_cast<float>(in.readInt16());
    const auto minY = static_cast<float>(in.readInt16());
    const auto minZ = static_cast<float>(in.readInt16());
    n.mins = Vector3f{minX, minY, minZ};
    const auto maxX = static_cast<float>(in.readInt16());
    const auto maxY = static_cast<float>(in.readInt16());
    const auto maxZ = static_cast<float>(in.readInt16());
    n.maxs = Vector3f{maxX, maxY, maxZ};
    n.fstface = in.readUInt16();
    n.numface = in.readUInt16();
    n.area = in.readInt16();
    in.seek(2); // padding
    return n;
}

DNode readDNodeVin(BinarySpanReader& in) {
    DNode n;
    n.planenum = in.readInt32();
    n.children[0] = in.readInt32();
    n.children[1] = in.readInt32();
    const auto minX = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto minY = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto minZ = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    n.mins = Vector3f{minX, minY, minZ};
    const auto maxX = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto maxY = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto maxZ = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    n.maxs = Vector3f{maxX, maxY, maxZ};
    n.fstface = in.readInt32();
    n.numface = in.readInt32();
    in.seek(4); // padding
    return n;
}

DNode readDNodeStrataV1(BinarySpanReader& in) {
    DNode n;
    n.planenum = in.readInt32();
    n.children[0] = in.readInt32();
    n.children[1] = in.readInt32();
    n.mins = in.readVector3f();
    n.maxs = in.readVector3f();
    n.fstface = static_cast<int>(in.readUInt32());
    n.numface = static_cast<int>(in.readUInt32());
    n.area = in.readInt16();
    in.seek(2); // padding
    return n;
}

DLeaf readDLeafBase(BinarySpanReader& in) {
    DLeaf l;
    l.contents = in.readInt32();
    l.cluster = in.readInt16();
    l.areaFlags = in.readInt16();
    const auto minX = static_cast<float>(in.readInt16());
    const auto minY = static_cast<float>(in.readInt16());
    const auto minZ = static_cast<float>(in.readInt16());
    l.mins = Vector3f{minX, minY, minZ};
    const auto maxX = static_cast<float>(in.readInt16());
    const auto maxY = static_cast<float>(in.readInt16());
    const auto maxZ = static_cast<float>(in.readInt16());
    l.maxs = Vector3f{maxX, maxY, maxZ};
    l.fstleafface = in.readUInt16();
    l.numleafface = in.readUInt16();
    l.fstleafbrush = in.readUInt16();
    l.numleafbrush = in.readUInt16();
    l.leafWaterDataID = in.readInt16();
    return l;
}

DLeaf readDLeafV0(BinarySpanReader& in) {
    DLeaf l = readDLeafBase(in);
    in.seek(24 + 2); // ambientLighting[24] + padding[2]
    return l;
}

DLeaf readDLeafV1(BinarySpanReader& in) {
    DLeaf l = readDLeafBase(in);
    in.seek(2); // padding
    return l;
}

DLeaf readDLeafVin(BinarySpanReader& in) {
    DLeaf l;
    l.contents = in.readInt32();
    l.cluster = static_cast<std::int16_t>(in.readInt32());
    l.areaFlags = static_cast<std::int16_t>(in.readInt32());
    const auto minX = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto minY = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto minZ = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    l.mins = Vector3f{minX, minY, minZ};
    const auto maxX = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto maxY = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    const auto maxZ = static_cast<float>(static_cast<std::int16_t>(in.readInt32()));
    l.maxs = Vector3f{maxX, maxY, maxZ};
    l.fstleafface = in.readInt32();
    l.numleafface = in.readInt32();
    l.fstleafbrush = in.readInt32();
    l.numleafbrush = in.readInt32();
    l.leafWaterDataID = static_cast<std::int16_t>(in.readInt32());
    return l;
}

DLeaf readDLeafStrataV2(BinarySpanReader& in) {
    DLeaf l;
    l.contents = in.readInt32();
    l.cluster = in.readInt32();
    l.areaFlags = in.readInt32();
    l.mins = in.readVector3f();
    l.maxs = in.readVector3f();
    l.fstleafface = static_cast<int>(in.readUInt32());
    l.numleafface = static_cast<int>(in.readUInt32());
    l.fstleafbrush = static_cast<int>(in.readUInt32());
    l.numleafbrush = static_cast<int>(in.readUInt32());
    l.leafWaterDataID = in.readInt32();
    return l;
}

DOverlay readDOverlayTail(BinarySpanReader& in, DOverlay o) {
    for (int i = 0; i < 64; ++i) {
        o.ofaces[i] = in.readInt32();
    }
    o.u[0] = in.readFloat();
    o.u[1] = in.readFloat();
    o.v[0] = in.readFloat();
    o.v[1] = in.readFloat();
    for (int i = 0; i < 4; ++i) {
        o.uvpoints[i] = in.readVector3f();
    }
    o.origin = in.readVector3f();
    o.basisNormal = in.readVector3f();
    return o;
}

DOverlay readDOverlayDefault(BinarySpanReader& in) {
    DOverlay o;
    o.id = in.readInt32();
    o.texinfo = in.readInt16();
    o.faceCountAndRenderOrder = in.readUInt16();
    return readDOverlayTail(in, std::move(o));
}

DOverlay readDOverlayDota2(BinarySpanReader& in) {
    DOverlay o = readDOverlayDefault(in);
    in.seek(4); // unknown
    return o;
}

DOverlay readDOverlayVin(BinarySpanReader& in) {
    DOverlay o;
    o.id = in.readInt32();
    o.texinfo = static_cast<std::int16_t>(in.readInt32());
    o.faceCountAndRenderOrder = in.readInt32();
    return readDOverlayTail(in, std::move(o));
}

DOverlay readDOverlayStrataV1(BinarySpanReader& in) {
    DOverlay o;
    o.id = in.readInt32();
    o.texinfo = in.readInt32();
    o.faceCountAndRenderOrder = in.readUInt16();
    in.seek(2); // padding
    return readDOverlayTail(in, std::move(o));
}

DOverlayFade readDOverlayFade(BinarySpanReader& in) {
    DOverlayFade f;
    f.fadeDistMinSq = in.readFloat();
    f.fadeDistMaxSq = in.readFloat();
    return f;
}

DOverlaySystemLevel readDOverlaySystemLevel(BinarySpanReader& in) {
    DOverlaySystemLevel s;
    s.minCPULevel = in.readUInt8();
    s.maxCPULevel = in.readUInt8();
    s.minGPULevel = in.readUInt8();
    s.maxGPULevel = in.readUInt8();
    return s;
}

DAreaportal readDAreaportalDefault(BinarySpanReader& in) {
    DAreaportal a;
    a.portalKey = in.readInt16();
    a.otherportal = in.readInt16();
    a.firstClipPortalVert = in.readInt16();
    a.clipPortalVerts = in.readInt16();
    a.planenum = in.readInt32();
    return a;
}

DAreaportal readDAreaportalVin(BinarySpanReader& in) {
    DAreaportal a;
    a.portalKey = static_cast<std::int16_t>(in.readInt32());
    a.otherportal = static_cast<std::int16_t>(in.readInt32());
    a.firstClipPortalVert = static_cast<std::int16_t>(in.readInt32());
    a.clipPortalVerts = static_cast<std::int16_t>(in.readInt32());
    a.planenum = in.readInt32();
    return a;
}

DAreaportal readDAreaportalStrataV1(BinarySpanReader& in) {
    DAreaportal a;
    a.portalKey = in.readInt32();
    a.otherportal = in.readInt32();
    a.firstClipPortalVert = in.readInt32();
    a.clipPortalVerts = in.readInt32();
    a.planenum = in.readInt32();
    return a;
}

DPrimitive readDPrimitiveDefault(BinarySpanReader& in) {
    DPrimitive p;
    p.type = in.readUInt16();
    p.firstIndex = in.readUInt16();
    p.indexCount = in.readUInt16();
    p.firstVert = in.readUInt16();
    p.vertCount = in.readUInt16();
    return p;
}

DPrimitive readDPrimitiveStrataV1(BinarySpanReader& in) {
    DPrimitive p;
    p.type = in.readUInt8();
    in.seek(3); // padding
    p.firstIndex = static_cast<int>(in.readUInt32());
    p.indexCount = static_cast<int>(in.readUInt32());
    p.firstVert = static_cast<int>(in.readUInt32());
    p.vertCount = static_cast<int>(in.readUInt32());
    return p;
}

enum class StaticPropKind : int {
    V4 = 0,
    V5,
    V6,
    V8,
    V9,
    V10,
    V11Lite,
    V11,
    V13,
    V5Ship,
    V6BGT,
    V7ZC,
    V6DM,
    V9DE,
    V6VIN,
    V7VIN,
    V8VIN,
    V7L4D,
    V10CSGO,
    V11CSGO,
    V4Padded,
};

struct StaticPropDescriptor {
    StaticPropKind kind;
    const char* name;
    int version;
    int appId;
    int size;
};

constexpr std::array<StaticPropDescriptor, 25> kStaticPropDescriptors{{
    {StaticPropKind::V4,      "DStaticPropV4",      4,  SourceAppId::kDefault,            56},
    {StaticPropKind::V5,      "DStaticPropV5",      5,  SourceAppId::kDefault,            60},
    {StaticPropKind::V6,      "DStaticPropV6",      6,  SourceAppId::kDefault,            64},
    {StaticPropKind::V8,      "DStaticPropV8",      8,  SourceAppId::kDefault,            68},
    {StaticPropKind::V9,      "DStaticPropV9",      9,  SourceAppId::kDefault,            72},
    {StaticPropKind::V10,     "DStaticPropV10",     10, SourceAppId::kDefault,            72},
    {StaticPropKind::V11Lite, "DStaticPropV11lite", 11, SourceAppId::kDefault,            76},
    {StaticPropKind::V11,     "DStaticPropV11",     11, SourceAppId::kDefault,            80},
    {StaticPropKind::V11,     "DStaticPropV11",     12, SourceAppId::kDefault,            80},
    {StaticPropKind::V13,     "DStaticPropV13",     13, SourceAppId::kDefault,            88},
    {StaticPropKind::V5Ship,  "DStaticPropV5Ship",  5,  SourceAppId::kTheShip,            188},
    {StaticPropKind::V6BGT,   "DStaticPropV6BGT",   6,  SourceAppId::kBloodyGoodTime,     192},
    {StaticPropKind::V7ZC,    "DStaticPropV7ZC",    7,  SourceAppId::kZenoClash,          68},
    {StaticPropKind::V6DM,    "DStaticPropV6DM",    6,  SourceAppId::kDarkMessiah,        136},
    {StaticPropKind::V9DE,    "DStaticPropV9DE",    9,  SourceAppId::kDearEsther,         76},
    {StaticPropKind::V6VIN,   "DStaticPropV6VIN",   6,  SourceAppId::kVindictus,          60},
    {StaticPropKind::V7VIN,   "DStaticPropV7VIN",   7,  SourceAppId::kVindictus,          64},
    {StaticPropKind::V8VIN,   "DStaticPropV8VIN",   8,  SourceAppId::kVindictus,          72},
    {StaticPropKind::V7L4D,   "DStaticPropV7L4D",   7,  SourceAppId::kLeft4Dead,          68},
    {StaticPropKind::V10,     "DStaticPropV10",     7,  SourceAppId::kTeamFortress2,      72},
    {StaticPropKind::V10CSGO, "DStaticPropV10CSGO", 10, SourceAppId::kCounterStrikeGo,    76},
    {StaticPropKind::V11CSGO, "DStaticPropV11CSGO", 11, SourceAppId::kCounterStrikeGo,    80},
    {StaticPropKind::V10CSGO, "DStaticPropV10CSGO", 10, SourceAppId::kInsurgency,         76},
    {StaticPropKind::V10CSGO, "DStaticPropV10CSGO", 10, SourceAppId::kBladeSymphony,      76},
    {StaticPropKind::V11CSGO, "DStaticPropV11CSGO", 11, SourceAppId::kBladeSymphony,      80},
}};

void readStaticPropV4(BinarySpanReader& in, DStaticProp& sp) {
    sp.origin = in.readVector3f();
    sp.angles = in.readVector3f();
    sp.propType = in.readUInt16();
    sp.firstLeaf = in.readUInt16();
    sp.leafCount = in.readUInt16();
    sp.solid = in.readUInt8();
    sp.flagsRaw = in.readUInt8();
    sp.skin = in.readInt32();
    sp.fademin = in.readFloat();
    sp.fademax = in.readFloat();
    sp.lightingOrigin = in.readVector3f();
}

void readStaticPropV5(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV4(in, sp);
    sp.hasV5 = true;
    sp.forcedFadeScale = in.readFloat();
}

void readStaticPropV6(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV5(in, sp);
    sp.hasV6 = true;
    sp.minDXLevel = in.readUInt16();
    sp.maxDXLevel = in.readUInt16();
}

void readStaticPropV8(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV5(in, sp);
    sp.hasV8 = true;
    sp.minCPULevel = in.readInt8();
    sp.maxCPULevel = in.readInt8();
    sp.minGPULevel = in.readInt8();
    sp.maxGPULevel = in.readInt8();
    sp.hasDiffuseModulation = true;
    sp.diffuseModulation = Color32(in.readInt32());
}

void readStaticPropV9(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV8(in, sp);
    sp.hasV9 = true;
    sp.disableX360 = (in.readUInt8() != 0);
    in.seek(3); // unknown
}

void readStaticPropV10(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV6(in, sp);
    sp.hasV10 = true;
    sp.flagsRaw = in.readUInt32();
    sp.lightmapResolutionX = in.readUInt16();
    sp.lightmapResolutionY = in.readUInt16();
}

void readStaticPropV10CSGO(BinarySpanReader& in, DStaticProp& sp) {
    readStaticPropV9(in, sp);
    sp.hasV10Csgo = true;
    sp.flagsExRaw = in.readUInt32();
}

DStaticProp readStaticPropByKind(BinarySpanReader& in, StaticPropKind kind, int structSize) {
    DStaticProp sp;
    switch (kind) {
    case StaticPropKind::V4:
        readStaticPropV4(in, sp);
        break;
    case StaticPropKind::V5:
    case StaticPropKind::V6VIN:
        readStaticPropV5(in, sp);
        break;
    case StaticPropKind::V6:
    case StaticPropKind::V7VIN:
        readStaticPropV6(in, sp);
        break;
    case StaticPropKind::V8:
        readStaticPropV8(in, sp);
        break;
    case StaticPropKind::V9:
        readStaticPropV9(in, sp);
        break;
    case StaticPropKind::V10:
    case StaticPropKind::V8VIN:
        readStaticPropV10(in, sp);
        break;
    case StaticPropKind::V11Lite:
        readStaticPropV10(in, sp);
        sp.hasDiffuseModulation = true;
        sp.isV11Lite = true;
        sp.diffuseModulation = Color32(in.readInt32());
        break;
    case StaticPropKind::V11:
        readStaticPropV10(in, sp);
        sp.hasDiffuseModulation = true;
        sp.isV11Lite = true;
        sp.diffuseModulation = Color32(in.readInt32());
        sp.flagsExRaw = in.readUInt32();
        break;
    case StaticPropKind::V13:
        readStaticPropV10CSGO(in, sp);
        sp.hasV13 = true;
        sp.scale = in.readVector3f();
        break;
    case StaticPropKind::V5Ship:
        readStaticPropV5(in, sp);
        sp.hasTargetName = true;
        sp.targetname = in.readStringFixed(128);
        break;
    case StaticPropKind::V6BGT:
        readStaticPropV6(in, sp);
        sp.hasTargetName = true;
        sp.targetname = in.readStringFixed(128);
        break;
    case StaticPropKind::V7ZC:
        readStaticPropV6(in, sp);
        in.seek(4); // unknown
        break;
    case StaticPropKind::V6DM:
        readStaticPropV6(in, sp);
        in.seek(72); // unknown
        break;
    case StaticPropKind::V9DE:
        sp.origin = in.readVector3f();
        sp.angles = in.readVector3f();
        sp.propType = in.readUInt16();
        sp.firstLeaf = in.readUInt16();
        sp.leafCount = in.readUInt16();
        sp.solid = in.readUInt8();
        sp.flagsRaw = in.readUInt8();
        in.seek(4);
        sp.skin = in.readInt32();
        sp.fademin = in.readFloat();
        sp.fademax = in.readFloat();
        in.seek(12); // invalid lighting origin
        sp.hasV5 = true;
        sp.forcedFadeScale = in.readFloat();
        sp.hasV8 = true;
        sp.minCPULevel = in.readInt8();
        sp.maxCPULevel = in.readInt8();
        sp.minGPULevel = in.readInt8();
        sp.maxGPULevel = in.readInt8();
        in.seek(1);
        sp.hasDiffuseModulation = true;
        sp.diffuseModulation = Color32(in.readInt32());
        in.seek(3);
        sp.disableLightingOrigin = true;
        break;
    case StaticPropKind::V7L4D:
        readStaticPropV6(in, sp);
        sp.hasDiffuseModulation = true;
        sp.diffuseModulation = Color32(in.readInt32());
        break;
    case StaticPropKind::V10CSGO:
        readStaticPropV10CSGO(in, sp);
        break;
    case StaticPropKind::V11CSGO:
        readStaticPropV10CSGO(in, sp);
        sp.hasV11Csgo = true;
        sp.uniformScale = in.readFloat();
        break;
    case StaticPropKind::V4Padded:
        readStaticPropV4(in, sp);
        if (structSize > 56) {
            in.seek(structSize - 56);
        }
        break;
    }
    return sp;
}

BspEntity buildEntity(const std::vector<EntityKeyValue>& kvList, const BspDecompileConfig& config) {
    BspEntity entity;
    QString className;
    bool hasClassName = false;

    for (const auto& kv : kvList) {
        if (kv.key == QLatin1StringView("classname")) {
            if (!hasClassName) {
                className = kv.value;
                hasClassName = true;
            } else {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Found duplicate classname key, ignoring \"%1\"").arg(kv.value));
            }
            continue;
        }

        if (BspEntity::isEntityIoValue(kv.value)) {
            entity.addIo(kv.key, kv.value);
        } else {
            entity.setValue(kv.key, kv.value);
        }
    }

    if (!hasClassName || className.isEmpty()) {
        config.log(BspLogLevel::Warning,
            QStringLiteral("Missing or empty class name, using \"unknown_entity\""));
        className = QStringLiteral("unknown_entity");
    }

    entity.setClassName(className);
    return entity;
}

void writeInt32LE(QByteArray& out, std::int32_t value) {
    const auto u = static_cast<std::uint32_t>(value);
    const char bytes[4]{
        static_cast<char>(u & 0xFFU),
        static_cast<char>((u >> 8) & 0xFFU),
        static_cast<char>((u >> 16) & 0xFFU),
        static_cast<char>((u >> 24) & 0xFFU),
    };
    out.append(bytes, 4);
}

void writeStringPrefixedU8(QByteArray& out, const QString& str) {
    const QByteArray utf8 = str.toUtf8();
    const std::uint8_t len = static_cast<std::uint8_t>(std::min<qsizetype>(utf8.size(), 255));
    out.append(static_cast<char>(len));
    if (len > 0) {
        out.append(utf8.constData(), static_cast<qsizetype>(len));
    }
}

} // namespace

Core::Result<BspFile> BspFile::open(
    const Core::Path::FilesystemPath& file,
    int defaultAppId,
    bool loadExternalLumpFiles,
    const BspDecompileConfig& config)
{
    if (config.token.isCancelled()) {
        return Core::Result<BspFile>::cancelled(
            QCoreApplication::translate("BspFile", "BSP loading cancelled"));
    }
    if (file.isEmpty() || !file.isValid()) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspFile", "BSP file path is empty or invalid"));
    }
    if (!file.exists()) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("BspFile", "BSP file not found"),
            file.toString());
    }

    auto bytesRes = readAllFileBytes(file);
    if (bytesRes.isFailure()) {
        return Core::Result<BspFile>::failure(bytesRes.error());
    }

    std::vector<std::byte> fileBytes = std::move(bytesRes.value());
    if (fileBytes.size() < 4) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspFile", "Invalid or missing BSP header"),
            file.toString());
    }

    const std::uint32_t identBE = readU32BE(fileBytes.data());
    if (identBE == kZipLocalSigBE || identBE == kZipEocdSigBE || identBE == kZipSpannedSigBE) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspFile", "Loaded file is a ZIP archive, not a BSP file"),
            file.toString());
    }

    if (fileBytes.size() < static_cast<std::size_t>(kHeaderSize)) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspFile", "BSP file is shorter than minimum header size"),
            file.toString());
    }

    BspFile bsp;
    bsp.m_file = file;
    bsp.m_name = nameWithoutExtension(file);
    bsp.m_appId = defaultAppId;

    const std::uint32_t identLE = readU32LE(fileBytes.data());
    if (identBE == kBspIdLE) {
        // File starts with 'P','S','B','V' -> big-endian VBSP
        bsp.m_bigEndian = true;
    } else if (identLE == kBspIdLE) {
        // File starts with 'V','B','S','P' -> little-endian VBSP
        bsp.m_bigEndian = false;
    } else if (identLE == kBspIdTfLE) {
        // File starts with 'r','B','S','P' -> Titanfall little-endian rBSP
        bsp.m_appId = SourceAppId::kTitanfall;
        bsp.m_bigEndian = false;
    } else if (identLE == kGoldSrcVersion) {
        return Core::Result<BspFile>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("BspFile", "The GoldSrc BSP format is not supported"),
            file.toString());
    } else {
        // Check Tactical Intervention 32-byte XOR encryption at offset 384
        const std::array<std::uint8_t, 4> unxored{
            static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(fileBytes[0]) ^ std::to_integer<std::uint8_t>(fileBytes[kXorKeyOffset + 0])),
            static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(fileBytes[1]) ^ std::to_integer<std::uint8_t>(fileBytes[kXorKeyOffset + 1])),
            static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(fileBytes[2]) ^ std::to_integer<std::uint8_t>(fileBytes[kXorKeyOffset + 2])),
            static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(fileBytes[3]) ^ std::to_integer<std::uint8_t>(fileBytes[kXorKeyOffset + 3])),
        };
        if (unxored[0] == 'V' && unxored[1] == 'B' && unxored[2] == 'S' && unxored[3] == 'P') {
            std::array<std::byte, kXorKeyLength> key{};
            std::copy_n(fileBytes.begin() + static_cast<std::ptrdiff_t>(kXorKeyOffset), kXorKeyLength, key.begin());
            for (std::size_t i = 0; i < fileBytes.size(); ++i) {
                fileBytes[i] = static_cast<std::byte>(
                    std::to_integer<std::uint8_t>(fileBytes[i]) ^ std::to_integer<std::uint8_t>(key[i % kXorKeyLength]));
            }
            bsp.m_bigEndian = false;
            config.log(BspLogLevel::Debug, QStringLiteral("Decrypted Tactical Intervention XOR BSP"));
        } else {
            return Core::Result<BspFile>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("BspFile", "Unknown BSP file signature"),
                file.toString());
        }
    }

    return Core::Error::ExecutionGuard::guard<BspFile>(
        [&]() -> Core::Result<BspFile> {
            BinarySpanReader reader(fileBytes, bsp.m_bigEndian);
            reader.setPosition(4);

            bsp.m_version = reader.readInt32();
            if (bsp.m_version == 0x40014) {
                bsp.m_appId = SourceAppId::kDarkMessiah;
                bsp.m_version &= 0xff;
            } else if (bsp.m_version == 27) {
                bsp.m_appId = SourceAppId::kContagion;
            }

            if (bsp.m_version == 21 && peekInt32At(fileBytes, 8, bsp.m_bigEndian) == 0) {
                bsp.m_appId = SourceAppId::kLeft4Dead2;
            }

            if (bsp.m_appId == SourceAppId::kContagion) {
                reader.readInt32(); // extra int
            }

            if (bsp.m_appId == SourceAppId::kTitanfall) {
                bsp.m_mapRev = reader.readInt32();
                reader.readInt32(); // always 127?
            }

            std::size_t cursor = reader.position();
            bsp.loadLumps(fileBytes, cursor, config);
            bsp.loadGameLumps(config);

            if (bsp.m_appId == SourceAppId::kTitanfall) {
                bsp.loadTitanfallLumpFiles(config);
                bsp.loadTitanfallEntityFiles(config);
            } else {
                reader.setPosition(cursor);
                bsp.m_mapRev = reader.readInt32();
            }

            if (loadExternalLumpFiles) {
                bsp.loadLumpFiles(config);
            }

            return Core::Result<BspFile>::success(std::move(bsp));
        },
        Core::Error::ExecutionContext{
            .stage = QStringLiteral("OpenBSP"),
            .targetPath = file.toString(),
        });
}

void BspFile::loadLumps(
    const std::vector<std::byte>& fileBytes,
    std::size_t& cursor,
    const BspDecompileConfig& config)
{
    BinarySpanReader reader(fileBytes, m_bigEndian);
    reader.setPosition(cursor);

    const int numLumps = (m_appId == SourceAppId::kTitanfall) ? kHeaderLumpsTf : kHeaderLumps;
    m_lumps.clear();
    m_lumps.reserve(static_cast<std::size_t>(numLumps));

    const std::size_t fileSize = fileBytes.size();
    for (int i = 0; i < numLumps; ++i) {
        int vers = 0;
        int ofs = 0;
        int len = 0;
        if (m_appId == SourceAppId::kLeft4Dead2) {
            vers = reader.readInt32();
            ofs = reader.readInt32();
            len = reader.readInt32();
        } else {
            ofs = reader.readInt32();
            len = reader.readInt32();
            vers = reader.readInt32();
        }
        const int fourCC = reader.readInt32();

        const LumpType ltype = lumpTypeForIndex(i, m_version);

        if (ofs < 0) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Negative lump offset %1 in lump %2, assuming 0").arg(ofs).arg(i));
            ofs = 0;
            len = 0;
        } else if (static_cast<std::size_t>(ofs) > fileSize) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Invalid lump offset %1 in lump %2, clamping").arg(ofs).arg(i));
            ofs = static_cast<int>(fileSize);
            len = 0;
        }

        if (len < 0) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Negative lump length %1 in lump %2, assuming 0").arg(len).arg(i));
            len = 0;
        } else if (static_cast<std::size_t>(ofs) + static_cast<std::size_t>(len) > fileSize) {
            const int clamped = static_cast<int>(fileSize - static_cast<std::size_t>(ofs));
            config.log(BspLogLevel::Warning,
                QStringLiteral("Invalid lump length %1 in lump %2, assuming %3").arg(len).arg(i).arg(clamped));
            len = clamped;
        }

        BspLumpBuffer lump;
        lump.index = i;
        lump.type = ltype;
        lump.offset = ofs;
        lump.length = len;
        lump.version = vers;
        lump.fourCC = fourCC;
        if (len > 0) {
            const auto beginIt = fileBytes.begin() + static_cast<std::ptrdiff_t>(ofs);
            lump.data.assign(beginIt, beginIt + static_cast<std::ptrdiff_t>(len));
        }
        m_lumps.push_back(std::move(lump));
    }

    cursor = reader.position();
}

void BspFile::loadGameLumps(const BspDecompileConfig& config) {
    const BspLumpBuffer* gameLump = getLump(LumpType::LUMP_GAME_LUMP);
    if (!gameLump || gameLump->data.size() < 4) {
        return;
    }

    try {
        BinarySpanReader in(gameLump->data, m_bigEndian);

        if (m_version == 20 && !m_bigEndian
            && checkInvalidGameLumpHeaders(in, false)
            && !checkInvalidGameLumpHeaders(in, true)) {
            m_appId = SourceAppId::kVindictus;
        }

        const int glumps = in.readInt32();
        if (glumps < 0) {
            return;
        }

        const std::size_t lumpLen = gameLump->data.size();
        m_gameLumps.reserve(static_cast<std::size_t>(glumps));

        for (int i = 0; i < glumps; ++i) {
            if (m_appId == SourceAppId::kDarkMessiah) {
                in.readInt32(); // unknown
            }

            const int fourCC = in.readInt32();
            int flags = 0;
            int vers = 0;
            if (m_appId == SourceAppId::kVindictus) {
                flags = in.readInt32();
                vers = in.readInt32();
            } else {
                flags = in.readUInt16();
                vers = in.readUInt16();
            }

            int ofs = in.readInt32();
            int len = in.readInt32();

            if (flags == 1) {
                const std::size_t savedPos = in.position();
                int nextOfs = 0;
                const std::size_t skipToNextOfs = (m_appId == SourceAppId::kVindictus) ? 12U : 8U;
                if (in.remaining() >= skipToNextOfs + 4U) {
                    in.seek(static_cast<std::ptrdiff_t>(skipToNextOfs));
                    nextOfs = in.readInt32();
                }
                in.setPosition(savedPos);
                if (nextOfs == 0) {
                    nextOfs = gameLump->offset + static_cast<int>(lumpLen);
                }
                len = nextOfs - ofs;
            }

            if (ofs - gameLump->offset > 0) {
                ofs -= gameLump->offset;
            }

            QString glName = fourCCToString(fourCC);
            if (glName.trimmed().isEmpty() || fourCC == 0) {
                glName = QStringLiteral("<dummy>");
            }

            if (ofs < 0) {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Negative game lump offset %1 in %2, assuming 0").arg(ofs).arg(glName));
                ofs = 0;
                len = 0;
            } else if (static_cast<std::size_t>(ofs) > lumpLen) {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Invalid game lump offset %1 in %2, clamping").arg(ofs).arg(glName));
                ofs = static_cast<int>(lumpLen);
                len = 0;
            }

            if (len < 0) {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Negative game lump length %1 in %2, assuming 0").arg(len).arg(glName));
                len = 0;
            } else if (static_cast<std::size_t>(ofs) + static_cast<std::size_t>(len) > lumpLen) {
                const int clamped = static_cast<int>(lumpLen - static_cast<std::size_t>(ofs));
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Invalid game lump length %1 in %2, assuming %3").arg(len).arg(glName).arg(clamped));
                len = clamped;
            }

            BspGameLumpBuffer gl;
            gl.fourCC = fourCC;
            gl.name = std::move(glName);
            gl.flags = flags;
            gl.version = vers;
            gl.offset = ofs;
            gl.length = len;
            if (len > 0) {
                const auto beginIt = gameLump->data.begin() + static_cast<std::ptrdiff_t>(ofs);
                gl.data.assign(beginIt, beginIt + static_cast<std::ptrdiff_t>(len));
            }
            m_gameLumps.push_back(std::move(gl));
        }
    } catch (const std::exception& ex) {
        config.log(BspLogLevel::Warning,
            QStringLiteral("Couldn't load game lumps: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void BspFile::loadLumpFiles(const BspDecompileConfig& config) {
    const Core::Path::FilesystemPath parentDir = m_file.parentPath();
    if (parentDir.isEmpty()) {
        return;
    }

    for (int i = 0; i < kMaxLumpFiles; ++i) {
        const QString fileName = QStringLiteral("%1_l_%2.lmp").arg(m_name).arg(i);
        const Core::Path::FilesystemPath lumpFile = parentDir / fileName;
        if (!lumpFile.exists()) {
            break;
        }

        auto bytesRes = readAllFileBytes(lumpFile);
        if (bytesRes.isFailure()) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Unable to load lump file %1: %2").arg(fileName, bytesRes.message()));
            continue;
        }

        const std::vector<std::byte>& lmpBytes = bytesRes.value();
        if (lmpBytes.size() < static_cast<std::size_t>(kLumpFileHeaderSize)) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Invalid or missing lump header in %1").arg(fileName));
            continue;
        }

        try {
            BinarySpanReader in(lmpBytes, m_bigEndian);
            int lumpOffset = in.readInt32();
            const int lumpIndex = in.readInt32();
            const int lumpVersion = in.readInt32();
            int lumpSize = in.readInt32();
            (void)in.readInt32(); // mapRev

            if (lumpIndex < 0 || static_cast<std::size_t>(lumpIndex) >= m_lumps.size()) {
                config.log(BspLogLevel::Warning,
                    QStringLiteral("Invalid lump ID %1 in %2").arg(lumpIndex).arg(fileName));
                continue;
            }

            const std::size_t limit = lmpBytes.size();
            if (lumpOffset < 0 || static_cast<std::size_t>(lumpOffset) > limit) {
                lumpOffset = kLumpFileHeaderSize;
            }
            if (lumpSize < 0 || static_cast<std::size_t>(lumpOffset) + static_cast<std::size_t>(lumpSize) > limit) {
                lumpSize = static_cast<int>(limit - static_cast<std::size_t>(lumpOffset));
            }

            BspLumpBuffer& target = m_lumps[static_cast<std::size_t>(lumpIndex)];
            target.offset = lumpOffset;
            target.length = lumpSize;
            target.version = lumpVersion;
            target.data.clear();
            if (lumpSize > 0) {
                const auto beginIt = lmpBytes.begin() + static_cast<std::ptrdiff_t>(lumpOffset);
                target.data.assign(beginIt, beginIt + static_cast<std::ptrdiff_t>(lumpSize));
            }

            if (target.type == LumpType::LUMP_GAME_LUMP) {
                m_gameLumps.clear();
                loadGameLumps(config);
            }
        } catch (const std::exception& ex) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Unable to parse lump file %1: %2").arg(fileName, QString::fromUtf8(ex.what())));
        }
    }
}

void BspFile::loadTitanfallLumpFiles(const BspDecompileConfig& config) {
    const Core::Path::FilesystemPath parentDir = m_file.parentPath();
    if (parentDir.isEmpty()) {
        return;
    }

    for (int i = 0; i < kHeaderLumpsTf && static_cast<std::size_t>(i) < m_lumps.size(); ++i) {
        const QString fileName = QStringLiteral("%1.bsp.%2.bsp_lump")
            .arg(m_name)
            .arg(i, 4, 16, QLatin1Char('0'));
        const Core::Path::FilesystemPath lumpFile = parentDir / fileName;
        if (!lumpFile.exists()) {
            continue;
        }

        auto bytesRes = readAllFileBytes(lumpFile);
        if (bytesRes.isFailure()) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Unable to load Titanfall lump file %1").arg(fileName));
            continue;
        }

        BspLumpBuffer& lump = m_lumps[static_cast<std::size_t>(i)];
        lump.data = std::move(bytesRes.value());
        lump.length = static_cast<int>(lump.data.size());
    }
}

void BspFile::loadTitanfallEntityFiles(const BspDecompileConfig& config) {
    if (m_lumps.empty()) {
        return;
    }

    const Core::Path::FilesystemPath parentDir = m_file.parentPath();
    if (parentDir.isEmpty()) {
        return;
    }

    BspLumpBuffer& entLump = m_lumps[static_cast<std::size_t>(LumpType::LUMP_ENTITIES)];
    std::vector<std::byte> combined;
    if (!entLump.data.empty()) {
        const std::size_t keep = (entLump.data.back() == std::byte{0})
            ? entLump.data.size() - 1
            : entLump.data.size();
        combined.insert(combined.end(), entLump.data.begin(), entLump.data.begin() + static_cast<std::ptrdiff_t>(keep));
    }

    constexpr std::array<const char*, 5> kEntSuffixes{"env", "fx", "script", "snd", "spawn"};
    for (const char* suffix : kEntSuffixes) {
        const QString fileName = QStringLiteral("%1_%2.ent").arg(m_name, QLatin1StringView(suffix));
        const Core::Path::FilesystemPath entFile = parentDir / fileName;
        if (!entFile.exists()) {
            continue;
        }

        auto bytesRes = readAllFileBytes(entFile);
        if (bytesRes.isFailure()) {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Unable to load Titanfall entity file %1").arg(fileName));
            continue;
        }

        const std::vector<std::byte>& entBytes = bytesRes.value();
        // Skip "ENTITIESXX\n" (11 bytes) and trailing "\0" (1 byte)
        if (entBytes.size() > 12) {
            combined.insert(combined.end(), entBytes.begin() + 11, entBytes.end() - 1);
        }
    }

    combined.push_back(std::byte{0});
    entLump.data = std::move(combined);
    entLump.length = static_cast<int>(entLump.data.size());
}

bool BspFile::canReadLump(LumpType type) const noexcept {
    const int idx = static_cast<int>(type);
    if (idx < 0 || static_cast<std::size_t>(idx) >= m_lumps.size()) {
        return false;
    }
    return Domain::Bsp::Internal::canReadLump(type, m_version);
}

const BspLumpBuffer* BspFile::getLump(LumpType type) const noexcept {
    const int idx = static_cast<int>(type);
    if (idx < 0 || static_cast<std::size_t>(idx) >= m_lumps.size()) {
        return nullptr;
    }
    return &m_lumps[static_cast<std::size_t>(idx)];
}

const BspGameLumpBuffer* BspFile::getGameLump(QStringView sid) const noexcept {
    for (const auto& gl : m_gameLumps) {
        if (gl.name.compare(sid, Qt::CaseInsensitive) == 0) {
            return &gl;
        }
    }
    return nullptr;
}

void BspFile::uncompress(const BspDecompileConfig& config) {
    for (auto& lump : m_lumps) {
        if (!Domain::Package::Internal::Lzma1Decoder::hasValveLzmaMagic(lump.data)) {
            continue;
        }
        auto decompRes = Domain::Package::Internal::Lzma1Decoder::decompressValveLump(lump.data);
        if (decompRes.isSuccess()) {
            lump.data = std::move(decompRes.value());
            lump.length = static_cast<int>(lump.data.size());
        } else {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Couldn't uncompress lump %1: %2")
                    .arg(lump.index)
                    .arg(decompRes.message()));
        }
    }

    for (auto& gl : m_gameLumps) {
        if (!Domain::Package::Internal::Lzma1Decoder::hasValveLzmaMagic(gl.data)) {
            continue;
        }
        auto decompRes = Domain::Package::Internal::Lzma1Decoder::decompressValveLump(gl.data);
        if (decompRes.isSuccess()) {
            gl.data = std::move(decompRes.value());
            gl.length = static_cast<int>(gl.data.size());
        } else {
            config.log(BspLogLevel::Warning,
                QStringLiteral("Couldn't uncompress game lump %1: %2")
                    .arg(gl.name, decompRes.message()));
        }
    }
}

BspFileReader::BspFileReader(BspFile& bspFile, const BspDecompileConfig& config)
    : m_bspFile(bspFile)
    , m_config(config)
{
    m_bspFile.uncompress(m_config);
}

void BspFileReader::loadAll() {
    loadEntities();
    loadVertices();
    loadEdges();
    loadFaces();
    loadOriginalFaces();
    loadModels();
    loadSurfaceEdges();
    loadOccluders();
    loadTexInfo();
    loadTexData();
    loadStaticProps();
    loadCubemaps();
    loadPlanes();
    loadBrushes();
    loadBrushSides();
    loadAreaportals();
    loadClipPortalVertices();
    loadDispInfos();
    loadDispVertices();
    loadDispTriangleTags();
    loadDispMultiBlend();
    loadNodes();
    loadLeaves();
    loadLeafFaces();
    loadLeafBrushes();
    loadOverlays();
    loadFlags();
}

void BspFileReader::loadPlanes() {
    if (m_loadedPlanes) {
        return;
    }
    m_loadedPlanes = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_PLANES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_PLANES);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.planes = readStructChunks<DPlane>(*lump, m_bspFile.isBigEndian(), 20, m_config, readDPlane);
}

void BspFileReader::loadBrushes() {
    if (m_loadedBrushes) {
        return;
    }
    m_loadedBrushes = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_BRUSHES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_BRUSHES);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.brushes = readStructChunks<DBrush>(*lump, m_bspFile.isBigEndian(), 12, m_config, readDBrush);
}

void BspFileReader::loadBrushSides() {
    if (m_loadedBrushSides) {
        return;
    }
    m_loadedBrushSides = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_BRUSHSIDES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_BRUSHSIDES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus) {
        m_bspData.brushSides = readStructChunks<DBrushSide>(*lump, m_bspFile.isBigEndian(), 16, m_config, readDBrushSideVin);
    } else if (app == SourceAppId::kStrataSource) {
        m_bspData.brushSides = readStructChunks<DBrushSide>(*lump, m_bspFile.isBigEndian(), 16, m_config, readDBrushSideStrataV1);
    } else if (m_bspFile.version() >= 21 && app != SourceAppId::kLeft4Dead2) {
        m_bspData.brushSides = readStructChunks<DBrushSide>(*lump, m_bspFile.isBigEndian(), 8, m_config, readDBrushSideV0New);
    } else {
        m_bspData.brushSides = readStructChunks<DBrushSide>(*lump, m_bspFile.isBigEndian(), 8, m_config, readDBrushSideDefault);
    }
}

void BspFileReader::loadVertices() {
    if (m_loadedVertices) {
        return;
    }
    m_loadedVertices = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_VERTEXES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_VERTEXES);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.verts = readStructChunks<DVertex>(*lump, m_bspFile.isBigEndian(), 12, m_config, readDVertex);
}

void BspFileReader::loadClipPortalVertices() {
    if (m_loadedClipPortalVertices) {
        return;
    }
    m_loadedClipPortalVertices = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_CLIPPORTALVERTS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_CLIPPORTALVERTS);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.clipPortalVerts = readStructChunks<DVertex>(*lump, m_bspFile.isBigEndian(), 12, m_config, readDVertex);
}

void BspFileReader::loadEdges() {
    if (m_loadedEdges) {
        return;
    }
    m_loadedEdges = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_EDGES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_EDGES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus) {
        m_bspData.edges = readStructChunks<DEdge>(*lump, m_bspFile.isBigEndian(), 8, m_config, readDEdgeVin);
    } else if (app == SourceAppId::kStrataSource) {
        m_bspData.edges = readStructChunks<DEdge>(*lump, m_bspFile.isBigEndian(), 8, m_config, readDEdgeStrataV1);
    } else {
        m_bspData.edges = readStructChunks<DEdge>(*lump, m_bspFile.isBigEndian(), 4, m_config, readDEdgeDefault);
    }
}

namespace {

std::vector<DFace> readFacesFromLump(
    const BspLumpBuffer& lump,
    int lumpVersion,
    int bspVersion,
    int appId,
    bool bigEndian,
    const BspDecompileConfig& config)
{
    switch (appId) {
    case SourceAppId::kVampireBloodlines:
        return readStructChunks<DFace>(lump, bigEndian, 104, config, readDFaceVTMB);
    case SourceAppId::kVindictus:
        if (lumpVersion == 2) {
            return readStructChunks<DFace>(lump, bigEndian, 76, config, [](BinarySpanReader& in) {
                return readDFaceVin(in, true);
            });
        }
        return readStructChunks<DFace>(lump, bigEndian, 72, config, [](BinarySpanReader& in) {
            return readDFaceVin(in, false);
        });
    case SourceAppId::kStrataSource:
        if (lumpVersion == 2) {
            return readStructChunks<DFace>(lump, bigEndian, 72, config, readDFaceStrataV2);
        }
        return readStructChunks<DFace>(lump, bigEndian, 56, config, readDFaceDefault);
    default:
        if (bspVersion == 17) {
            return readStructChunks<DFace>(lump, bigEndian, 68, config, readDFaceBSP17);
        }
        if (bspVersion == 18) {
            return readStructChunks<DFace>(lump, bigEndian, 72, config, readDFaceBSP18);
        }
        return readStructChunks<DFace>(lump, bigEndian, 56, config, readDFaceDefault);
    }
}

} // namespace

void BspFileReader::loadFaces() {
    if (m_loadedFaces) {
        return;
    }
    m_loadedFaces = true;

    const bool useHdrLump = m_bspFile.canReadLump(LumpType::LUMP_FACES_HDR)
        && m_bspFile.getLump(LumpType::LUMP_FACES_HDR) != nullptr
        && !m_bspFile.getLump(LumpType::LUMP_FACES_HDR)->data.empty();
    const LumpType faceLumpType = useHdrLump ? LumpType::LUMP_FACES_HDR : LumpType::LUMP_FACES;

    if (!m_bspFile.canReadLump(faceLumpType)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(faceLumpType);
    if (!lump || lump->data.empty()) {
        return;
    }

    m_bspData.faces = readFacesFromLump(
        *lump, lump->version, m_bspFile.version(), appId(), m_bspFile.isBigEndian(), m_config);
}

void BspFileReader::loadOriginalFaces() {
    if (m_loadedOriginalFaces) {
        return;
    }
    m_loadedOriginalFaces = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_ORIGINALFACES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_ORIGINALFACES);
    if (!lump || lump->data.empty()) {
        return;
    }

    // LUMP_ORIGINALFACES inherits lump version from LUMP_FACES
    const auto* facesLump = m_bspFile.getLump(LumpType::LUMP_FACES);
    const int lumpVersion = facesLump ? facesLump->version : lump->version;

    m_bspData.origFaces = readFacesFromLump(
        *lump, lumpVersion, m_bspFile.version(), appId(), m_bspFile.isBigEndian(), m_config);
}

void BspFileReader::loadModels() {
    if (m_loadedModels) {
        return;
    }
    m_loadedModels = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_MODELS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_MODELS);
    if (!lump || lump->data.empty()) {
        return;
    }

    const bool dm = (appId() == SourceAppId::kDarkMessiah);
    const std::size_t structSize = dm ? 52U : 48U;
    m_bspData.models = readStructChunks<DModel>(*lump, m_bspFile.isBigEndian(), structSize, m_config,
        [dm](BinarySpanReader& in) { return readDModel(in, dm); });
}

void BspFileReader::loadSurfaceEdges() {
    if (m_loadedSurfaceEdges) {
        return;
    }
    m_loadedSurfaceEdges = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_SURFEDGES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_SURFEDGES);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.surfEdges = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 4, m_config,
        [](BinarySpanReader& in) { return in.readInt32(); });
}

void BspFileReader::loadStaticProps() {
    if (m_loadedStaticProps) {
        return;
    }
    m_loadedStaticProps = true;

    const BspGameLumpBuffer* gl = m_bspFile.getGameLump(QStringLiteral("sprp"));
    if (!gl || gl->data.empty()) {
        return;
    }

    try {
        BinarySpanReader in(gl->data, m_bspFile.isBigEndian());
        const int sprpVersion = gl->version;
        const int app = appId();

        // Read static prop model dictionary
        const int nameCount = in.readInt32();
        if (nameCount < 0 || static_cast<std::size_t>(nameCount) > in.remaining() / 128U) {
            return;
        }
        m_bspData.staticPropName.reserve(static_cast<std::size_t>(nameCount));
        for (int i = 0; i < nameCount; ++i) {
            m_bspData.staticPropName.push_back(in.readStringFixed(128));
        }

        // Extra model path strings in Zeno Clash
        if (app == SourceAppId::kZenoClash) {
            const int psextra = in.readInt32();
            if (psextra < 0 || static_cast<std::size_t>(psextra) > in.remaining() / 128U) {
                return;
            }
            in.seek(static_cast<std::ptrdiff_t>(psextra) * 128);
        }

        // Read static prop leafs
        const int leafCount = in.readInt32();
        const std::size_t leafElemSize = (sprpVersion >= 12) ? 4U : 2U;
        if (leafCount < 0 || static_cast<std::size_t>(leafCount) > in.remaining() / leafElemSize) {
            return;
        }
        m_bspData.staticPropLeaf.reserve(static_cast<std::size_t>(leafCount));
        for (int i = 0; i < leafCount; ++i) {
            const int leafVal = (sprpVersion >= 12)
                ? static_cast<int>(in.readUInt32())
                : static_cast<int>(in.readUInt16());
            m_bspData.staticPropLeaf.push_back(leafVal);
        }

        // Read Vindictus scaling array
        QHash<int, Vector3f> vinScalingMap;
        if (app == SourceAppId::kVindictus && sprpVersion > 5) {
            const int scalingCount = in.readInt32();
            if (scalingCount < 0 || static_cast<std::size_t>(scalingCount) > in.remaining() / 16U) {
                return;
            }
            for (int i = 0; i < scalingCount; ++i) {
                const int propIdx = in.readInt32();
                const Vector3f vec = in.readVector3f();
                vinScalingMap.insert(propIdx, vec);
            }
        }

        // Read static prop instances
        const int propStaticCount = in.readInt32();
        if (propStaticCount <= 0) {
            return;
        }
        if (static_cast<std::size_t>(propStaticCount) > in.remaining()) {
            m_config.log(BspLogLevel::Warning,
                QStringLiteral("Invalid static prop count %1").arg(propStaticCount));
            return;
        }

        const int propStaticSize = static_cast<int>(in.remaining() / static_cast<std::size_t>(propStaticCount));

        const StaticPropDescriptor* matched = nullptr;
        for (const auto& desc : kStaticPropDescriptors) {
            if (desc.version != sprpVersion || desc.size != propStaticSize) {
                continue;
            }
            if (desc.appId != SourceAppId::kDefault && desc.appId != app) {
                continue;
            }
            if (!matched || desc.appId != SourceAppId::kDefault) {
                matched = &desc;
            }
        }

        StaticPropKind chosenKind = StaticPropKind::V4Padded;
        if (matched != nullptr) {
            chosenKind = matched->kind;
            m_config.log(BspLogLevel::Info,
                QStringLiteral("Using '%1' for sprp version %2")
                    .arg(QLatin1StringView(matched->name))
                    .arg(sprpVersion));
        } else {
            m_config.log(BspLogLevel::Warning,
                QStringLiteral("Couldn't find static prop struct for appId %1, version %2, size %3; falling back to static prop v4")
                    .arg(app)
                    .arg(sprpVersion)
                    .arg(propStaticSize));
            if (propStaticSize < 56) {
                return;
            }
        }

        const bool isVinScaling = (chosenKind == StaticPropKind::V6VIN
            || chosenKind == StaticPropKind::V7VIN
            || chosenKind == StaticPropKind::V8VIN);

        m_bspData.staticProps.reserve(static_cast<std::size_t>(propStaticCount));
        for (int i = 0; i < propStaticCount; ++i) {
            DStaticProp sp = readStaticPropByKind(in, chosenKind, propStaticSize);
            if (isVinScaling) {
                const auto it = vinScalingMap.constFind(i);
                if (it != vinScalingMap.constEnd()) {
                    sp.hasVinScaling = true;
                    sp.vinScaling = it.value();
                }
            }
            m_bspData.staticProps.push_back(std::move(sp));
        }

        if (in.hasRemaining()) {
            m_config.log(BspLogLevel::Warning,
                QStringLiteral("%1 bytes remaining after reading static props").arg(in.remaining()));
        }
    } catch (const std::exception& ex) {
        m_config.log(BspLogLevel::Warning,
            QStringLiteral("Error reading static props: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void BspFileReader::loadCubemaps() {
    if (m_loadedCubemaps) {
        return;
    }
    m_loadedCubemaps = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_CUBEMAPS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_CUBEMAPS);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.cubemaps = readStructChunks<DCubemapSample>(*lump, m_bspFile.isBigEndian(), 16, m_config, readDCubemapSample);
}

void BspFileReader::loadDispInfos() {
    if (m_loadedDispInfos) {
        return;
    }
    m_loadedDispInfos = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_DISPINFO)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_DISPINFO);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    const int ver = m_bspFile.version();
    if (app == SourceAppId::kVindictus) {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 232, m_config, readDDispInfoVin);
    } else if (app == SourceAppId::kHalfLife2 && ver == 17) {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 172, m_config, readDDispInfoBSP17);
    } else if (app == SourceAppId::kDota2Beta && ver == 22) {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 180, m_config, readDDispInfoBSP22);
    } else if (app == SourceAppId::kDota2Beta && ver >= 23) {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 184, m_config, readDDispInfoBSP23);
    } else if (app == SourceAppId::kStrataSource) {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 232, m_config, readDDispInfoStrataV1);
    } else {
        m_bspData.dispinfos = readStructChunks<DDispInfo>(*lump, m_bspFile.isBigEndian(), 176, m_config, readDDispInfoDefault);
    }
}

void BspFileReader::loadDispVertices() {
    if (m_loadedDispVertices) {
        return;
    }
    m_loadedDispVertices = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_DISP_VERTS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_DISP_VERTS);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.dispverts = readStructChunks<DDispVert>(*lump, m_bspFile.isBigEndian(), 20, m_config, readDDispVert);
}

void BspFileReader::loadDispTriangleTags() {
    if (m_loadedDispTriangleTags) {
        return;
    }
    m_loadedDispTriangleTags = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_DISP_TRIS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_DISP_TRIS);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.disptris = readStructChunks<DDispTri>(*lump, m_bspFile.isBigEndian(), 2, m_config, readDDispTri);
}

void BspFileReader::loadDispMultiBlend() {
    if (m_loadedDispMultiBlend) {
        return;
    }
    m_loadedDispMultiBlend = true;
    const LumpType lumpType = (appId() == SourceAppId::kBlackMesa)
        ? LumpType::LUMP_OVERLAY_SYSTEM_LEVELS
        : LumpType::LUMP_DISP_MULTIBLEND;

    if (!m_bspFile.canReadLump(lumpType)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(lumpType);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.dispmultiblend = readStructChunks<DDispMultiBlend>(*lump, m_bspFile.isBigEndian(), 80, m_config, readDDispMultiBlend);
}

void BspFileReader::loadTexInfo() {
    if (m_loadedTexInfo) {
        return;
    }
    m_loadedTexInfo = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_TEXINFO)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_TEXINFO);
    if (!lump || lump->data.empty()) {
        return;
    }
    const bool dm = (appId() == SourceAppId::kDarkMessiah);
    const std::size_t structSize = dm ? 96U : 72U;
    m_bspData.texinfos = readStructChunks<DTexInfo>(*lump, m_bspFile.isBigEndian(), structSize, m_config,
        [dm](BinarySpanReader& in) { return readDTexInfo(in, dm); });
}

void BspFileReader::loadTexData() {
    if (m_loadedTexData) {
        return;
    }
    m_loadedTexData = true;
    if (m_bspFile.canReadLump(LumpType::LUMP_TEXDATA)) {
        const auto* lump = m_bspFile.getLump(LumpType::LUMP_TEXDATA);
        if (lump && !lump->data.empty()) {
            m_bspData.texdatas = readStructChunks<DTexData>(*lump, m_bspFile.isBigEndian(), 32, m_config, readDTexData);
        }
    }
    loadTexDataStrings();
}

void BspFileReader::loadTexDataStrings() {
    if (m_loadedTexDataStrings) {
        return;
    }
    m_loadedTexDataStrings = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_TEXDATA_STRING_TABLE)
        || !m_bspFile.canReadLump(LumpType::LUMP_TEXDATA_STRING_DATA)) {
        return;
    }
    const auto* tableLump = m_bspFile.getLump(LumpType::LUMP_TEXDATA_STRING_TABLE);
    const auto* dataLump = m_bspFile.getLump(LumpType::LUMP_TEXDATA_STRING_DATA);
    if (!tableLump || tableLump->data.empty() || !dataLump || dataLump->data.empty()) {
        return;
    }

    const std::vector<int> stringOffsets = readStructChunks<int>(
        *tableLump, m_bspFile.isBigEndian(), 4, m_config,
        [](BinarySpanReader& in) { return in.readInt32(); });

    try {
        BinarySpanReader dataReader(dataLump->data, m_bspFile.isBigEndian());
        m_bspData.texnames.reserve(stringOffsets.size());
        for (int ofs : stringOffsets) {
            if (ofs < 0 || static_cast<std::size_t>(ofs) >= dataLump->data.size()) {
                m_bspData.texnames.emplace_back();
                continue;
            }
            dataReader.setPosition(static_cast<std::size_t>(ofs));
            m_bspData.texnames.push_back(dataReader.readStringNullTerminated(dataLump->data.size() - static_cast<std::size_t>(ofs)));
        }
    } catch (const std::exception& ex) {
        m_config.log(BspLogLevel::Warning,
            QStringLiteral("Error reading texdata strings: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void BspFileReader::loadEntities() {
    if (m_loadedEntities) {
        return;
    }
    m_loadedEntities = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_ENTITIES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_ENTITIES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const bool allowEscSeq = (m_bspFile.version() == 17);
    const std::span<const std::byte> bytes(lump->data);

    bool section = false;
    bool inString = false;
    bool esc = false;
    QByteArray tokenBuf;
    tokenBuf.reserve(512);
    std::vector<EntityKeyValue> keyValues;
    std::optional<QString> currentKey;

    auto skipRestOfSection = [&](std::size_t& idx) {
        while (idx < bytes.size() && static_cast<char>(bytes[idx]) != '}') {
            ++idx;
        }
        m_bspData.entities.push_back(buildEntity(keyValues, m_config));
        section = false;
        inString = false;
        esc = false;
        tokenBuf.clear();
        keyValues.clear();
        currentKey.reset();
    };

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const char ch = static_cast<char>(bytes[i]);
        switch (ch) {
        case '"':
            if (!section) {
                m_config.log(BspLogLevel::Warning,
                    QStringLiteral("String in unopened entity section at %1").arg(i));
                skipRestOfSection(i);
                continue;
            }
            if (esc) {
                esc = false;
                break;
            }
            if (inString) {
                QString str = QString::fromUtf8(tokenBuf);
                if (!currentKey.has_value()) {
                    currentKey = std::move(str);
                } else {
                    if (currentKey->isEmpty()) {
                        m_config.log(BspLogLevel::Debug,
                            QStringLiteral("Skipped value \"%1\" with empty key at %2").arg(str).arg(i));
                    } else {
                        keyValues.push_back(EntityKeyValue{*currentKey, std::move(str)});
                    }
                    currentKey.reset();
                }
                tokenBuf.clear();
            }
            inString = !inString;
            continue;

        case '{':
            if (section && !inString) {
                m_config.log(BspLogLevel::Warning,
                    QStringLiteral("Opened unclosed entity section at %1").arg(i));
                skipRestOfSection(i);
                continue;
            }
            if (!inString) {
                section = true;
            }
            break;

        case '}':
            if (!section && !inString) {
                m_config.log(BspLogLevel::Warning,
                    QStringLiteral("Closed unopened entity section at %1").arg(i));
                skipRestOfSection(i);
                continue;
            }
            if (!inString) {
                m_bspData.entities.push_back(buildEntity(keyValues, m_config));
                section = false;
                inString = false;
                esc = false;
                tokenBuf.clear();
                keyValues.clear();
                currentKey.reset();
                continue;
            }
            break;

        case '\\':
            if (allowEscSeq) {
                esc = true;
            }
            break;

        default:
            break;
        }

        if (section && inString) {
            tokenBuf.append(ch);
        }
    }

    if (m_bspFile.appId() == SourceAppId::kUnknown) {
        QSet<QString> entityClasses;
        entityClasses.reserve(static_cast<qsizetype>(m_bspData.entities.size()));
        for (const auto& ent : m_bspData.entities) {
            entityClasses.insert(ent.className());
        }
        const int detected = SourceAppDb::instance().find(
            m_bspFile.name(), m_bspFile.version(), entityClasses);
        m_bspFile.setAppId(detected);
    }
}

void BspFileReader::loadNodes() {
    if (m_loadedNodes) {
        return;
    }
    m_loadedNodes = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_NODES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_NODES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus) {
        m_bspData.nodes = readStructChunks<DNode>(*lump, m_bspFile.isBigEndian(), 48, m_config, readDNodeVin);
    } else if (app == SourceAppId::kStrataSource) {
        m_bspData.nodes = readStructChunks<DNode>(*lump, m_bspFile.isBigEndian(), 48, m_config, readDNodeStrataV1);
    } else {
        m_bspData.nodes = readStructChunks<DNode>(*lump, m_bspFile.isBigEndian(), 32, m_config, readDNodeDefault);
    }
}

void BspFileReader::loadLeaves() {
    if (m_loadedLeaves) {
        return;
    }
    m_loadedLeaves = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_LEAFS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_LEAFS);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus) {
        m_bspData.leaves = readStructChunks<DLeaf>(*lump, m_bspFile.isBigEndian(), 56, m_config, readDLeafVin);
    } else if (lump->version == 0 && m_bspFile.version() == 19) {
        m_bspData.leaves = readStructChunks<DLeaf>(*lump, m_bspFile.isBigEndian(), 56, m_config, readDLeafV0);
    } else if (app == SourceAppId::kStrataSource && lump->version == 2) {
        m_bspData.leaves = readStructChunks<DLeaf>(*lump, m_bspFile.isBigEndian(), 56, m_config, readDLeafStrataV2);
    } else {
        m_bspData.leaves = readStructChunks<DLeaf>(*lump, m_bspFile.isBigEndian(), 32, m_config, readDLeafV1);
    }
}

void BspFileReader::loadLeafFaces() {
    if (m_loadedLeafFaces) {
        return;
    }
    m_loadedLeafFaces = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_LEAFFACES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_LEAFFACES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus || app == SourceAppId::kStrataSource) {
        m_bspData.leafFaces = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 4, m_config,
            [](BinarySpanReader& in) { return in.readInt32(); });
    } else {
        m_bspData.leafFaces = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 2, m_config,
            [](BinarySpanReader& in) { return static_cast<int>(in.readUInt16()); });
    }
}

void BspFileReader::loadLeafBrushes() {
    if (m_loadedLeafBrushes) {
        return;
    }
    m_loadedLeafBrushes = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_LEAFBRUSHES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_LEAFBRUSHES);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus || app == SourceAppId::kStrataSource) {
        m_bspData.leafBrushes = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 4, m_config,
            [](BinarySpanReader& in) { return in.readInt32(); });
    } else {
        m_bspData.leafBrushes = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 2, m_config,
            [](BinarySpanReader& in) { return static_cast<int>(in.readUInt16()); });
    }
}

void BspFileReader::loadOverlays() {
    if (!m_loadedOverlays) {
        m_loadedOverlays = true;
        if (m_bspFile.canReadLump(LumpType::LUMP_OVERLAYS)) {
            const auto* lump = m_bspFile.getLump(LumpType::LUMP_OVERLAYS);
            if (lump && !lump->data.empty()) {
                const int app = appId();
                if (app == SourceAppId::kVindictus) {
                    m_bspData.overlays = readStructChunks<DOverlay>(*lump, m_bspFile.isBigEndian(), 356, m_config, readDOverlayVin);
                } else if (app == SourceAppId::kDota2Beta) {
                    m_bspData.overlays = readStructChunks<DOverlay>(*lump, m_bspFile.isBigEndian(), 356, m_config, readDOverlayDota2);
                } else if (app == SourceAppId::kStrataSource) {
                    m_bspData.overlays = readStructChunks<DOverlay>(*lump, m_bspFile.isBigEndian(), 356, m_config, readDOverlayStrataV1);
                } else {
                    m_bspData.overlays = readStructChunks<DOverlay>(*lump, m_bspFile.isBigEndian(), 352, m_config, readDOverlayDefault);
                }
            }
        }
    }

    if (!m_loadedOverlayFades) {
        m_loadedOverlayFades = true;
        if (m_bspFile.canReadLump(LumpType::LUMP_OVERLAY_FADES)) {
            const auto* lump = m_bspFile.getLump(LumpType::LUMP_OVERLAY_FADES);
            if (lump && !lump->data.empty()) {
                m_bspData.overlayFades = readStructChunks<DOverlayFade>(*lump, m_bspFile.isBigEndian(), 8, m_config, readDOverlayFade);
            }
        }
    }

    if (!m_loadedOverlaySysLevels) {
        m_loadedOverlaySysLevels = true;
        if (appId() != SourceAppId::kBlackMesa && m_bspFile.canReadLump(LumpType::LUMP_OVERLAY_SYSTEM_LEVELS)) {
            const auto* lump = m_bspFile.getLump(LumpType::LUMP_OVERLAY_SYSTEM_LEVELS);
            if (lump && !lump->data.empty()) {
                m_bspData.overlaySysLevels = readStructChunks<DOverlaySystemLevel>(
                    *lump, m_bspFile.isBigEndian(), 4, m_config, readDOverlaySystemLevel);
            }
        }
    }
}

void BspFileReader::loadAreaportals() {
    if (m_loadedAreaportals) {
        return;
    }
    m_loadedAreaportals = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_AREAPORTALS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_AREAPORTALS);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int app = appId();
    if (app == SourceAppId::kVindictus) {
        m_bspData.areaportals = readStructChunks<DAreaportal>(*lump, m_bspFile.isBigEndian(), 20, m_config, readDAreaportalVin);
    } else if (app == SourceAppId::kStrataSource) {
        m_bspData.areaportals = readStructChunks<DAreaportal>(*lump, m_bspFile.isBigEndian(), 20, m_config, readDAreaportalStrataV1);
    } else {
        m_bspData.areaportals = readStructChunks<DAreaportal>(*lump, m_bspFile.isBigEndian(), 12, m_config, readDAreaportalDefault);
    }
}

void BspFileReader::loadOccluders() {
    if (m_loadedOccluders) {
        return;
    }
    m_loadedOccluders = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_OCCLUSION)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_OCCLUSION);
    if (!lump || lump->data.empty()) {
        return;
    }

    const int alteredVersion = (appId() == SourceAppId::kContagion) ? 1 : lump->version;
    const bool hasArea = (alteredVersion != 0);

    try {
        BinarySpanReader in(lump->data, m_bspFile.isBigEndian());

        const int occluderCount = in.readInt32();
        const std::size_t occluderElemSize = hasArea ? 40U : 36U;
        if (occluderCount < 0 || static_cast<std::size_t>(occluderCount) > in.remaining() / occluderElemSize) {
            return;
        }
        m_bspData.occluderDatas.reserve(static_cast<std::size_t>(occluderCount));
        for (int i = 0; i < occluderCount; ++i) {
            DOccluderData occ;
            occ.flags = in.readInt32();
            occ.firstpoly = in.readInt32();
            occ.polycount = in.readInt32();
            occ.mins = in.readVector3f();
            occ.maxs = in.readVector3f();
            if (hasArea) {
                occ.area = in.readInt32();
            }
            m_bspData.occluderDatas.push_back(occ);
        }

        const int polyCount = in.readInt32();
        if (polyCount < 0 || static_cast<std::size_t>(polyCount) > in.remaining() / 12U) {
            return;
        }
        m_bspData.occluderPolyDatas.reserve(static_cast<std::size_t>(polyCount));
        for (int i = 0; i < polyCount; ++i) {
            DOccluderPolyData poly;
            poly.firstvertexindex = in.readInt32();
            poly.vertexcount = in.readInt32();
            poly.planenum = in.readInt32();
            m_bspData.occluderPolyDatas.push_back(poly);
        }

        const int vertexCount = in.readInt32();
        if (vertexCount < 0 || static_cast<std::size_t>(vertexCount) > in.remaining() / 4U) {
            return;
        }
        m_bspData.occluderVerts.reserve(static_cast<std::size_t>(vertexCount));
        for (int i = 0; i < vertexCount; ++i) {
            m_bspData.occluderVerts.push_back(in.readInt32());
        }

        if (in.hasRemaining()) {
            m_config.log(BspLogLevel::Warning,
                QStringLiteral("%1 bytes remaining after reading occluders").arg(in.remaining()));
        }
    } catch (const std::exception& ex) {
        m_config.log(BspLogLevel::Warning,
            QStringLiteral("Error reading occluders: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void BspFileReader::loadFlags() {
    if (m_loadedFlags) {
        return;
    }
    m_loadedFlags = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_MAP_FLAGS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_MAP_FLAGS);
    if (!lump || lump->data.size() < 4) {
        return;
    }

    try {
        BinarySpanReader in(lump->data, m_bspFile.isBigEndian());
        m_bspData.mapFlags = levelFlagsFromInt(in.readInt32());
    } catch (const std::exception& ex) {
        m_config.log(BspLogLevel::Warning,
            QStringLiteral("Error reading map flags: %1").arg(QString::fromUtf8(ex.what())));
    }
}

void BspFileReader::loadPrimitives() {
    if (m_loadedPrimitives) {
        return;
    }
    m_loadedPrimitives = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_PRIMITIVES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_PRIMITIVES);
    if (!lump || lump->data.empty()) {
        return;
    }

    if (appId() == SourceAppId::kStrataSource) {
        m_bspData.prims = readStructChunks<DPrimitive>(*lump, m_bspFile.isBigEndian(), 20, m_config, readDPrimitiveStrataV1);
    } else {
        m_bspData.prims = readStructChunks<DPrimitive>(*lump, m_bspFile.isBigEndian(), 10, m_config, readDPrimitiveDefault);
    }
}

void BspFileReader::loadPrimIndices() {
    if (m_loadedPrimIndices) {
        return;
    }
    m_loadedPrimIndices = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_PRIMINDICES)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_PRIMINDICES);
    if (!lump || lump->data.empty()) {
        return;
    }

    if (appId() == SourceAppId::kStrataSource) {
        m_bspData.primIndices = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 4, m_config,
            [](BinarySpanReader& in) { return in.readInt32(); });
    } else {
        m_bspData.primIndices = readStructChunks<int>(*lump, m_bspFile.isBigEndian(), 2, m_config,
            [](BinarySpanReader& in) { return static_cast<int>(in.readUInt16()); });
    }
}

void BspFileReader::loadPrimVerts() {
    if (m_loadedPrimVerts) {
        return;
    }
    m_loadedPrimVerts = true;
    if (!m_bspFile.canReadLump(LumpType::LUMP_PRIMVERTS)) {
        return;
    }
    const auto* lump = m_bspFile.getLump(LumpType::LUMP_PRIMVERTS);
    if (!lump || lump->data.empty()) {
        return;
    }
    m_bspData.primVerts = readStructChunks<DVertex>(*lump, m_bspFile.isBigEndian(), 12, m_config, readDVertex);
}

Core::Result<NmoData> NmoFileParser::load(const Core::Path::FilesystemPath& nmoPath) {
    if (nmoPath.isEmpty() || !nmoPath.isValid()) {
        return Core::Result<NmoData>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("NmoFileParser", "NMO file path is empty or invalid"));
    }
    if (!nmoPath.exists()) {
        return Core::Result<NmoData>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("NmoFileParser", "NMO file not found"),
            nmoPath.toString());
    }

    auto bytesRes = readAllFileBytes(nmoPath);
    if (bytesRes.isFailure()) {
        return Core::Result<NmoData>::failure(bytesRes.error());
    }

    return Core::Error::ExecutionGuard::guard<NmoData>(
        [&]() -> Core::Result<NmoData> {
            BinarySpanReader in(bytesRes.value(), false);
            const std::uint8_t signature = in.readUInt8();
            const std::int32_t version = in.readInt32();
            if (signature != static_cast<std::uint8_t>(kNmoSignature)) {
                return Core::Result<NmoData>::failure(
                    Core::Error::ErrorCode::InvalidFile,
                    QCoreApplication::translate("NmoFileParser", "Invalid NMO file signature"),
                    nmoPath.toString());
            }
            if (version != kNmoVersion) {
                return Core::Result<NmoData>::failure(
                    Core::Error::ErrorCode::NotSupported,
                    QCoreApplication::translate("NmoFileParser", "Unsupported NMO file version: %1").arg(version),
                    nmoPath.toString());
            }

            const int nodeCount = in.readInt32();
            const int antiNodeCount = in.readInt32();
            const int extractionCount = in.readInt32();
            if (nodeCount < 0 || antiNodeCount < 0 || extractionCount < 0) {
                return Core::Result<NmoData>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("NmoFileParser", "Corrupted NMO section counts"),
                    nmoPath.toString());
            }

            NmoData nmo;
            nmo.nodes.reserve(static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(nodeCount), in.remaining())));
            for (int i = 0; i < nodeCount; ++i) {
                NmoObjective obj;
                obj.id = in.readInt32();
                obj.name = in.readStringNullTerminated();
                obj.comment = in.readStringNullTerminated();
                obj.entityName = in.readStringNullTerminated();

                const int entityCount = in.readInt32();
                if (entityCount < 0 || static_cast<std::size_t>(entityCount) > in.remaining()) {
                    return Core::Result<NmoData>::failure(
                        Core::Error::ErrorCode::CorruptedData,
                        QCoreApplication::translate("NmoFileParser", "Corrupted NMO objective entity count"),
                        nmoPath.toString());
                }
                QSet<QString> seenEntities;
                for (int e = 0; e < entityCount; ++e) {
                    QString entName = in.readStringNullTerminated();
                    if (!seenEntities.contains(entName)) {
                        seenEntities.insert(entName);
                        obj.entities.push_back(std::move(entName));
                    }
                }

                const int childrenCount = in.readInt32();
                if (childrenCount < 0 || static_cast<std::size_t>(childrenCount) > in.remaining() / 4U) {
                    return Core::Result<NmoData>::failure(
                        Core::Error::ErrorCode::CorruptedData,
                        QCoreApplication::translate("NmoFileParser", "Corrupted NMO objective children count"),
                        nmoPath.toString());
                }
                QSet<int> seenChildren;
                for (int c = 0; c < childrenCount; ++c) {
                    const int childId = in.readInt32();
                    if (!seenChildren.contains(childId)) {
                        seenChildren.insert(childId);
                        obj.children.push_back(childId);
                    }
                }
                nmo.nodes.push_back(std::move(obj));
            }

            nmo.antiNodes.reserve(static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(antiNodeCount), in.remaining())));
            for (int i = 0; i < antiNodeCount; ++i) {
                NmoAntiObjective anti;
                anti.id = in.readInt32();
                anti.name = in.readStringNullTerminated();

                const int entityCount = in.readInt32();
                if (entityCount < 0 || static_cast<std::size_t>(entityCount) > in.remaining()) {
                    return Core::Result<NmoData>::failure(
                        Core::Error::ErrorCode::CorruptedData,
                        QCoreApplication::translate("NmoFileParser", "Corrupted NMO anti-objective entity count"),
                        nmoPath.toString());
                }
                QSet<QString> seenEntities;
                for (int e = 0; e < entityCount; ++e) {
                    QString entName = in.readStringNullTerminated();
                    if (!seenEntities.contains(entName)) {
                        seenEntities.insert(entName);
                        anti.entities.push_back(std::move(entName));
                    }
                }
                nmo.antiNodes.push_back(std::move(anti));
            }

            nmo.extractions.reserve(static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(extractionCount), in.remaining())));
            for (int i = 0; i < extractionCount; ++i) {
                NmoExtraction ext;
                ext.id = in.readInt32();
                ext.name = in.readStringNullTerminated();
                nmo.extractions.push_back(std::move(ext));
            }

            return Core::Result<NmoData>::success(std::move(nmo));
        },
        Core::Error::ExecutionContext{
            .stage = QStringLiteral("LoadNMO"),
            .targetPath = nmoPath.toString(),
        });
}

Core::Result<void> NmoFileParser::writeAsNmos(
    const NmoData& nmo,
    const Core::Path::FilesystemPath& nmosPath,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    if (nmosPath.isEmpty() || !nmosPath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("NmoFileParser", "NMOS output path is empty or invalid"));
    }

    QByteArray out;
    out.reserve(256);
    out.append(static_cast<char>(kNmoSignature));
    writeInt32LE(out, kNmoVersion);
    writeInt32LE(out, static_cast<std::int32_t>(nmo.nodes.size() + nmo.extractions.size()));

    std::int32_t xCoordinate = 0;
    constexpr std::int32_t kSpacing = 75;

    for (const auto& obj : nmo.nodes) {
        out.append(static_cast<char>(1));
        writeInt32LE(out, obj.id);
        writeInt32LE(out, xCoordinate);
        writeInt32LE(out, 0);
        writeStringPrefixedU8(out, obj.comment);
        writeInt32LE(out, static_cast<std::int32_t>(obj.children.size()));
        for (int child : obj.children) {
            writeInt32LE(out, child);
        }
        xCoordinate += kSpacing;
    }

    for (const auto& ext : nmo.extractions) {
        out.append(static_cast<char>(0));
        writeInt32LE(out, ext.id);
        writeInt32LE(out, xCoordinate);
        writeInt32LE(out, 0);
        writeInt32LE(out, 0);
        xCoordinate += kSpacing;
    }

    return Core::Error::ExecutionGuard::guard<void>(
        [&]() -> Core::Result<void> {
            Core::FileSystem::AtomicFile::writeAtomic(nmosPath.toString(), out, expectedBaseDir);
            return Core::Result<void>::success();
        },
        Core::Error::ExecutionContext{
            .stage = QStringLiteral("WriteNMOS"),
            .targetPath = nmosPath.toString(),
        });
}

} // namespace Domain::Bsp::Internal
