#include "Domain/Material/VtfCodec.h"

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QtZlib/zlib.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Domain::Material {

namespace {

constexpr std::uint32_t kVtfSignature = 0x00465456u; // "VTF\0" little-endian
constexpr std::uint32_t kPcVersionMajor = 7u;
constexpr std::uint32_t kMaxSupportedMinor = 6u;
constexpr std::uint32_t kMinVtfHeaderSize = 64u;
constexpr std::uint32_t kV73HeaderBaseSize = 80u;
constexpr std::uint32_t kMaxHeaderReadBytes = 16u * 1024u * 1024u;

constexpr std::uint32_t kFlagNoMip = static_cast<std::uint32_t>(VtfFlag::NoMip);
constexpr std::uint32_t kFlagOneBitAlpha = static_cast<std::uint32_t>(VtfFlag::OneBitAlpha);
constexpr std::uint32_t kFlagMultiBitAlpha = static_cast<std::uint32_t>(VtfFlag::EightBitAlpha);
constexpr std::uint32_t kFlagEnvMap = static_cast<std::uint32_t>(VtfFlag::EnvMap);

constexpr std::uint32_t kResourceTypeSheet = 0x00000010u;
constexpr std::uint32_t kResourceTypeImageData = 48u; // {0x30, 0x00, 0x00}
constexpr std::uint32_t kResourceTypeAuxCompression = 0x00435841u; // "AXC"
constexpr std::uint8_t kResourceFlagLocalData = 1u << 1;

constexpr std::int16_t kCompressionDeflate = 8;
constexpr std::int16_t kCompressionZstd = 93;

constexpr std::uint64_t kMaxDecodeBufferBytes = 256ull * 1024ull * 1024ull;
constexpr qint64 kMaxVtfFileReadBytes = 512ll * 1024ll * 1024ll;

struct VtfResourceEntry {
    std::uint32_t type = 0;
    std::uint8_t flags = 0;
    std::uint32_t offsetOrData = 0;
    std::uint64_t size = 0;
};

struct DecodedVtfHeader {
    std::uint32_t versionMajor = kPcVersionMajor;
    std::uint32_t versionMinor = 0;
    std::uint32_t headerSize = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t depth = 1;
    std::uint32_t flags = 0;
    std::uint16_t frameCount = 1;
    std::array<float, 3> reflectivity = {0.0f, 0.0f, 0.0f};
    float bumpmapScale = 1.0f;
    VtfImageFormat format = VtfImageFormat::EMPTY;
    std::uint8_t mipCount = 1;
    bool isCompressedHdrBgra = false;
    bool isCompressedHdrRgba16 = false;
    std::uint64_t imageDataOffset = 0;
    std::uint64_t imageResourceSize = 0;
    bool hasAxc = false;
    std::uint64_t axcOffset = 0;
    std::uint64_t axcSize = 0;
    bool hasSheet = false;
    std::uint64_t sheetOffset = 0;
    std::uint64_t sheetSize = 0;
    std::uint8_t faceCount = 1;
};

struct InflateStreamGuard {
    z_stream strm{};
    bool initialized = false;

    ~InflateStreamGuard()
    {
        if (initialized) {
            inflateEnd(&strm);
        }
    }
};

std::uint16_t readU16Le(const std::byte* p) noexcept
{
    const auto b0 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(p[0]));
    const auto b1 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(p[1]));
    return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

std::uint32_t readU32Le(const std::byte* p) noexcept
{
    const auto b0 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[0]));
    const auto b1 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[1]));
    const auto b2 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[2]));
    const auto b3 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(p[3]));
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

std::int32_t readI32Le(const std::byte* p) noexcept
{
    return std::bit_cast<std::int32_t>(readU32Le(p));
}

float readF32Le(const std::byte* p) noexcept
{
    return std::bit_cast<float>(readU32Le(p));
}

std::uint64_t readU64Le(const std::byte* p) noexcept
{
    const auto lo = static_cast<std::uint64_t>(readU32Le(p));
    const auto hi = static_cast<std::uint64_t>(readU32Le(p + 4));
    return lo | (hi << 32);
}

std::uint8_t readU8(const std::byte* p) noexcept
{
    return std::to_integer<std::uint8_t>(*p);
}

bool isBlockCompressed(VtfImageFormat format) noexcept
{
    switch (format) {
        case VtfImageFormat::DXT1:
        case VtfImageFormat::DXT3:
        case VtfImageFormat::DXT5:
        case VtfImageFormat::DXT1_ONE_BIT_ALPHA:
        case VtfImageFormat::ATI2N:
        case VtfImageFormat::ATI1N:
            return true;
        default:
            return false;
    }
}

bool isFloatingPointFormat(VtfImageFormat format, bool isCompressedHdrBgra, bool isCompressedHdrRgba16) noexcept
{
    if (isCompressedHdrBgra || isCompressedHdrRgba16) {
        return true;
    }
    switch (format) {
        case VtfImageFormat::RGBA16161616F:
        case VtfImageFormat::R32F:
        case VtfImageFormat::RGB323232F:
        case VtfImageFormat::RGBA32323232F:
        case VtfImageFormat::RG1616F:
        case VtfImageFormat::RG3232F:
        case VtfImageFormat::R16F:
            return true;
        default:
            return false;
    }
}

bool formatHasAlpha(VtfImageFormat format) noexcept
{
    switch (format) {
        case VtfImageFormat::RGBA8888:
        case VtfImageFormat::ABGR8888:
        case VtfImageFormat::IA88:
        case VtfImageFormat::A8:
        case VtfImageFormat::RGB888_BLUESCREEN:
        case VtfImageFormat::BGR888_BLUESCREEN:
        case VtfImageFormat::ARGB8888:
        case VtfImageFormat::BGRA8888:
        case VtfImageFormat::DXT3:
        case VtfImageFormat::DXT5:
        case VtfImageFormat::BGRA4444:
        case VtfImageFormat::DXT1_ONE_BIT_ALPHA:
        case VtfImageFormat::BGRA5551:
        case VtfImageFormat::UVWQ8888:
        case VtfImageFormat::RGBA16161616F:
        case VtfImageFormat::RGBA16161616:
        case VtfImageFormat::RGBA32323232F:
        case VtfImageFormat::RGBA1010102:
        case VtfImageFormat::BGRA1010102:
            return true;
        default:
            return false;
    }
}

// Returns 0 if format is unsupported or EMPTY.
std::uint32_t formatBpp(VtfImageFormat format) noexcept
{
    switch (format) {
        case VtfImageFormat::RGBA32323232F:
            return 128;
        case VtfImageFormat::RGB323232F:
            return 96;
        case VtfImageFormat::RGBA16161616F:
        case VtfImageFormat::RGBA16161616:
        case VtfImageFormat::RG3232F:
            return 64;
        case VtfImageFormat::RGBA8888:
        case VtfImageFormat::ABGR8888:
        case VtfImageFormat::ARGB8888:
        case VtfImageFormat::BGRA8888:
        case VtfImageFormat::BGRX8888:
        case VtfImageFormat::UVWQ8888:
        case VtfImageFormat::UVLX8888:
        case VtfImageFormat::R32F:
        case VtfImageFormat::RG1616F:
        case VtfImageFormat::RGBX8888:
        case VtfImageFormat::RGBA1010102:
        case VtfImageFormat::BGRA1010102:
            return 32;
        case VtfImageFormat::RGB888:
        case VtfImageFormat::BGR888:
        case VtfImageFormat::RGB888_BLUESCREEN:
        case VtfImageFormat::BGR888_BLUESCREEN:
            return 24;
        case VtfImageFormat::RGB565:
        case VtfImageFormat::IA88:
        case VtfImageFormat::BGR565:
        case VtfImageFormat::BGRX5551:
        case VtfImageFormat::BGRA4444:
        case VtfImageFormat::BGRA5551:
        case VtfImageFormat::UV88:
        case VtfImageFormat::R16F:
            return 16;
        case VtfImageFormat::I8:
        case VtfImageFormat::P8:
        case VtfImageFormat::A8:
        case VtfImageFormat::DXT3:
        case VtfImageFormat::DXT5:
        case VtfImageFormat::ATI2N:
            return 8;
        case VtfImageFormat::DXT1:
        case VtfImageFormat::DXT1_ONE_BIT_ALPHA:
        case VtfImageFormat::ATI1N:
            return 4;
        case VtfImageFormat::EMPTY:
        default:
            return 0;
    }
}

std::uint16_t mipDimension(std::uint8_t mip, std::uint16_t dim) noexcept
{
    if (dim == 0) {
        return 0;
    }
    for (int i = 0; i < mip && dim > 1; ++i) {
        dim >>= 1;
    }
    return dim;
}

