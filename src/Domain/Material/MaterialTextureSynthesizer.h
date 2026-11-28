#pragma once

#include <QImage>
#include <QString>

#include <array>
#include <cmath>
#include <optional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

namespace Domain::Material {

/**
 * @brief Channel selector for single-channel mask extraction and PBR synthesis.
 */
enum class TextureChannel : int {
    Red = 0,
    Green = 1,
    Blue = 2,
    Alpha = 3,
    Luminance = 4,
};

/**
 * @brief Self-contained 2D UV transform matching Source 1 VMT texture transforms
 *        (center, scale, rotate in degrees, translate).
 */
struct UvTransform2D {
    std::array<float, 2> center = {0.5f, 0.5f};
    std::array<float, 2> scale = {1.0f, 1.0f};
    float rotateDeg = 0.0f;
    std::array<float, 2> translate = {0.0f, 0.0f};

    [[nodiscard]] bool isIdentity() const noexcept
    {
        constexpr float kEps = 1e-5f;
        return std::abs(scale[0] - 1.0f) <= kEps
            && std::abs(scale[1] - 1.0f) <= kEps
            && std::abs(rotateDeg) <= kEps
            && std::abs(translate[0]) <= kEps
            && std::abs(translate[1]) <= kEps;
    }
};

/**
 * @brief Combined output of UnlitTwoTexture linear-space synthesis.
 */
struct TwoTextureCombineResult {
    QImage colorImage;                ///< Opaque 24-bit sRGB color (Format_RGB888).
    std::optional<QImage> alphaMask;  ///< Optional 8-bit linear alpha mask (Format_Grayscale8) when non-opaque.
};

/**
 * @brief Pure in-memory CPU pixel synthesizer for Source 1 VMT/VTF -> CS2 PNG conversion.
 *
 * All synthesis functions are deterministic, touch no filesystem, check cooperative
 * cancellation periodically, and return QImages in Straight Format_RGBA8888,
 * Format_RGB888, or Format_Grayscale8 ready for TextureIO::writeImage.
 */
class MaterialTextureSynthesizer {
public:
    // =========================================================================
    // 1. Color / Alpha extraction, cropping, tint baking, and two-texture combine
    // =========================================================================

    /**
     * @brief Returns true if the image has an alpha channel with at least one pixel < 255.
     */
    [[nodiscard]] static bool hasNonOpaqueAlpha(const QImage& source);

