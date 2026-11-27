#pragma once

#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Material/TextureImage.h"

class QImage;

namespace Domain::Material {

/**
 * @brief Source 1 VTF compiled texture header bitflags.
 */
enum class VtfFlag : std::uint32_t {
    PointSample   = 0x00000001u,
    Trilinear     = 0x00000002u,
    ClampS        = 0x00000004u,
    ClampT        = 0x00000008u,
    Anisotropic   = 0x00000010u,
    Srgb          = 0x00000040u,
    Normal        = 0x00000080u,
    NoMip         = 0x00000100u,
    NoLod         = 0x00000200u,
    OneBitAlpha   = 0x00001000u,
    EightBitAlpha = 0x00002000u,
    EnvMap        = 0x00004000u,
    ClampU        = 0x02000000u,
};

/**
 * @brief Valve Texture Format pixel storage formats (PC v7.0-v7.6).
 */
enum class VtfImageFormat : int {
    RGBA8888 = 0,
    ABGR8888 = 1,
    RGB888 = 2,
    BGR888 = 3,
    RGB565 = 4,
    I8 = 5,
    IA88 = 6,
    P8 = 7,
    A8 = 8,
    RGB888_BLUESCREEN = 9,
    BGR888_BLUESCREEN = 10,
    ARGB8888 = 11,
    BGRA8888 = 12,
    DXT1 = 13,
    DXT3 = 14,
    DXT5 = 15,
    BGRX8888 = 16,
    BGR565 = 17,
    BGRX5551 = 18,
    BGRA4444 = 19,
    DXT1_ONE_BIT_ALPHA = 20,
    BGRA5551 = 21,
    UV88 = 22,
    UVWQ8888 = 23,
    RGBA16161616F = 24,
    RGBA16161616 = 25,
    UVLX8888 = 26,
    R32F = 27,
    RGB323232F = 28,
    RGBA32323232F = 29,
    RG1616F = 30,
    RG3232F = 31,
    RGBX8888 = 32,
    EMPTY = 33,
    ATI2N = 34,
    ATI1N = 35,
    RGBA1010102 = 36,
    BGRA1010102 = 37,
    R16F = 38,
};

/**
 * @brief Single frame UV box inside a VTF_RSRC_SHEET particle sequence.
 */
struct VtfSheetFrame {
    float duration = 0.0f;
    std::array<float, 4> uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
};

/**
 * @brief Particle sprite-sheet sequence parsed from VTF_RSRC_SHEET (0x00000010).
 */
struct VtfSheetSequence {
    std::uint32_t sequenceNumber = 0;
    bool clamp = false;
    float totalTime = 0.0f;
    std::vector<VtfSheetFrame> frames;
};

/**
 * @brief Decoded VTF_RSRC_SHEET atlas metadata container.
 */
struct VtfSheetData {
    std::uint32_t version = 0;
    std::vector<VtfSheetSequence> sequences;
};

/**
 * @brief Decoded VTF header metadata without pixel payload decompression.
 */
struct VtfHeaderInfo {
    std::uint32_t versionMajor = 0;
    std::uint32_t versionMinor = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t depth = 1;
    std::uint16_t frameCount = 1;
    std::uint8_t faceCount = 1;
    std::uint8_t mipCount = 1;
    std::uint32_t flags = 0;
    VtfImageFormat format = VtfImageFormat::EMPTY;
    float bumpmapScale = 1.0f;
    std::array<float, 3> reflectivity = {0.0f, 0.0f, 0.0f};
    bool isHdr = false;
    bool hasAlphaChannel = false;
    bool hasSheetResource = false;

    bool hasFlag(VtfFlag flag) const noexcept {
        return (flags & static_cast<std::uint32_t>(flag)) != 0u;
    }
};

/**
 * @brief Target slice selector within mip 0 of a VTF container.
 */
struct VtfSliceSelector {
    std::uint16_t frame = 0;
    std::uint8_t face = 0;
    std::uint16_t depthSlice = 0;
    bool treatAsCompressedHdr = false;
};

/**
 * @brief Native C++20 read-only VTF (Valve Texture Format v7.0-v7.6) decoder.
 *
 * Supports file and in-memory span decoding across frames, cubemap faces, depth
 * slices, v7.6 AXC Deflate chunks, HDR floating-point/RGBM formats, and
 * VTF_RSRC_SHEET particle atlas metadata.
 */
class VtfCodec {
public:
    /**
     * @brief Reads mip 0 (frame 0, face 0, slice 0) of a VTF file into a QImage (RGBA8888).
     */
    static Core::Result<QImage> read(const Core::Path::FilesystemPath& path);

    /**
     * @brief Reads only the VTF header metadata from a file on disk.
     */
    static Core::Result<VtfHeaderInfo> readHeader(const Core::Path::FilesystemPath& path);

    /**
     * @brief Parses VTF header metadata from an in-memory buffer.
     */
    static Core::Result<VtfHeaderInfo> readHeaderFromMemory(
        std::span<const std::byte> data,
        const QString& virtualFileName = {});

    /**
     * @brief Decodes a selected mip 0 slice from an in-memory VTF buffer to RGBA8888 QImage.
     */
    static Core::Result<QImage> readSlice(
        std::span<const std::byte> data,
        const VtfSliceSelector& slice = {},
        const QString& virtualFileName = {},
        VtfHeaderInfo* outHeader = nullptr);

    /**
     * @brief Decodes a selected mip 0 slice from an in-memory VTF buffer into a
     *        4-channel TextureImage, preserving HDR > 1.0 dynamic range.
     */
    static Core::Result<TextureImage> readFloatSlice(
        std::span<const std::byte> data,
        const VtfSliceSelector& slice = {},
        const QString& virtualFileName = {},
        VtfHeaderInfo* outHeader = nullptr);

    /**
     * @brief Parses the VTF_RSRC_SHEET (0x00000010) particle atlas resource from memory.
     */
    static Core::Result<std::optional<VtfSheetData>> readSheetFromMemory(
        std::span<const std::byte> data,
        const QString& virtualFileName = {});

    static bool isVtfExtension(const QString& lowerCaseExtension);
};

} // namespace Domain::Material