std::uint64_t calcSliceBytes(VtfImageFormat format, std::uint16_t width, std::uint16_t height) noexcept
{
    const std::uint64_t bpp = formatBpp(format);
    if (bpp == 0) {
        return 0;
    }
    if (isBlockCompressed(format)) {
        const std::uint64_t blocksX = (static_cast<std::uint64_t>(width) + 3u) / 4u;
        const std::uint64_t blocksY = (static_cast<std::uint64_t>(height) + 3u) / 4u;
        const std::uint64_t blockBytes = bpp * 2u; // 4 bpp -> 8 bytes, 8 bpp -> 16 bytes
        return blocksX * blocksY * blockBytes;
    }
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * (bpp / 8u);
}

enum class BcColorMode : int {
    Dxt1Opaque,
    Dxt1OneBitAlpha,
    FourColorOnly,
};

bool checkedMul(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
{
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return false;
    }
    out = a * b;
    return true;
}

bool checkedAdd(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
{
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        return false;
    }
    out = a + b;
    return true;
}

std::optional<std::uint64_t> calcAllMipsBytes(
    VtfImageFormat format,
    std::uint8_t mipCount,
    std::uint16_t frameCount,
    std::uint8_t faceCount,
    std::uint16_t width,
    std::uint16_t height,
    std::uint16_t depth) noexcept
{
    std::uint64_t frameFace = 0;
    if (!checkedMul(frameCount, faceCount, frameFace)) {
        return std::nullopt;
    }

    std::uint64_t total = 0;
    for (int m = static_cast<int>(mipCount) - 1; m >= 0; --m) {
        const std::uint16_t mw = mipDimension(static_cast<std::uint8_t>(m), width);
        const std::uint16_t mh = mipDimension(static_cast<std::uint8_t>(m), height);
        const std::uint16_t md = mipDimension(static_cast<std::uint8_t>(m), depth);
        std::uint64_t count = 0;
        std::uint64_t mipBytes = 0;
        if (!checkedMul(frameFace, md, count)
            || !checkedMul(calcSliceBytes(format, mw, mh), count, mipBytes)
            || !checkedAdd(total, mipBytes, total)) {
            return std::nullopt;
        }
    }
    return total;
}

std::uint8_t remapTo8(std::uint32_t value, std::uint32_t maxSrc) noexcept
{
    return static_cast<std::uint8_t>((value * 255u) / maxSrc);
}

std::uint8_t remap10To8(std::uint32_t value) noexcept
{
    const std::uint32_t v16 = (value * 65535u) / 1023u;
    return static_cast<std::uint8_t>((v16 * 255u) / 65535u);
}

float halfToFloat(std::uint16_t h) noexcept
{
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    std::uint32_t exp = (h >> 10) & 0x1Fu;
    std::uint32_t mant = h & 0x03FFu;

    std::uint32_t bits = 0;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            // Normalize subnormal FP16 into normal IEEE-754 FP32.
            exp = 127u - 15u + 1u;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x03FFu;
            bits = sign | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        bits = sign | (0xFFu << 23) | (mant << 13);
    } else {
        bits = sign | ((exp + 127u - 15u) << 23) | (mant << 13);
    }
    return std::bit_cast<float>(bits);
}

std::uint8_t floatToU8(float v) noexcept
{
    if (std::isnan(v)) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}

void decodeBcColorBlock(const std::byte* block, std::uint8_t* dstRgba, int dstPitchBytes, BcColorMode mode) noexcept
{
    const std::uint16_t c0 = readU16Le(block);
    const std::uint16_t c1 = readU16Le(block + 2);

    const std::uint32_t r0 = (c0 >> 11) & 0x1Fu;
    const std::uint32_t g0 = (c0 >> 5) & 0x3Fu;
    const std::uint32_t b0 = c0 & 0x1Fu;

    const std::uint32_t r1 = (c1 >> 11) & 0x1Fu;
    const std::uint32_t g1 = (c1 >> 5) & 0x3Fu;
    const std::uint32_t b1 = c1 & 0x1Fu;

    std::array<std::array<std::uint8_t, 4>, 4> palette{};
    palette[0] = {
        static_cast<std::uint8_t>((r0 * 527u + 23u) >> 6),
        static_cast<std::uint8_t>((g0 * 259u + 33u) >> 6),
        static_cast<std::uint8_t>((b0 * 527u + 23u) >> 6),
        255u,
    };
    palette[1] = {
        static_cast<std::uint8_t>((r1 * 527u + 23u) >> 6),
        static_cast<std::uint8_t>((g1 * 259u + 33u) >> 6),
        static_cast<std::uint8_t>((b1 * 527u + 23u) >> 6),
        255u,
    };

    if (c0 > c1 || mode == BcColorMode::FourColorOnly) {
        palette[2] = {
            static_cast<std::uint8_t>(((2u * r0 + r1) * 351u + 61u) >> 7),
            static_cast<std::uint8_t>(((2u * g0 + g1) * 2763u + 1039u) >> 11),
            static_cast<std::uint8_t>(((2u * b0 + b1) * 351u + 61u) >> 7),
            255u,
        };
        palette[3] = {
            static_cast<std::uint8_t>(((r0 + 2u * r1) * 351u + 61u) >> 7),
            static_cast<std::uint8_t>(((g0 + 2u * g1) * 2763u + 1039u) >> 11),
            static_cast<std::uint8_t>(((b0 + 2u * b1) * 351u + 61u) >> 7),
            255u,
        };
    } else {
        palette[2] = {
            static_cast<std::uint8_t>(((r0 + r1) * 1053u + 125u) >> 8),
            static_cast<std::uint8_t>(((g0 + g1) * 4145u + 1019u) >> 11),
            static_cast<std::uint8_t>(((b0 + b1) * 1053u + 125u) >> 8),
            255u,
        };
        palette[3] = {
            0u,
            0u,
            0u,
            mode == BcColorMode::Dxt1OneBitAlpha ? static_cast<std::uint8_t>(0u) : static_cast<std::uint8_t>(255u),
        };
    }

    std::uint32_t indices = readU32Le(block + 4);
    for (int py = 0; py < 4; ++py) {
        std::uint8_t* row = dstRgba + py * dstPitchBytes;
        for (int px = 0; px < 4; ++px) {
            const auto& c = palette[indices & 0x03u];
            row[px * 4 + 0] = c[0];
            row[px * 4 + 1] = c[1];
            row[px * 4 + 2] = c[2];
            row[px * 4 + 3] = c[3];
            indices >>= 2;
        }
    }
}

void decodeBcSharpAlphaBlock(const std::byte* block, std::uint8_t* dstAlpha, int dstPitchBytes) noexcept
{
    for (int py = 0; py < 4; ++py) {
        const std::uint16_t rowBits = readU16Le(block + py * 2);
        std::uint8_t* row = dstAlpha + py * dstPitchBytes;
        for (int px = 0; px < 4; ++px) {
            row[px * 4] = static_cast<std::uint8_t>(((rowBits >> (4 * px)) & 0x0Fu) * 17u);
        }
    }
}

void decodeBcSmoothAlphaBlock(const std::byte* block, std::uint8_t* dstChannel, int dstPitchBytes, int pixelStrideBytes) noexcept
{
    const std::uint64_t bits = readU64Le(block);
    std::array<std::uint8_t, 8> table{};
    table[0] = static_cast<std::uint8_t>(bits & 0xFFu);
    table[1] = static_cast<std::uint8_t>((bits >> 8) & 0xFFu);

    const std::uint32_t a0 = table[0];
    const std::uint32_t a1 = table[1];
    if (a0 > a1) {
        table[2] = static_cast<std::uint8_t>((6u * a0 + 1u * a1) / 7u);
        table[3] = static_cast<std::uint8_t>((5u * a0 + 2u * a1) / 7u);
        table[4] = static_cast<std::uint8_t>((4u * a0 + 3u * a1) / 7u);
        table[5] = static_cast<std::uint8_t>((3u * a0 + 4u * a1) / 7u);
        table[6] = static_cast<std::uint8_t>((2u * a0 + 5u * a1) / 7u);
        table[7] = static_cast<std::uint8_t>((1u * a0 + 6u * a1) / 7u);
    } else {
        table[2] = static_cast<std::uint8_t>((4u * a0 + 1u * a1) / 5u);
        table[3] = static_cast<std::uint8_t>((3u * a0 + 2u * a1) / 5u);
        table[4] = static_cast<std::uint8_t>((2u * a0 + 3u * a1) / 5u);
        table[5] = static_cast<std::uint8_t>((1u * a0 + 4u * a1) / 5u);
        table[6] = 0u;
        table[7] = 255u;
    }

    std::uint64_t indices = bits >> 16;
    for (int py = 0; py < 4; ++py) {
        std::uint8_t* row = dstChannel + py * dstPitchBytes;
        for (int px = 0; px < 4; ++px) {
            row[px * pixelStrideBytes] = table[indices & 0x07u];
            indices >>= 3;
        }
    }
}