    /**
     * @brief Exports an opaque 24-bit sRGB image (Format_RGB888), stripping alpha.
     */
    static Core::Result<QImage> exportOpaqueColor(
        const QImage& source,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Preserves full 32bpp Straight RGBA8888 (for ExplicitSprite / SpriteCard).
     */
    static Core::Result<QImage> exportStraightRgba(
        const QImage& source,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Extracts the alpha channel as an 8-bit linear grayscale mask (Format_Grayscale8).
     *        Images without an alpha channel yield a solid 255 mask.
     */
    static Core::Result<QImage> extractAlphaMask(
        const QImage& source,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Crops a subrectangle (x, y, width, height) with bounds clamping and validation.
     *        Preserves Format_RGBA8888 / Format_RGB888 / Format_Grayscale8; other formats
     *        are normalized to Straight Format_RGBA8888.
     */
    static Core::Result<QImage> cropSubrect(
        const QImage& source,
        int x,
        int y,
        int width,
        int height,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Multiplies sRGB source RGB by linearTint in linear light space and re-encodes sRGB.
     *        Outputs Format_RGB888 by default, or Format_RGBA8888 when preserveAlpha is true.
     */
    static Core::Result<QImage> bakeLinearTint(
        const QImage& source,
        const std::array<float, 3>& linearTint,
        bool preserveAlpha = false,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Combines $basetexture with $texture2 resampled by texture2Transform (repeat wrap)
     *        in linear light space, outputting sRGB color (Format_RGB888) and an optional
     *        8-bit linear alpha mask (Format_Grayscale8) when the combined alpha is non-opaque.
     */
    static Core::Result<TwoTextureCombineResult> combineTwoTextures(
        const QImage& baseTexture,
        const QImage& texture2,
        const UvTransform2D& texture2Transform = {},
        const Core::Async::CancellationToken& token = {});

    // =========================================================================
    // 2. Roughness reconstruction & $surfaceprop physical baseline lookup
    // =========================================================================

    /**
     * @brief Synthesizes an 8-bit linear roughness map (Format_Grayscale8) from a specular mask:
     *        R = clamp(1.0 - sqrt(M * t), Rmin, Rmax)
     *        where t = clamp(mean(envmapTint), 0.02, 1.0) (default 0.40 when unspecified),
     *        non-metal [0.25, 0.95], metal [0.18, 0.85].
     */
    static Core::Result<QImage> synthesizeRoughnessFromSpecMask(
        const QImage& maskSource,
        TextureChannel maskChannel,
        const std::optional<std::array<float, 3>>& envmapTint = std::nullopt,
        bool isMetallic = false,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Synthesizes an 8-bit linear roughness map (Format_Grayscale8) from
     *        $phongexponenttexture G channel:
     *        e = 1.0 + 149.0 * g, R = clamp(sqrt(2.0 / (e + 2.0)), 0.15, 0.95).
     */
    static Core::Result<QImage> synthesizeRoughnessFromPhongExponentTexture(
        const QImage& phongExponentTexture,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Computes scalar roughness from $phongexponent:
     *        R = clamp(sqrt(2.0 / (e + 2.0)), 0.35, 0.95).
     */
    [[nodiscard]] static float roughnessFromPhongExponent(float exponent) noexcept;

    /**
     * @brief Looks up the physical baseline roughness for a Source 1 $surfaceprop value
     *        (case-insensitive). Returns std::nullopt if unknown (caller uses 1.0).
     */
    [[nodiscard]] static std::optional<float> surfacePropBaselineRoughness(const QString& surfaceProp);

    /**
     * @brief Returns true if $surfaceprop belongs to the metallic material family.
     */
    [[nodiscard]] static bool isMetallicSurfaceProp(const QString& surfaceProp);

    // =========================================================================
    // 3. SSBump -> DirectX -Y tangent-space normal
    // =========================================================================

    /**
     * @brief Converts a Valve self-shadowed bump map ($ssbump 1) to a linear DirectX -Y
     *        tangent-space normal map (Format_RGB888).
     */
    static Core::Result<QImage> convertSsbumpToNormal(
        const QImage& ssbumpSource,
        const Core::Async::CancellationToken& token = {});

    // =========================================================================
    // 4. Normal convention & auxiliary masks
    // =========================================================================

    /**
     * @brief Flips the green channel (G' = 255 - G) of a normal map (Format_RGB888)
     *        for explicit NormalMap2D .vtex PNG export.
     */
    static Core::Result<QImage> flipNormalGreenChannel(
        const QImage& normalSource,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Extracts a single channel (or Rec.709 luminance) as an 8-bit linear grayscale
     *        mask (Format_Grayscale8).
     */
    static Core::Result<QImage> extractChannelMask(
        const QImage& source,
        TextureChannel channel,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Extracts Rec.709 luminance as an 8-bit linear grayscale mask (Format_Grayscale8).
     */
    static Core::Result<QImage> extractLuminanceMask(
        const QImage& source,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Synthesizes a conservative 8-bit linear metalness mask (Format_Grayscale8)
     *        from a specular mask for metallic $surfaceprop materials:
     *        metalness = clamp(sqrt(M * t), 0.0, 0.90).
     */
    static Core::Result<QImage> synthesizeMetalnessFromSpecMask(
        const QImage& maskSource,
        TextureChannel maskChannel,
        const std::optional<std::array<float, 3>>& envmapTint = std::nullopt,
        const Core::Async::CancellationToken& token = {});
};

} // namespace Domain::Material