void copyBlockToImage(
    const std::array<std::uint8_t, 4 * 4 * 4>& blockRgba,
    int bx,
    int by,
    int width,
    int height,
    QImage& outImage) noexcept
{
    const int baseX = bx * 4;
    const int baseY = by * 4;
    const int copyW = std::min(4, width - baseX);
    const int copyH = std::min(4, height - baseY);
    for (int py = 0; py < copyH; ++py) {
        std::uint8_t* dst = outImage.scanLine(baseY + py) + baseX * 4;
        std::memcpy(dst, blockRgba.data() + py * 16, static_cast<std::size_t>(copyW) * 4u);
    }
}

VtfHeaderInfo toPublicHeaderInfo(const DecodedVtfHeader& h) noexcept
{
    VtfHeaderInfo info;
    info.versionMajor = h.versionMajor;
    info.versionMinor = h.versionMinor;
    info.width = h.width;
    info.height = h.height;
    info.depth = h.depth;
    info.frameCount = h.frameCount;
    info.faceCount = h.faceCount;
    info.mipCount = h.mipCount;
    info.flags = h.flags;
    info.format = h.format;
    info.bumpmapScale = h.bumpmapScale;
    info.reflectivity = h.reflectivity;
    info.isHdr = isFloatingPointFormat(h.format, h.isCompressedHdrBgra, h.isCompressedHdrRgba16);
    info.hasAlphaChannel = (!h.isCompressedHdrBgra && !h.isCompressedHdrRgba16)
        && (formatHasAlpha(h.format) || (h.flags & (kFlagOneBitAlpha | kFlagMultiBitAlpha)) != 0u);
    info.hasSheetResource = h.hasSheet && h.sheetSize >= 4u;
    return info;
}

Core::Result<DecodedVtfHeader> parseVtfHeaderFromSpan(
    std::span<const std::byte> headerBytes,
    std::uint64_t totalFileSize,
    const QString& sourceLabel,
    bool treatAsCompressedHdr)
{
    const QString detailLabel = sourceLabel.isEmpty() ? QStringLiteral("<memory>") : sourceLabel;
    if (totalFileSize < kMinVtfHeaderSize || headerBytes.size() < kMinVtfHeaderSize) {
        return Core::Result<DecodedVtfHeader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF file is too small to contain a valid header"),
            detailLabel);
    }

    const std::byte* h = headerBytes.data();
    const std::uint32_t signature = readU32Le(h + 0);
    if (signature != kVtfSignature) {
        return Core::Result<DecodedVtfHeader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "Invalid VTF file signature"),
            detailLabel);
    }

    const std::uint32_t versionMajor = readU32Le(h + 4);
    const std::uint32_t versionMinor = readU32Le(h + 8);
    if (versionMajor != kPcVersionMajor || versionMinor > kMaxSupportedMinor) {
        return Core::Result<DecodedVtfHeader>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("VtfCodec", "Unsupported VTF version"),
            QStringLiteral("%1 (v%2.%3)").arg(detailLabel).arg(versionMajor).arg(versionMinor));
    }

    const std::uint32_t headerSize = readU32Le(h + 12);
    const std::uint32_t minVersionHeader = versionMinor >= 3 ? kV73HeaderBaseSize : kMinVtfHeaderSize;
    if (headerSize < minVersionHeader
        || static_cast<std::uint64_t>(headerSize) > totalFileSize
        || headerSize > headerBytes.size()) {
        return Core::Result<DecodedVtfHeader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "Invalid VTF header size"),
            detailLabel);
    }

    DecodedVtfHeader out;
    out.versionMajor = versionMajor;
    out.versionMinor = versionMinor;
    out.headerSize = headerSize;
    out.width = readU16Le(h + 16);
    out.height = readU16Le(h + 18);
    out.flags = readU32Le(h + 20);
    out.frameCount = std::max<std::uint16_t>(readU16Le(h + 24), 1);
    out.reflectivity = {readF32Le(h + 32), readF32Le(h + 36), readF32Le(h + 40)};
    out.bumpmapScale = readF32Le(h + 48);
    const std::int32_t rawFormat = readI32Le(h + 52);
    out.mipCount = readU8(h + 56);
    const std::int32_t lowResFormat = readI32Le(h + 57);
    const std::uint8_t lowResWidth = readU8(h + 61);
    const std::uint8_t lowResHeight = readU8(h + 62);

    if (out.width == 0 || out.height == 0 || out.mipCount == 0) {
        return Core::Result<DecodedVtfHeader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF image dimensions or mip count are zero"),
            detailLabel);
    }

    if (versionMinor >= 2) {
        out.depth = std::max<std::uint16_t>(readU16Le(h + 63), 1);
    }

    // Apply post-header engine compatibility fixups for DXT1 alpha flags and v7.2-v7.4 enum shift.
    std::int32_t fixedFormat = rawFormat;
    if (fixedFormat == static_cast<std::int32_t>(VtfImageFormat::DXT1)
        && (out.flags & (kFlagOneBitAlpha | kFlagMultiBitAlpha)) != 0) {
        fixedFormat = static_cast<std::int32_t>(VtfImageFormat::DXT1_ONE_BIT_ALPHA);
    }
    if (versionMinor < 5 && (fixedFormat == 36 || fixedFormat == 37 || fixedFormat == 38)) {
        fixedFormat -= 3;
    }
    out.format = static_cast<VtfImageFormat>(fixedFormat);

    const QString lowerName = sourceLabel.toLower();
    if (treatAsCompressedHdr || lowerName.endsWith(QLatin1String(".hdr.vtf"))) {
        if (out.format == VtfImageFormat::BGRA8888) {
            out.isCompressedHdrBgra = true;
        } else if (out.format == VtfImageFormat::RGBA16161616) {
            out.isCompressedHdrRgba16 = true;
        }
    }

    if ((out.flags & kFlagNoMip) != 0 && out.mipCount > 1) {
        out.flags &= ~kFlagNoMip; // HL2 broken Borealis skybox fixup
    }

    if (formatBpp(out.format) == 0) {
        const auto code = out.format == VtfImageFormat::EMPTY
            ? Core::Error::ErrorCode::InvalidFile
            : Core::Error::ErrorCode::NotSupported;
        return Core::Result<DecodedVtfHeader>::failure(
            code,
            QCoreApplication::translate("VtfCodec", "Unsupported VTF pixel format"),
            QStringLiteral("%1 (format=%2)").arg(detailLabel).arg(fixedFormat));
    }

    if (versionMinor < 3) {
        const std::uint64_t lowResBytes = (lowResWidth > 0 && lowResHeight > 0 && lowResFormat == 13)
            ? ((static_cast<std::uint64_t>(lowResWidth) + 3u) / 4u)
                * ((static_cast<std::uint64_t>(lowResHeight) + 3u) / 4u)
                * 8u
            : 0u;
        out.imageDataOffset = static_cast<std::uint64_t>(headerSize) + lowResBytes;
        if (out.imageDataOffset > totalFileSize) {
            return Core::Result<DecodedVtfHeader>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF image data offset exceeds file size"),
                detailLabel);
        }
        out.imageResourceSize = totalFileSize - out.imageDataOffset;
    } else {
        const std::uint32_t resourceCount = readU32Le(h + 68);
        if (static_cast<std::uint64_t>(kV73HeaderBaseSize) + static_cast<std::uint64_t>(resourceCount) * 8u > headerSize) {
            return Core::Result<DecodedVtfHeader>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "VTF resource directory exceeds header size"),
                detailLabel);
        }

        std::vector<VtfResourceEntry> nonLocalResources;
        nonLocalResources.reserve(resourceCount);
        for (std::uint32_t i = 0; i < resourceCount; ++i) {
            const std::byte* entryPtr = h + kV73HeaderBaseSize + static_cast<std::size_t>(i) * 8u;
            const std::uint32_t typeAndFlags = readU32Le(entryPtr);
            VtfResourceEntry entry;
            entry.type = typeAndFlags & 0x00FFFFFFu;
            entry.flags = static_cast<std::uint8_t>((typeAndFlags >> 24) & 0xFFu);
            entry.offsetOrData = readU32Le(entryPtr + 4);
            if ((entry.flags & kResourceFlagLocalData) == 0) {
                if (static_cast<std::uint64_t>(entry.offsetOrData) > totalFileSize) {
                    return Core::Result<DecodedVtfHeader>::failure(
                        Core::Error::ErrorCode::CorruptedData,
                        QCoreApplication::translate("VtfCodec", "VTF resource offset exceeds file size"),
                        detailLabel);
                }
                nonLocalResources.push_back(entry);
            }
        }

        std::stable_sort(
            nonLocalResources.begin(),
            nonLocalResources.end(),
            [](const VtfResourceEntry& lhs, const VtfResourceEntry& rhs) {
                return lhs.offsetOrData < rhs.offsetOrData;
            });

        for (std::size_t i = 0; i < nonLocalResources.size(); ++i) {
            const std::uint64_t curOffset = nonLocalResources[i].offsetOrData;
            const std::uint64_t nextOffset = (i + 1 < nonLocalResources.size())
                ? static_cast<std::uint64_t>(nonLocalResources[i + 1].offsetOrData)
                : totalFileSize;
            nonLocalResources[i].size = nextOffset >= curOffset ? (nextOffset - curOffset) : 0u;

            if (nonLocalResources[i].type == kResourceTypeImageData) {
                out.imageDataOffset = curOffset;
                out.imageResourceSize = nonLocalResources[i].size;
            } else if (nonLocalResources[i].type == kResourceTypeAuxCompression) {
                out.hasAxc = true;
                out.axcOffset = curOffset;
                out.axcSize = nonLocalResources[i].size;
            } else if (nonLocalResources[i].type == kResourceTypeSheet) {
                out.hasSheet = true;
                out.sheetOffset = curOffset;
                out.sheetSize = nonLocalResources[i].size;
            }
        }

        if (out.imageDataOffset == 0 || out.imageResourceSize == 0) {
            return Core::Result<DecodedVtfHeader>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "VTF file contains no image data resource"),
                detailLabel);
        }
    }

    if ((out.flags & kFlagEnvMap) == 0) {
        out.faceCount = 1;
    } else if (out.versionMinor >= 6) {
        out.faceCount = 6;
    } else {
        const auto expected6 = calcAllMipsBytes(
            out.format, out.mipCount, out.frameCount, 6, out.width, out.height, out.depth);
        if (!expected6.has_value()) {
            return Core::Result<DecodedVtfHeader>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "VTF image dimensions exceed safe decoding limits"),
                detailLabel);
        }
        if (out.versionMinor == 5) {
            out.faceCount = out.imageResourceSize >= *expected6 ? 6 : 1;
        } else if (out.versionMinor >= 1 && *expected6 < out.imageResourceSize) {
            out.faceCount = 7;
        } else if (*expected6 == out.imageResourceSize) {
            out.faceCount = 6;
        } else {
            out.faceCount = 1;
        }
    }

    return Core::Result<DecodedVtfHeader>::success(out);
}

struct ExtractedSlice {
    std::span<const std::byte> spanView;
    std::vector<std::byte> ownedBuffer;

    std::span<const std::byte> bytes() const noexcept {
        return ownedBuffer.empty() ? spanView : std::span<const std::byte>(ownedBuffer);
    }
};

Core::Result<ExtractedSlice> extractMip0SliceBytes(
    std::span<const std::byte> data,
    const DecodedVtfHeader& header,
    const VtfSliceSelector& slice,
    const QString& sourceLabel)
{
    const QString detailLabel = sourceLabel.isEmpty() ? QStringLiteral("<memory>") : sourceLabel;
    if (slice.frame >= header.frameCount || slice.face >= header.faceCount || slice.depthSlice >= header.depth) {
        return Core::Result<ExtractedSlice>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("VtfCodec", "Requested VTF slice index is out of range"),
            QStringLiteral("%1 (frame=%2/%3, face=%4/%5, depth=%6/%7)")
                .arg(detailLabel)
                .arg(slice.frame).arg(header.frameCount)
                .arg(slice.face).arg(header.faceCount)
                .arg(slice.depthSlice).arg(header.depth));
    }

    const std::uint64_t outputRgbaBytes =
        static_cast<std::uint64_t>(header.width) * static_cast<std::uint64_t>(header.height) * 4ull;
    const std::uint64_t mip0SliceBytes = calcSliceBytes(header.format, header.width, header.height);
    if (mip0SliceBytes == 0 || mip0SliceBytes > kMaxDecodeBufferBytes || outputRgbaBytes > kMaxDecodeBufferBytes) {
        return Core::Result<ExtractedSlice>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF mip 0 dimensions exceed safe decoding limits"),
            detailLabel);
    }

    // Check v7.6 AXC auxiliary compression first.
    if (header.versionMinor >= 6 && header.hasAxc && header.axcSize >= 8) {
        if (header.axcSize > 16ull * 1024ull * 1024ull) {
            return Core::Result<ExtractedSlice>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF AXC resource table is unexpectedly large"),
                detailLabel);
        }
        if (header.axcOffset + header.axcSize > data.size()) {
            return Core::Result<ExtractedSlice>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF AXC compression table is truncated"),
                detailLabel);
        }

        const std::byte* axcPtr = data.data() + header.axcOffset;
        const std::uint32_t levelAndMethod = readU32Le(axcPtr + 4);
        const std::uint16_t compressionLevel = static_cast<std::uint16_t>(levelAndMethod & 0xFFFFu);
        std::int16_t compressionMethod = static_cast<std::int16_t>((levelAndMethod >> 16) & 0xFFFFu);
        if (compressionMethod <= 0) {
            compressionMethod = kCompressionDeflate;
        }

        if (compressionLevel != 0) {
            if (compressionMethod == kCompressionZstd) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::NotSupported,
                    QCoreApplication::translate("VtfCodec", "Zstd-compressed VTF textures are not supported"),
                    detailLabel);
            }
            if (compressionMethod != kCompressionDeflate) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::NotSupported,
                    QCoreApplication::translate("VtfCodec", "Unsupported VTF auxiliary compression method"),
                    QStringLiteral("%1 (method=%2)").arg(detailLabel).arg(compressionMethod));
            }

            const std::size_t totalEntries = static_cast<std::size_t>(header.mipCount)
                * static_cast<std::size_t>(header.frameCount)
                * static_cast<std::size_t>(header.faceCount);
            if (header.axcSize < (2u + totalEntries) * sizeof(std::uint32_t)) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("VtfCodec", "VTF AXC compression table is truncated"),
                    detailLabel);
            }

            // AXC order: (mip = mipCount - 1 .. 0) x (frame = 0 .. frames - 1) x (face = 0 .. faces - 1).
            const std::size_t mip0BaseIndex = static_cast<std::size_t>(header.mipCount - 1)
                * static_cast<std::size_t>(header.frameCount)
                * static_cast<std::size_t>(header.faceCount);
            const std::size_t targetIndex = mip0BaseIndex
                + static_cast<std::size_t>(slice.frame) * static_cast<std::size_t>(header.faceCount)
                + static_cast<std::size_t>(slice.face);

            std::uint64_t compressedOffset = 0;
            for (std::size_t i = 0; i < targetIndex; ++i) {
                if (!checkedAdd(compressedOffset, readU32Le(axcPtr + (2u + i) * sizeof(std::uint32_t)), compressedOffset)) {
                    return Core::Result<ExtractedSlice>::failure(
                        Core::Error::ErrorCode::CorruptedData,
                        QCoreApplication::translate("VtfCodec", "VTF AXC compressed image block is out of bounds"),
                        detailLabel);
                }
            }
            const std::uint32_t compressedLen = readU32Le(axcPtr + (2u + targetIndex) * sizeof(std::uint32_t));

            std::uint64_t compressedEndInRes = 0;
            std::uint64_t compressedAbsEnd = 0;
            if (compressedLen == 0
                || !checkedAdd(compressedOffset, compressedLen, compressedEndInRes)
                || compressedEndInRes > header.imageResourceSize
                || !checkedAdd(header.imageDataOffset, compressedEndInRes, compressedAbsEnd)
                || compressedAbsEnd > data.size()) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("VtfCodec", "VTF AXC compressed image block is out of bounds"),
                    detailLabel);
            }

            std::uint64_t expectedDecompressedBytes = 0;
            if (!checkedMul(mip0SliceBytes, header.depth, expectedDecompressedBytes)
                || expectedDecompressedBytes > kMaxDecodeBufferBytes) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::InvalidFile,
                    QCoreApplication::translate("VtfCodec", "VTF mip 0 volume exceeds safe decoding limits"),
                    detailLabel);
            }

            std::vector<std::byte> decompressedBuf(static_cast<std::size_t>(expectedDecompressedBytes));
            InflateStreamGuard guard;
            guard.strm.next_in = reinterpret_cast<Bytef*>(
                const_cast<std::byte*>(data.data() + header.imageDataOffset + compressedOffset));
            guard.strm.avail_in = static_cast<uInt>(compressedLen);
            guard.strm.next_out = reinterpret_cast<Bytef*>(decompressedBuf.data());
            guard.strm.avail_out = static_cast<uInt>(decompressedBuf.size());

            if (inflateInit2(&guard.strm, MAX_WBITS) != Z_OK) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("VtfCodec", "Failed to initialize zlib decoder for VTF"),
                    detailLabel);
            }
            guard.initialized = true;

            const int inflateStatus = inflate(&guard.strm, Z_FINISH);
            if (inflateStatus != Z_STREAM_END || guard.strm.total_out != expectedDecompressedBytes) {
                return Core::Result<ExtractedSlice>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("VtfCodec", "Failed to decompress VTF AXC image data"),
                    detailLabel);
            }

            ExtractedSlice out;
            const std::size_t depthByteOffset = static_cast<std::size_t>(slice.depthSlice) * static_cast<std::size_t>(mip0SliceBytes);
            if (slice.depthSlice == 0) {
                decompressedBuf.resize(static_cast<std::size_t>(mip0SliceBytes));
                out.ownedBuffer = std::move(decompressedBuf);
            } else {
                out.ownedBuffer.assign(
                    decompressedBuf.begin() + static_cast<std::ptrdiff_t>(depthByteOffset),
                    decompressedBuf.begin() + static_cast<std::ptrdiff_t>(depthByteOffset + mip0SliceBytes));
            }
            return Core::Result<ExtractedSlice>::success(std::move(out));
        }
    }

    // Uncompressed VTF image resource: skip mips (mipCount - 1 .. 1), then address (frame -> face -> depthSlice) within Mip 0.
    std::uint64_t frameFace = 0;
    if (!checkedMul(header.frameCount, header.faceCount, frameFace)) {
        return Core::Result<ExtractedSlice>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VtfCodec", "VTF image data is truncated or corrupted"),
            detailLabel);
    }

    std::uint64_t skipBytes = 0;
    for (int m = static_cast<int>(header.mipCount) - 1; m >= 1; --m) {
        const std::uint16_t mw = mipDimension(static_cast<std::uint8_t>(m), header.width);
        const std::uint16_t mh = mipDimension(static_cast<std::uint8_t>(m), header.height);
        const std::uint16_t md = mipDimension(static_cast<std::uint8_t>(m), header.depth);
        std::uint64_t count = 0;
        std::uint64_t mipBytes = 0;
        if (!checkedMul(frameFace, md, count)
            || !checkedMul(calcSliceBytes(header.format, mw, mh), count, mipBytes)
            || !checkedAdd(skipBytes, mipBytes, skipBytes)) {
            return Core::Result<ExtractedSlice>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF image data is truncated or corrupted"),
                detailLabel);
        }
    }

    const std::uint64_t frameFaceIndex =
        static_cast<std::uint64_t>(slice.frame) * static_cast<std::uint64_t>(header.faceCount)
        + static_cast<std::uint64_t>(slice.face);
    std::uint64_t sliceLinearIndex = 0;
    std::uint64_t sliceByteOffset = 0;
    std::uint64_t sliceOffsetInResource = 0;
    std::uint64_t totalNeededInResource = 0;
    std::uint64_t absoluteEnd = 0;
    if (!checkedMul(frameFaceIndex, header.depth, sliceLinearIndex)
        || !checkedAdd(sliceLinearIndex, slice.depthSlice, sliceLinearIndex)
        || !checkedMul(sliceLinearIndex, mip0SliceBytes, sliceByteOffset)
        || !checkedAdd(skipBytes, sliceByteOffset, sliceOffsetInResource)
        || !checkedAdd(sliceOffsetInResource, mip0SliceBytes, totalNeededInResource)
        || totalNeededInResource > header.imageResourceSize
        || !checkedAdd(header.imageDataOffset, totalNeededInResource, absoluteEnd)
        || absoluteEnd > data.size()) {
        return Core::Result<ExtractedSlice>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VtfCodec", "VTF image data is truncated or corrupted"),
            detailLabel);
    }

    ExtractedSlice out;
    out.spanView = data.subspan(
        static_cast<std::size_t>(header.imageDataOffset + sliceOffsetInResource),
        static_cast<std::size_t>(mip0SliceBytes));
    return Core::Result<ExtractedSlice>::success(std::move(out));
}

void decodeMip0ToFloatBuffer(
    std::span<const std::byte> mip0,
    const DecodedVtfHeader& header,
    std::vector<float>& outRgbaFloat)
{
    const std::size_t pixelCount = static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height);
    outRgbaFloat.resize(pixelCount * 4u);
    const std::byte* src = mip0.data();

    if (header.isCompressedHdrBgra) {
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::byte* p = src + i * 4u;
            const float alpha = static_cast<float>(readU8(p + 3)) / 255.0f;
            outRgbaFloat[i * 4u + 0] = (static_cast<float>(readU8(p + 2)) / 255.0f) * alpha * 16.0f;
            outRgbaFloat[i * 4u + 1] = (static_cast<float>(readU8(p + 1)) / 255.0f) * alpha * 16.0f;
            outRgbaFloat[i * 4u + 2] = (static_cast<float>(readU8(p + 0)) / 255.0f) * alpha * 16.0f;
            outRgbaFloat[i * 4u + 3] = 1.0f;
        }
        return;
    }

    if (header.isCompressedHdrRgba16) {
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::byte* p = src + i * 8u;
            outRgbaFloat[i * 4u + 0] = static_cast<float>(readU16Le(p + 0)) / 4096.0f;
            outRgbaFloat[i * 4u + 1] = static_cast<float>(readU16Le(p + 2)) / 4096.0f;
            outRgbaFloat[i * 4u + 2] = static_cast<float>(readU16Le(p + 4)) / 4096.0f;
            outRgbaFloat[i * 4u + 3] = 1.0f;
        }
        return;
    }

    switch (header.format) {
        case VtfImageFormat::RGBA16161616F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 8u;
                outRgbaFloat[i * 4u + 0] = halfToFloat(readU16Le(p + 0));
                outRgbaFloat[i * 4u + 1] = halfToFloat(readU16Le(p + 2));
                outRgbaFloat[i * 4u + 2] = halfToFloat(readU16Le(p + 4));
                outRgbaFloat[i * 4u + 3] = halfToFloat(readU16Le(p + 6));
            }
            break;
        case VtfImageFormat::RG1616F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 4u;
                outRgbaFloat[i * 4u + 0] = halfToFloat(readU16Le(p + 0));
                outRgbaFloat[i * 4u + 1] = halfToFloat(readU16Le(p + 2));
                outRgbaFloat[i * 4u + 2] = 0.0f;
                outRgbaFloat[i * 4u + 3] = 1.0f;
            }
            break;
        case VtfImageFormat::R16F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 2u;
                outRgbaFloat[i * 4u + 0] = halfToFloat(readU16Le(p));
                outRgbaFloat[i * 4u + 1] = 0.0f;
                outRgbaFloat[i * 4u + 2] = 0.0f;
                outRgbaFloat[i * 4u + 3] = 1.0f;
            }
            break;
        case VtfImageFormat::R32F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 4u;
                outRgbaFloat[i * 4u + 0] = readF32Le(p);
                outRgbaFloat[i * 4u + 1] = 0.0f;
                outRgbaFloat[i * 4u + 2] = 0.0f;
                outRgbaFloat[i * 4u + 3] = 1.0f;
            }
            break;
        case VtfImageFormat::RG3232F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 8u;
                outRgbaFloat[i * 4u + 0] = readF32Le(p + 0);
                outRgbaFloat[i * 4u + 1] = readF32Le(p + 4);
                outRgbaFloat[i * 4u + 2] = 0.0f;
                outRgbaFloat[i * 4u + 3] = 1.0f;
            }
            break;
        case VtfImageFormat::RGB323232F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 12u;
                outRgbaFloat[i * 4u + 0] = readF32Le(p + 0);
                outRgbaFloat[i * 4u + 1] = readF32Le(p + 4);
                outRgbaFloat[i * 4u + 2] = readF32Le(p + 8);
                outRgbaFloat[i * 4u + 3] = 1.0f;
            }
            break;
        case VtfImageFormat::RGBA32323232F:
            for (std::size_t i = 0; i < pixelCount; ++i) {
                const std::byte* p = src + i * 16u;
                outRgbaFloat[i * 4u + 0] = readF32Le(p + 0);
                outRgbaFloat[i * 4u + 1] = readF32Le(p + 4);
                outRgbaFloat[i * 4u + 2] = readF32Le(p + 8);
                outRgbaFloat[i * 4u + 3] = readF32Le(p + 12);
            }
            break;
        default:
            break;
    }
}

void decodeMip0ToQImage(
    std::span<const std::byte> mip0,
    const DecodedVtfHeader& header,
    QImage& outImage)
{
    const int width = static_cast<int>(header.width);
    const int height = static_cast<int>(header.height);
    outImage = QImage(width, height, QImage::Format_RGBA8888);

    if (isFloatingPointFormat(header.format, header.isCompressedHdrBgra, header.isCompressedHdrRgba16)) {
        std::vector<float> floatRgba;
        decodeMip0ToFloatBuffer(mip0, header, floatRgba);
        for (int y = 0; y < height; ++y) {
            std::uint8_t* dst = outImage.scanLine(y);
            const float* row = floatRgba.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4u;
            for (int x = 0; x < width; ++x) {
                dst[x * 4 + 0] = floatToU8(row[x * 4 + 0]);
                dst[x * 4 + 1] = floatToU8(row[x * 4 + 1]);
                dst[x * 4 + 2] = floatToU8(row[x * 4 + 2]);
                dst[x * 4 + 3] = floatToU8(row[x * 4 + 3]);
            }
        }
        return;
    }

    if (isBlockCompressed(header.format)) {
        const int blocksX = (width + 3) / 4;
        const int blocksY = (height + 3) / 4;
        const std::size_t blockBytes = formatBpp(header.format) * 2u;
        const std::byte* src = mip0.data();

        std::array<std::uint8_t, 4 * 4 * 4> blockRgba{};
        std::array<std::uint8_t, 4 * 4 * 2> bc5Uv{};

        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                const std::byte* blockPtr = src + (static_cast<std::size_t>(by) * blocksX + bx) * blockBytes;
                switch (header.format) {
                    case VtfImageFormat::DXT1:
                        decodeBcColorBlock(blockPtr, blockRgba.data(), 16, BcColorMode::Dxt1Opaque);
                        break;
                    case VtfImageFormat::DXT1_ONE_BIT_ALPHA:
                        decodeBcColorBlock(blockPtr, blockRgba.data(), 16, BcColorMode::Dxt1OneBitAlpha);
                        break;
                    case VtfImageFormat::DXT3:
                        decodeBcColorBlock(blockPtr + 8, blockRgba.data(), 16, BcColorMode::FourColorOnly);
                        decodeBcSharpAlphaBlock(blockPtr, blockRgba.data() + 3, 16);
                        break;
                    case VtfImageFormat::DXT5:
                        decodeBcColorBlock(blockPtr + 8, blockRgba.data(), 16, BcColorMode::FourColorOnly);
                        decodeBcSmoothAlphaBlock(blockPtr, blockRgba.data() + 3, 16, 4);
                        break;
                    case VtfImageFormat::ATI1N: {
                        std::array<std::uint8_t, 16> gray{};
                        decodeBcSmoothAlphaBlock(blockPtr, gray.data(), 4, 1);
                        for (int i = 0; i < 16; ++i) {
                            blockRgba[i * 4 + 0] = gray[i];
                            blockRgba[i * 4 + 1] = gray[i];
                            blockRgba[i * 4 + 2] = gray[i];
                            blockRgba[i * 4 + 3] = 255u;
                        }
                        break;
                    }
                    case VtfImageFormat::ATI2N: {
                        // Valve ATI2N stores G (V) in the first block and R (U) in the second block.
                        decodeBcSmoothAlphaBlock(blockPtr, bc5Uv.data() + 1, 8, 2);
                        decodeBcSmoothAlphaBlock(blockPtr + 8, bc5Uv.data(), 8, 2);
                        for (int i = 0; i < 16; ++i) {
                            const std::uint8_t u = bc5Uv[i * 2 + 0];
                            const std::uint8_t v = bc5Uv[i * 2 + 1];
                            const float nX = static_cast<float>(u) / 255.0f * 2.0f - 1.0f;
                            const float nY = static_cast<float>(v) / 255.0f * 2.0f - 1.0f;
                            const float nZ = std::sqrt(std::max(0.0f, 1.0f - (nX * nX) - (nY * nY)));
                            blockRgba[i * 4 + 0] = u;
                            blockRgba[i * 4 + 1] = v;
                            blockRgba[i * 4 + 2] = static_cast<std::uint8_t>(std::clamp(nZ, 0.0f, 1.0f) * 255.0f);
                            blockRgba[i * 4 + 3] = 255u;
                        }
                        break;
                    }
                    default:
                        break;
                }
                copyBlockToImage(blockRgba, bx, by, width, height, outImage);
            }
        }
        return;
    }

    const std::size_t bytesPerPixel = formatBpp(header.format) / 8u;
    for (int y = 0; y < height; ++y) {
        std::uint8_t* dst = outImage.scanLine(y);
        const std::byte* row = mip0.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * bytesPerPixel;
        for (int x = 0; x < width; ++x) {
            const std::byte* p = row + static_cast<std::size_t>(x) * bytesPerPixel;
            std::uint8_t* d = dst + x * 4;
            switch (header.format) {
                case VtfImageFormat::RGBA8888:
                case VtfImageFormat::UVWQ8888:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 2);
                    d[3] = readU8(p + 3);
                    break;
                case VtfImageFormat::ABGR8888:
                    d[0] = readU8(p + 3);
                    d[1] = readU8(p + 2);
                    d[2] = readU8(p + 1);
                    d[3] = readU8(p + 0);
                    break;
                case VtfImageFormat::RGB888:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 2);
                    d[3] = 255u;
                    break;
                case VtfImageFormat::BGR888:
                    d[0] = readU8(p + 2);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 0);
                    d[3] = 255u;
                    break;
                case VtfImageFormat::RGB565: {
                    const std::uint16_t v = readU16Le(p);
                    d[0] = remapTo8(v & 0x1Fu, 31u);
                    d[1] = remapTo8((v >> 5) & 0x3Fu, 63u);
                    d[2] = remapTo8((v >> 11) & 0x1Fu, 31u);
                    d[3] = 255u;
                    break;
                }
                case VtfImageFormat::I8:
                case VtfImageFormat::P8: {
                    const std::uint8_t v = readU8(p);
                    d[0] = v;
                    d[1] = v;
                    d[2] = v;
                    d[3] = 255u;
                    break;
                }
                case VtfImageFormat::IA88: {
                    const std::uint8_t lum = readU8(p + 0);
                    d[0] = lum;
                    d[1] = lum;
                    d[2] = lum;
                    d[3] = readU8(p + 1);
                    break;
                }
                case VtfImageFormat::A8:
                    d[0] = 255u;
                    d[1] = 255u;
                    d[2] = 255u;
                    d[3] = readU8(p);
                    break;
                case VtfImageFormat::RGB888_BLUESCREEN:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 2);
                    d[3] = (d[0] == 0u && d[1] == 0u && d[2] == 255u) ? 0u : 255u;
                    break;
                case VtfImageFormat::BGR888_BLUESCREEN:
                    d[0] = readU8(p + 2);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 0);
                    d[3] = (d[0] == 0u && d[1] == 0u && d[2] == 255u) ? 0u : 255u;
                    break;
                case VtfImageFormat::ARGB8888:
                    d[0] = readU8(p + 1);
                    d[1] = readU8(p + 2);
                    d[2] = readU8(p + 3);
                    d[3] = readU8(p + 0);
                    break;
                case VtfImageFormat::BGRA8888:
                    d[0] = readU8(p + 2);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 0);
                    d[3] = readU8(p + 3);
                    break;
                case VtfImageFormat::BGRX8888:
                    d[0] = readU8(p + 2);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 0);
                    d[3] = 255u;
                    break;
                case VtfImageFormat::BGR565: {
                    const std::uint16_t v = readU16Le(p);
                    d[0] = remapTo8((v >> 11) & 0x1Fu, 31u);
                    d[1] = remapTo8((v >> 5) & 0x3Fu, 63u);
                    d[2] = remapTo8(v & 0x1Fu, 31u);
                    d[3] = 255u;
                    break;
                }
                case VtfImageFormat::BGRX5551: {
                    const std::uint16_t v = readU16Le(p);
                    d[0] = remapTo8((v >> 10) & 0x1Fu, 31u);
                    d[1] = remapTo8((v >> 5) & 0x1Fu, 31u);
                    d[2] = remapTo8(v & 0x1Fu, 31u);
                    d[3] = 255u;
                    break;
                }
                case VtfImageFormat::BGRA4444: {
                    const std::uint16_t v = readU16Le(p);
                    d[0] = static_cast<std::uint8_t>(((v >> 8) & 0x0Fu) * 17u);
                    d[1] = static_cast<std::uint8_t>(((v >> 4) & 0x0Fu) * 17u);
                    d[2] = static_cast<std::uint8_t>((v & 0x0Fu) * 17u);
                    d[3] = static_cast<std::uint8_t>(((v >> 12) & 0x0Fu) * 17u);
                    break;
                }
                case VtfImageFormat::BGRA5551: {
                    const std::uint16_t v = readU16Le(p);
                    d[0] = remapTo8((v >> 10) & 0x1Fu, 31u);
                    d[1] = remapTo8((v >> 5) & 0x1Fu, 31u);
                    d[2] = remapTo8(v & 0x1Fu, 31u);
                    d[3] = (v & 0x8000u) != 0 ? 255u : 0u;
                    break;
                }
                case VtfImageFormat::UV88:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = 0u;
                    d[3] = 255u;
                    break;
                case VtfImageFormat::RGBA16161616:
                    d[0] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(readU16Le(p + 0)) * 255u) / 65535u);
                    d[1] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(readU16Le(p + 2)) * 255u) / 65535u);
                    d[2] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(readU16Le(p + 4)) * 255u) / 65535u);
                    d[3] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(readU16Le(p + 6)) * 255u) / 65535u);
                    break;
                case VtfImageFormat::UVLX8888:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 2);
                    d[3] = 255u;
                    break;
                case VtfImageFormat::RGBX8888:
                    d[0] = readU8(p + 0);
                    d[1] = readU8(p + 1);
                    d[2] = readU8(p + 2);
                    d[3] = 255u;
                    break;
                case VtfImageFormat::RGBA1010102: {
                    const std::uint32_t v = readU32Le(p);
                    d[0] = remap10To8(v & 0x3FFu);
                    d[1] = remap10To8((v >> 10) & 0x3FFu);
                    d[2] = remap10To8((v >> 20) & 0x3FFu);
                    d[3] = static_cast<std::uint8_t>(((v >> 30) & 0x03u) * 85u);
                    break;
                }
                case VtfImageFormat::BGRA1010102: {
                    const std::uint32_t v = readU32Le(p);
                    d[0] = remap10To8((v >> 20) & 0x3FFu);
                    d[1] = remap10To8((v >> 10) & 0x3FFu);
                    d[2] = remap10To8(v & 0x3FFu);
                    d[3] = static_cast<std::uint8_t>(((v >> 30) & 0x03u) * 85u);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

Core::Result<std::vector<std::byte>> readFileBytes(const Core::Path::FilesystemPath& path)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfCodec", "VTF file path is empty or invalid"));
    }
    if (!path.exists() || !path.isFile()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VtfCodec", "VTF file not found"),
            path.toString());
    }

    QFile file(path.toString());
    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VtfCodec", "Failed to open VTF file"),
            path.toString() + QStringLiteral(" | ") + file.errorString());
    }

    const qint64 fileSize = file.size();
    if (fileSize < 0 || fileSize > kMaxVtfFileReadBytes) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF file size exceeds safe reading limits"),
            path.toString());
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(fileSize));
    if (fileSize > 0
        && file.read(reinterpret_cast<char*>(bytes.data()), fileSize) != fileSize) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VtfCodec", "Failed to read VTF file"),
            path.toString() + QStringLiteral(" | ") + file.errorString());
    }

    return Core::Result<std::vector<std::byte>>::success(std::move(bytes));
}

} // namespace

namespace Detail {

Core::Result<std::vector<float>> readVtfMip0RgbaFloat(
    const Core::Path::FilesystemPath& path,
    int& outWidth,
    int& outHeight)
{
    auto bytesRes = readFileBytes(path);
    if (bytesRes.isFailure()) {
        return Core::Result<std::vector<float>>::failure(bytesRes.error());
    }

    const auto& data = bytesRes.value();
    auto headerRes = parseVtfHeaderFromSpan(data, data.size(), path.toString(), false);
    if (headerRes.isFailure()) {
        return Core::Result<std::vector<float>>::failure(headerRes.error());
    }
    const DecodedVtfHeader& header = headerRes.value();

    auto sliceRes = extractMip0SliceBytes(data, header, VtfSliceSelector{}, path.toString());
    if (sliceRes.isFailure()) {
        return Core::Result<std::vector<float>>::failure(sliceRes.error());
    }

    outWidth = static_cast<int>(header.width);
    outHeight = static_cast<int>(header.height);

    std::vector<float> floatRgba;
    if (isFloatingPointFormat(header.format, header.isCompressedHdrBgra, header.isCompressedHdrRgba16)) {
        decodeMip0ToFloatBuffer(sliceRes.value().bytes(), header, floatRgba);
        return Core::Result<std::vector<float>>::success(std::move(floatRgba));
    }

    QImage image;
    decodeMip0ToQImage(sliceRes.value().bytes(), header, image);
    const std::size_t pixelCount = static_cast<std::size_t>(outWidth) * static_cast<std::size_t>(outHeight);
    floatRgba.resize(pixelCount * 4u);
    for (int y = 0; y < outHeight; ++y) {
        const std::uint8_t* src = image.constScanLine(y);
        float* dst = floatRgba.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(outWidth) * 4u;
        for (int x = 0; x < outWidth; ++x) {
            dst[x * 4 + 0] = static_cast<float>(src[x * 4 + 0]) / 255.0f;
            dst[x * 4 + 1] = static_cast<float>(src[x * 4 + 1]) / 255.0f;
            dst[x * 4 + 2] = static_cast<float>(src[x * 4 + 2]) / 255.0f;
            dst[x * 4 + 3] = static_cast<float>(src[x * 4 + 3]) / 255.0f;
        }
    }
    return Core::Result<std::vector<float>>::success(std::move(floatRgba));
}

} // namespace Detail

bool VtfCodec::isVtfExtension(const QString& lowerCaseExtension)
{
    return lowerCaseExtension == QLatin1String("vtf");
}

Core::Result<VtfHeaderInfo> VtfCodec::readHeader(const Core::Path::FilesystemPath& path)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<VtfHeaderInfo>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfCodec", "VTF file path is empty or invalid"));
    }
    if (!path.exists() || !path.isFile()) {
        return Core::Result<VtfHeaderInfo>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VtfCodec", "VTF file not found"),
            path.toString());
    }

    QFile file(path.toString());
    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<VtfHeaderInfo>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VtfCodec", "Failed to open VTF file"),
            path.toString() + QStringLiteral(" | ") + file.errorString());
    }

    const qint64 fileSize = file.size();
    if (fileSize < static_cast<qint64>(kMinVtfHeaderSize)) {
        return Core::Result<VtfHeaderInfo>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF file is too small to contain a valid header"),
            path.toString());
    }

    std::vector<std::byte> headerBuf(static_cast<std::size_t>(std::min<qint64>(fileSize, kV73HeaderBaseSize)));
    if (file.read(reinterpret_cast<char*>(headerBuf.data()), static_cast<qint64>(headerBuf.size()))
        != static_cast<qint64>(headerBuf.size())) {
        return Core::Result<VtfHeaderInfo>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VtfCodec", "Failed to read VTF header"),
            path.toString());
    }

    const std::uint32_t headerSize = readU32Le(headerBuf.data() + 12);
    if (headerSize > headerBuf.size() && headerSize <= kMaxHeaderReadBytes && static_cast<qint64>(headerSize) <= fileSize) {
        headerBuf.resize(headerSize);
        if (!file.seek(0)
            || file.read(reinterpret_cast<char*>(headerBuf.data()), static_cast<qint64>(headerSize))
                != static_cast<qint64>(headerSize)) {
            return Core::Result<VtfHeaderInfo>::failure(
                Core::Error::ErrorCode::ReadFailed,
                QCoreApplication::translate("VtfCodec", "Failed to read VTF header"),
                path.toString());
        }
    }

    auto headerRes = parseVtfHeaderFromSpan(
        headerBuf, static_cast<std::uint64_t>(fileSize), path.toString(), false);
    if (headerRes.isFailure()) {
        return Core::Result<VtfHeaderInfo>::failure(headerRes.error());
    }
    return Core::Result<VtfHeaderInfo>::success(toPublicHeaderInfo(headerRes.value()));
}

Core::Result<VtfHeaderInfo> VtfCodec::readHeaderFromMemory(
    std::span<const std::byte> data,
    const QString& virtualFileName)
{
    auto headerRes = parseVtfHeaderFromSpan(data, data.size(), virtualFileName, false);
    if (headerRes.isFailure()) {
        return Core::Result<VtfHeaderInfo>::failure(headerRes.error());
    }
    return Core::Result<VtfHeaderInfo>::success(toPublicHeaderInfo(headerRes.value()));
}

Core::Result<QImage> VtfCodec::readSlice(
    std::span<const std::byte> data,
    const VtfSliceSelector& slice,
    const QString& virtualFileName,
    VtfHeaderInfo* outHeader)
{
    auto headerRes = parseVtfHeaderFromSpan(data, data.size(), virtualFileName, slice.treatAsCompressedHdr);
    if (headerRes.isFailure()) {
        return Core::Result<QImage>::failure(headerRes.error());
    }
    const DecodedVtfHeader& header = headerRes.value();

    auto sliceRes = extractMip0SliceBytes(data, header, slice, virtualFileName);
    if (sliceRes.isFailure()) {
        return Core::Result<QImage>::failure(sliceRes.error());
    }

    QImage image;
    decodeMip0ToQImage(sliceRes.value().bytes(), header, image);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF image data is missing or has unexpected size"),
            virtualFileName);
    }

    if (outHeader != nullptr) {
        *outHeader = toPublicHeaderInfo(header);
    }
    return Core::Result<QImage>::success(std::move(image));
}

Core::Result<TextureImage> VtfCodec::readFloatSlice(
    std::span<const std::byte> data,
    const VtfSliceSelector& slice,
    const QString& virtualFileName,
    VtfHeaderInfo* outHeader)
{
    auto headerRes = parseVtfHeaderFromSpan(data, data.size(), virtualFileName, slice.treatAsCompressedHdr);
    if (headerRes.isFailure()) {
        return Core::Result<TextureImage>::failure(headerRes.error());
    }
    const DecodedVtfHeader& header = headerRes.value();

    auto sliceRes = extractMip0SliceBytes(data, header, slice, virtualFileName);
    if (sliceRes.isFailure()) {
        return Core::Result<TextureImage>::failure(sliceRes.error());
    }

    const int width = static_cast<int>(header.width);
    const int height = static_cast<int>(header.height);
    TextureImage out(width, height, 4);
    if (!out.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VtfCodec", "VTF image data is missing or has unexpected size"),
            virtualFileName);
    }

    float* p0 = out.planeData(0);
    float* p1 = out.planeData(1);
    float* p2 = out.planeData(2);
    float* p3 = out.planeData(3);

    if (isFloatingPointFormat(header.format, header.isCompressedHdrBgra, header.isCompressedHdrRgba16)) {
        std::vector<float> floatRgba;
        decodeMip0ToFloatBuffer(sliceRes.value().bytes(), header, floatRgba);
        const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            p0[i] = floatRgba[i * 4u + 0];
            p1[i] = floatRgba[i * 4u + 1];
            p2[i] = floatRgba[i * 4u + 2];
            p3[i] = floatRgba[i * 4u + 3];
        }
    } else {
        QImage image;
        decodeMip0ToQImage(sliceRes.value().bytes(), header, image);
        if (image.isNull() || image.width() != width || image.height() != height) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "VTF image data is missing or has unexpected size"),
                virtualFileName);
        }
        for (int y = 0; y < height; ++y) {
            const std::uint8_t* row = image.constScanLine(y);
            const std::size_t rowStart = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
            for (int x = 0; x < width; ++x) {
                const std::size_t idx = rowStart + static_cast<std::size_t>(x);
                p0[idx] = static_cast<float>(row[x * 4 + 0]) / 255.0f;
                p1[idx] = static_cast<float>(row[x * 4 + 1]) / 255.0f;
                p2[idx] = static_cast<float>(row[x * 4 + 2]) / 255.0f;
                p3[idx] = static_cast<float>(row[x * 4 + 3]) / 255.0f;
            }
        }
    }

    if (outHeader != nullptr) {
        *outHeader = toPublicHeaderInfo(header);
    }
    return Core::Result<TextureImage>::success(std::move(out));
}

Core::Result<std::optional<VtfSheetData>> VtfCodec::readSheetFromMemory(
    std::span<const std::byte> data,
    const QString& virtualFileName)
{
    const QString detailLabel = virtualFileName.isEmpty() ? QStringLiteral("<memory>") : virtualFileName;
    auto headerRes = parseVtfHeaderFromSpan(data, data.size(), virtualFileName, false);
    if (headerRes.isFailure()) {
        return Core::Result<std::optional<VtfSheetData>>::failure(headerRes.error());
    }
    const DecodedVtfHeader& header = headerRes.value();
    if (!header.hasSheet) {
        return Core::Result<std::optional<VtfSheetData>>::success(std::nullopt);
    }

    std::uint64_t sheetEnd = 0;
    if (header.sheetSize < 4u
        || !checkedAdd(header.sheetOffset, header.sheetSize, sheetEnd)
        || sheetEnd > data.size()) {
        return Core::Result<std::optional<VtfSheetData>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VtfCodec", "VTF sheet resource block is truncated"),
            detailLabel);
    }

    // VTF_RSRC_SHEET stores a uint32 byte-size prefix followed by version and seqCount.
    const std::uint32_t payloadSize = readU32Le(data.data() + header.sheetOffset);
    if (payloadSize < 8u || static_cast<std::uint64_t>(payloadSize) > header.sheetSize - 4u) {
        return Core::Result<std::optional<VtfSheetData>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VtfCodec", "VTF sheet resource size is invalid"),
            detailLabel);
    }

    const std::byte* cursor = data.data() + header.sheetOffset + 4u;
    const std::byte* const end = cursor + payloadSize;

    VtfSheetData sheet;
    sheet.version = readU32Le(cursor);
    const std::uint32_t seqCount = readU32Le(cursor + 4);
    cursor += 8;

    if (sheet.version != 0u && sheet.version != 1u) {
        return Core::Result<std::optional<VtfSheetData>>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("VtfCodec", "Unsupported VTF sheet resource version"),
            QStringLiteral("%1 (version=%2)").arg(detailLabel).arg(sheet.version));
    }

    const std::size_t coordSets = (sheet.version == 0u) ? 1u : 4u;
    const std::size_t frameBytes = 4u + coordSets * 16u;

    sheet.sequences.reserve(std::min<std::size_t>(seqCount, static_cast<std::size_t>(end - cursor) / 16u));
    for (std::uint32_t s = 0; s < seqCount; ++s) {
        if (static_cast<std::size_t>(end - cursor) < 16u) {
            return Core::Result<std::optional<VtfSheetData>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF sheet sequence header is truncated"),
                detailLabel);
        }

        VtfSheetSequence seq;
        seq.sequenceNumber = readU32Le(cursor + 0);
        seq.clamp = readU32Le(cursor + 4) != 0u;
        const std::uint32_t frameCount = readU32Le(cursor + 8);
        seq.totalTime = readF32Le(cursor + 12);
        cursor += 16;

        const std::size_t remaining = static_cast<std::size_t>(end - cursor);
        if (frameCount > remaining / frameBytes) {
            return Core::Result<std::optional<VtfSheetData>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VtfCodec", "VTF sheet frame data is truncated"),
                detailLabel);
        }

        seq.frames.reserve(frameCount);
        for (std::uint32_t f = 0; f < frameCount; ++f) {
            VtfSheetFrame frame;
            frame.duration = readF32Le(cursor);
            frame.uvRect = {
                readF32Le(cursor + 4),
                readF32Le(cursor + 8),
                readF32Le(cursor + 12),
                readF32Le(cursor + 16),
            };
            seq.frames.push_back(frame);
            cursor += frameBytes;
        }
        sheet.sequences.push_back(std::move(seq));
    }

    return Core::Result<std::optional<VtfSheetData>>::success(std::move(sheet));
}

Core::Result<QImage> VtfCodec::read(const Core::Path::FilesystemPath& path)
{
    auto bytesRes = readFileBytes(path);
    if (bytesRes.isFailure()) {
        return Core::Result<QImage>::failure(bytesRes.error());
    }
    return readSlice(bytesRes.value(), VtfSliceSelector{}, path.toString());
}

} // namespace Domain::Material
