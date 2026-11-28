#include "Domain/Material/MaterialTextureSynthesizer.h"

#include <QCoreApplication>
#include <QHash>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "Core/Error/ErrorCode.h"
#include "Domain/Material/TextureIO.h"
#include "Domain/Material/TextureImage.h"

namespace Domain::Material {

namespace {

constexpr int kCancelCheckRowMask = 63;

float srgbDecodeByte(unsigned char encoded) noexcept
{
    static const std::array<float, 256> kLut = [] {
        std::array<float, 256> table{};
        for (std::size_t i = 0; i < table.size(); ++i) {
            const float c = static_cast<float>(i) / 255.0f;
            table[i] = c <= 0.04045f
                ? (c / 12.92f)
                : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }
        return table;
    }();
    return kLut[encoded];
}

unsigned char srgbEncodeFloat(float linear) noexcept
{
    const float c = std::isfinite(linear) ? std::clamp(linear, 0.0f, 1.0f) : 0.0f;
    const float encoded = c <= 0.0031308f
        ? c * 12.92f
        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<unsigned char>(std::lround(encoded * 255.0f));
}

unsigned char quantizeLinear(float value) noexcept
{
    const float clamped = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    return static_cast<unsigned char>(std::lround(clamped * 255.0f));
}

float resolveEnvmapTintFactor(const std::optional<std::array<float, 3>>& envmapTint) noexcept
{
    if (!envmapTint.has_value()) {
        return 0.40f;
    }
    const auto& tint = *envmapTint;
    const float r = std::isfinite(tint[0]) ? std::max(0.0f, tint[0]) : 0.0f;
    const float g = std::isfinite(tint[1]) ? std::max(0.0f, tint[1]) : 0.0f;
    const float b = std::isfinite(tint[2]) ? std::max(0.0f, tint[2]) : 0.0f;
    const float mean = (r + g + b) / 3.0f;
    return std::clamp(mean, 0.02f, 1.0f);
}

float extractChannelValue(const unsigned char* rgbaPixel, TextureChannel channel) noexcept
{
    constexpr float kInv255 = 1.0f / 255.0f;
    switch (channel) {
    case TextureChannel::Red:
        return static_cast<float>(rgbaPixel[0]) * kInv255;
    case TextureChannel::Green:
        return static_cast<float>(rgbaPixel[1]) * kInv255;
    case TextureChannel::Blue:
        return static_cast<float>(rgbaPixel[2]) * kInv255;
    case TextureChannel::Alpha:
        return static_cast<float>(rgbaPixel[3]) * kInv255;
    case TextureChannel::Luminance:
        return (0.2126f * static_cast<float>(rgbaPixel[0])
              + 0.7152f * static_cast<float>(rgbaPixel[1])
              + 0.0722f * static_cast<float>(rgbaPixel[2])) * kInv255;
    }
    return 0.0f;
}

Core::Result<QImage> toRgba8888OrError(const QImage& source)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "source image is null or has invalid dimensions"));
    }

    QImage rgba = source.format() == QImage::Format_RGBA8888
        ? source
        : source.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to convert source image to RGBA8888"));
    }
    return Core::Result<QImage>::success(std::move(rgba));
}

} // namespace

// =============================================================================
// 1. Color / Alpha extraction, cropping, tint baking, and two-texture combine
// =============================================================================

bool MaterialTextureSynthesizer::hasNonOpaqueAlpha(const QImage& source)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0 || !source.hasAlphaChannel()) {
        return false;
    }

    const QImage rgba = source.format() == QImage::Format_RGBA8888
        ? source
        : source.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) {
        return false;
    }

    const int width = rgba.width();
    const int height = rgba.height();
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = rgba.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            if (row[x * 4 + 3] < 255u) {
                return true;
            }
        }
    }
    return false;
}

Core::Result<QImage> MaterialTextureSynthesizer::exportOpaqueColor(
    const QImage& source,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(source);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_RGB888);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate RGB888 color image"));
    }

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char* sp = srcRow + x * 4;
            unsigned char* dp = dstRow + x * 3;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

Core::Result<QImage> MaterialTextureSynthesizer::exportStraightRgba(
    const QImage& source,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }
    return toRgba8888OrError(source);
}

Core::Result<QImage> MaterialTextureSynthesizer::extractAlphaMask(
    const QImage& source,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "source image is null or has invalid dimensions"));
    }

    if (!source.hasAlphaChannel()) {
        QImage opaque(source.width(), source.height(), QImage::Format_Grayscale8);
        if (opaque.isNull()) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate grayscale mask image"));
        }
        opaque.fill(255u);
        return Core::Result<QImage>::success(std::move(opaque));
    }

    return extractChannelMask(source, TextureChannel::Alpha, token);
}

Core::Result<QImage> MaterialTextureSynthesizer::cropSubrect(
    const QImage& source,
    int x,
    int y,
    int width,
    int height,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "source image is null or has invalid dimensions"));
    }
    if (width <= 0 || height <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "subrect width and height must be positive"));
    }

    const int srcW = source.width();
    const int srcH = source.height();
    const int x0 = std::clamp(x, 0, srcW);
    const int y0 = std::clamp(y, 0, srcH);
    const int x1 = std::clamp(x + width, 0, srcW);
    const int y1 = std::clamp(y + height, 0, srcH);
    const int clampedW = x1 - x0;
    const int clampedH = y1 - y0;

    if (clampedW <= 0 || clampedH <= 0) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "subrect lies completely outside source image bounds"));
    }

    QImage cropped = source.copy(x0, y0, clampedW, clampedH);
    if (cropped.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to crop subrect from source image"));
    }

    const QImage::Format fmt = cropped.format();
    if (fmt != QImage::Format_RGBA8888
        && fmt != QImage::Format_RGB888
        && fmt != QImage::Format_Grayscale8) {
        cropped = cropped.convertToFormat(QImage::Format_RGBA8888);
        if (cropped.isNull()) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("MaterialTextureSynthesizer", "failed to normalize cropped subrect image"));
        }
    }

    return Core::Result<QImage>::success(std::move(cropped));
}

Core::Result<QImage> MaterialTextureSynthesizer::bakeLinearTint(
    const QImage& source,
    const std::array<float, 3>& linearTint,
    bool preserveAlpha,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(source);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const float tr = std::isfinite(linearTint[0]) ? std::max(0.0f, linearTint[0]) : 1.0f;
    const float tg = std::isfinite(linearTint[1]) ? std::max(0.0f, linearTint[1]) : 1.0f;
    const float tb = std::isfinite(linearTint[2]) ? std::max(0.0f, linearTint[2]) : 1.0f;

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, preserveAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate tinted output image"));
    }

    const int dstStride = preserveAlpha ? 4 : 3;
    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char* sp = srcRow + x * 4;
            unsigned char* dp = dstRow + x * dstStride;
            dp[0] = srgbEncodeFloat(srgbDecodeByte(sp[0]) * tr);
            dp[1] = srgbEncodeFloat(srgbDecodeByte(sp[1]) * tg);
            dp[2] = srgbEncodeFloat(srgbDecodeByte(sp[2]) * tb);
            if (preserveAlpha) {
                dp[3] = sp[3];
            }
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

Core::Result<TwoTextureCombineResult> MaterialTextureSynthesizer::combineTwoTextures(
    const QImage& baseTexture,
    const QImage& texture2,
    const UvTransform2D& texture2Transform,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<TwoTextureCombineResult>::cancelled();
    }
    if (baseTexture.isNull() || baseTexture.width() <= 0 || baseTexture.height() <= 0) {
        return Core::Result<TwoTextureCombineResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "base texture is null or has invalid dimensions"));
    }
    if (texture2.isNull() || texture2.width() <= 0 || texture2.height() <= 0) {
        return Core::Result<TwoTextureCombineResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "secondary texture is null or has invalid dimensions"));
    }

    auto baseLinearRes = TextureIO::fromQImage(baseTexture, true);
    if (baseLinearRes.isFailure()) {
        return Core::Result<TwoTextureCombineResult>::failure(baseLinearRes.error());
    }
    auto tex2LinearRes = TextureIO::fromQImage(texture2, true);
    if (tex2LinearRes.isFailure()) {
        return Core::Result<TwoTextureCombineResult>::failure(tex2LinearRes.error());
    }

    const TextureImage& baseLin = baseLinearRes.value();
    const TextureImage& tex2Lin = tex2LinearRes.value();

    const int outW = baseLin.width();
    const int outH = baseLin.height();
    const int tex2W = tex2Lin.width();
    const int tex2H = tex2Lin.height();

    QImage colorOut(outW, outH, QImage::Format_RGB888);
    QImage alphaOut(outW, outH, QImage::Format_Grayscale8);
    if (colorOut.isNull() || alphaOut.isNull()) {
        return Core::Result<TwoTextureCombineResult>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate combined texture buffers"));
    }

    const float* baseR = baseLin.planeData(0);
    const float* baseG = baseLin.planeData(1);
    const float* baseB = baseLin.planeData(2);
    const float* baseA = baseLin.planeData(3);

    const bool directTex2Match = texture2Transform.isIdentity() && tex2W == outW && tex2H == outH;
    const float* tex2R = tex2Lin.planeData(0);
    const float* tex2G = tex2Lin.planeData(1);
    const float* tex2B = tex2Lin.planeData(2);
    const float* tex2A = tex2Lin.planeData(3);

    constexpr float kDegToRad = 0.01745329251994329577f;
    const float rad = std::isfinite(texture2Transform.rotateDeg)
        ? texture2Transform.rotateDeg * kDegToRad
        : 0.0f;
    const float cosR = std::cos(rad);
    const float sinR = std::sin(rad);
    const float cx = std::isfinite(texture2Transform.center[0]) ? texture2Transform.center[0] : 0.5f;
    const float cy = std::isfinite(texture2Transform.center[1]) ? texture2Transform.center[1] : 0.5f;
    const float sx = std::isfinite(texture2Transform.scale[0]) ? texture2Transform.scale[0] : 1.0f;
    const float sy = std::isfinite(texture2Transform.scale[1]) ? texture2Transform.scale[1] : 1.0f;
    const float txOff = std::isfinite(texture2Transform.translate[0]) ? texture2Transform.translate[0] : 0.0f;
    const float tyOff = std::isfinite(texture2Transform.translate[1]) ? texture2Transform.translate[1] : 0.0f;

    const float invOutW = 1.0f / static_cast<float>(outW);
    const float invOutH = 1.0f / static_cast<float>(outH);
    const float fTex2W = static_cast<float>(tex2W);
    const float fTex2H = static_cast<float>(tex2H);

    bool anyNonOpaqueAlpha = false;

    for (int y = 0; y < outH; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<TwoTextureCombineResult>::cancelled();
        }
        const std::size_t rowStart = static_cast<std::size_t>(y) * static_cast<std::size_t>(outW);
        unsigned char* colorRow = colorOut.scanLine(y);
        unsigned char* alphaRow = alphaOut.scanLine(y);

        const float v = (static_cast<float>(y) + 0.5f) * invOutH;
        const float dv = (v - cy) * sy;

        for (int x = 0; x < outW; ++x) {
            const std::size_t idx = rowStart + static_cast<std::size_t>(x);
            float r2 = 1.0f;
            float g2 = 1.0f;
            float b2 = 1.0f;
            float a2 = 1.0f;

            if (directTex2Match) {
                r2 = tex2R[idx];
                g2 = tex2G[idx];
                b2 = tex2B[idx];
                a2 = tex2A[idx];
            } else {
                const float u = (static_cast<float>(x) + 0.5f) * invOutW;
                const float du = (u - cx) * sx;
                const float ru = du * cosR - dv * sinR;
                const float rv = du * sinR + dv * cosR;
                const float uPrime = ru + cx + txOff;
                const float vPrime = rv + cy + tyOff;

                const float texelX = uPrime * fTex2W - 0.5f;
                const float texelY = vPrime * fTex2H - 0.5f;
                r2 = tex2Lin.sampleBilinearWrapped(0, texelX, texelY);
                g2 = tex2Lin.sampleBilinearWrapped(1, texelX, texelY);
                b2 = tex2Lin.sampleBilinearWrapped(2, texelX, texelY);
                a2 = tex2Lin.sampleBilinearWrapped(3, texelX, texelY);
            }

            unsigned char* cp = colorRow + x * 3;
            cp[0] = srgbEncodeFloat(baseR[idx] * r2);
            cp[1] = srgbEncodeFloat(baseG[idx] * g2);
            cp[2] = srgbEncodeFloat(baseB[idx] * b2);

            const unsigned char alphaByte = quantizeLinear(baseA[idx] * a2);
            alphaRow[x] = alphaByte;
            if (alphaByte < 255u) {
                anyNonOpaqueAlpha = true;
            }
        }
    }

    TwoTextureCombineResult result;
    result.colorImage = std::move(colorOut);
    if (anyNonOpaqueAlpha) {
        result.alphaMask = std::move(alphaOut);
    }
    return Core::Result<TwoTextureCombineResult>::success(std::move(result));
}

// =============================================================================
// 2. Roughness reconstruction & $surfaceprop physical baseline lookup
// =============================================================================

Core::Result<QImage> MaterialTextureSynthesizer::synthesizeRoughnessFromSpecMask(
    const QImage& maskSource,
    TextureChannel maskChannel,
    const std::optional<std::array<float, 3>>& envmapTint,
    bool isMetallic,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(maskSource);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_Grayscale8);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate roughness output image"));
    }

    const float tintFactor = resolveEnvmapTintFactor(envmapTint);
    const float rMin = isMetallic ? 0.18f : 0.25f;
    const float rMax = isMetallic ? 0.85f : 0.95f;

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const float m = extractChannelValue(srcRow + x * 4, maskChannel);
            const float roughness = std::clamp(1.0f - std::sqrt(std::max(0.0f, m * tintFactor)), rMin, rMax);
            dstRow[x] = quantizeLinear(roughness);
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

Core::Result<QImage> MaterialTextureSynthesizer::synthesizeRoughnessFromPhongExponentTexture(
    const QImage& phongExponentTexture,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(phongExponentTexture);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_Grayscale8);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate roughness output image"));
    }

    constexpr float kInv255 = 1.0f / 255.0f;
    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const float g = static_cast<float>(srcRow[x * 4 + 1]) * kInv255;
            const float exponent = 1.0f + 149.0f * g;
            const float roughness = std::clamp(std::sqrt(2.0f / (exponent + 2.0f)), 0.15f, 0.95f);
            dstRow[x] = quantizeLinear(roughness);
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

float MaterialTextureSynthesizer::roughnessFromPhongExponent(float exponent) noexcept
{
    const float safeExp = (std::isfinite(exponent) && exponent > 0.0f) ? exponent : 0.0f;
    return std::clamp(std::sqrt(2.0f / (safeExp + 2.0f)), 0.35f, 0.95f);
}

std::optional<float> MaterialTextureSynthesizer::surfacePropBaselineRoughness(const QString& surfaceProp)
{
    const QString key = surfaceProp.trimmed().toLower();
    if (key.isEmpty()) {
        return std::nullopt;
    }

    static const QHash<QString, float> kBaselineTable = {
        // 0.92 — rough mineral / soil / snow
        {QStringLiteral("concrete"),     0.92f},
        {QStringLiteral("brick"),        0.92f},
        {QStringLiteral("plaster"),      0.92f},
        {QStringLiteral("dirt"),         0.92f},
        {QStringLiteral("sand"),         0.92f},
        {QStringLiteral("rock"),         0.92f},
        {QStringLiteral("gravel"),       0.92f},
        {QStringLiteral("mud"),          0.92f},
        {QStringLiteral("asphalt"),      0.92f},
        {QStringLiteral("snow"),         0.92f},

        // 0.85 — organic / matte wood / fabric / paper
        {QStringLiteral("wood"),         0.85f},
        {QStringLiteral("wood_plank"),   0.85f},
        {QStringLiteral("wood_panel"),   0.85f},
        {QStringLiteral("wood_crate"),   0.85f},
        {QStringLiteral("wood_solid"),   0.85f},
        {QStringLiteral("foliage"),      0.85f},
        {QStringLiteral("carpet"),       0.85f},
        {QStringLiteral("cloth"),        0.85f},
        {QStringLiteral("paper"),        0.85f},
        {QStringLiteral("cardboard"),    0.85f},

        // 0.70 — glazed ceramic / tile
        {QStringLiteral("tile"),         0.70f},
        {QStringLiteral("porcelain"),    0.70f},
        {QStringLiteral("ceramic"),      0.70f},

        // 0.65 — polymer / rubber
        {QStringLiteral("plastic"),        0.65f},
        {QStringLiteral("plastic_barrel"), 0.65f},
        {QStringLiteral("plastic_box"),    0.65f},
        {QStringLiteral("rubber"),         0.65f},

        // 0.55 — metallic surfaces
        {QStringLiteral("metal"),        0.55f},
        {QStringLiteral("metalpanel"),   0.55f},
        {QStringLiteral("metalvent"),    0.55f},
        {QStringLiteral("metalgrate"),   0.55f},
        {QStringLiteral("chainlink"),    0.55f},
        {QStringLiteral("solidmetal"),   0.55f},
        {QStringLiteral("metal_barrel"), 0.55f},
        {QStringLiteral("metal_box"),    0.55f},

        // 0.15 — smooth dielectric glass / ice
        {QStringLiteral("glass"),        0.15f},
        {QStringLiteral("glassbottle"),  0.15f},
        {QStringLiteral("ice"),          0.15f},
    };

    const auto it = kBaselineTable.constFind(key);
    if (it == kBaselineTable.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

bool MaterialTextureSynthesizer::isMetallicSurfaceProp(const QString& surfaceProp)
{
    const QString key = surfaceProp.trimmed().toLower();
    if (key.isEmpty()) {
        return false;
    }

    return key == QLatin1String("metal")
        || key == QLatin1String("metalpanel")
        || key == QLatin1String("metalvent")
        || key == QLatin1String("metalgrate")
        || key == QLatin1String("chainlink")
        || key == QLatin1String("solidmetal")
        || key == QLatin1String("metal_barrel")
        || key == QLatin1String("metal_box");
}

// =============================================================================
// 3. SSBump -> DirectX -Y tangent-space normal
// =============================================================================

Core::Result<QImage> MaterialTextureSynthesizer::convertSsbumpToNormal(
    const QImage& ssbumpSource,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(ssbumpSource);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_RGB888);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate normal output image"));
    }

    // Valve 3 orthogonal SSBump basis vectors:
    // b0 = ( sqrt(2/3),  0,          sqrt(1/3))
    // b1 = (-sqrt(1/6),  sqrt(1/2),  sqrt(1/3))
    // b2 = (-sqrt(1/6), -sqrt(1/2),  sqrt(1/3))
    constexpr float kSqrt2Div3 = 0.8164965809277260f;
    constexpr float kSqrt1Div6 = 0.4082482904638630f;
    constexpr float kSqrt1Div2 = 0.7071067811865475f;
    constexpr float kSqrt1Div3 = 0.5773502691896258f;
    constexpr float kInv255 = 1.0f / 255.0f;

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char* sp = srcRow + x * 4;
            const float h0 = static_cast<float>(sp[0]) * kInv255;
            const float h1 = static_cast<float>(sp[1]) * kInv255;
            const float h2 = static_cast<float>(sp[2]) * kInv255;

            float nx = h0 * kSqrt2Div3 - (h1 + h2) * kSqrt1Div6;
            float ny = (h1 - h2) * kSqrt1Div2;
            float nz = (h0 + h1 + h2) * kSqrt1Div3;

            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) {
                const float invLen = 1.0f / len;
                nx *= invLen;
                ny *= invLen;
                nz *= invLen;
            } else {
                nx = 0.0f;
                ny = 0.0f;
                nz = 1.0f;
            }

            // Encode DirectX -Y tangent-space normal: (nx*0.5+0.5, -ny*0.5+0.5, nz*0.5+0.5)
            unsigned char* dp = dstRow + x * 3;
            dp[0] = quantizeLinear(nx * 0.5f + 0.5f);
            dp[1] = quantizeLinear(-ny * 0.5f + 0.5f);
            dp[2] = quantizeLinear(nz * 0.5f + 0.5f);
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

// =============================================================================
// 4. Normal convention & auxiliary masks
// =============================================================================

Core::Result<QImage> MaterialTextureSynthesizer::flipNormalGreenChannel(
    const QImage& normalSource,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(normalSource);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_RGB888);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate flipped normal image"));
    }

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char* sp = srcRow + x * 4;
            unsigned char* dp = dstRow + x * 3;
            dp[0] = sp[0];
            dp[1] = static_cast<unsigned char>(255u - sp[1]);
            dp[2] = sp[2];
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

Core::Result<QImage> MaterialTextureSynthesizer::extractChannelMask(
    const QImage& source,
    TextureChannel channel,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(source);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_Grayscale8);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate grayscale mask image"));
    }

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const unsigned char* sp = srcRow + x * 4;
            switch (channel) {
            case TextureChannel::Red:
                dstRow[x] = sp[0];
                break;
            case TextureChannel::Green:
                dstRow[x] = sp[1];
                break;
            case TextureChannel::Blue:
                dstRow[x] = sp[2];
                break;
            case TextureChannel::Alpha:
                dstRow[x] = sp[3];
                break;
            case TextureChannel::Luminance:
                dstRow[x] = quantizeLinear(extractChannelValue(sp, TextureChannel::Luminance));
                break;
            }
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

Core::Result<QImage> MaterialTextureSynthesizer::extractLuminanceMask(
    const QImage& source,
    const Core::Async::CancellationToken& token)
{
    return extractChannelMask(source, TextureChannel::Luminance, token);
}

Core::Result<QImage> MaterialTextureSynthesizer::synthesizeMetalnessFromSpecMask(
    const QImage& maskSource,
    TextureChannel maskChannel,
    const std::optional<std::array<float, 3>>& envmapTint,
    const Core::Async::CancellationToken& token)
{
    if (token.isCancelled()) {
        return Core::Result<QImage>::cancelled();
    }

    auto rgbaResult = toRgba8888OrError(maskSource);
    if (rgbaResult.isFailure()) {
        return rgbaResult;
    }
    const QImage& rgba = rgbaResult.value();

    const int width = rgba.width();
    const int height = rgba.height();
    QImage out(width, height, QImage::Format_Grayscale8);
    if (out.isNull()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate metalness output image"));
    }

    const float tintFactor = resolveEnvmapTintFactor(envmapTint);

    for (int y = 0; y < height; ++y) {
        if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
            return Core::Result<QImage>::cancelled();
        }
        const unsigned char* srcRow = rgba.constScanLine(y);
        unsigned char* dstRow = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const float m = extractChannelValue(srcRow + x * 4, maskChannel);
            const float metalness = std::clamp(std::sqrt(std::max(0.0f, m * tintFactor)), 0.0f, 0.90f);
            dstRow[x] = quantizeLinear(metalness);
        }
    }

    return Core::Result<QImage>::success(std::move(out));
}

// =============================================================================
// 5. Uniform power-of-2 grid sequence atlas synthesis
// =============================================================================

Core::Result<TextureAtlasResult> MaterialTextureSynthesizer::synthesizeUniformGridAtlas(
    const QVector<QImage>& frames,
    bool extractAlphaAtlas,
    const Core::Async::CancellationToken& token,
    int maxAtlasDim)
{
    if (token.isCancelled()) {
        return Core::Result<TextureAtlasResult>::cancelled();
    }
    if (frames.isEmpty()) {
        return Core::Result<TextureAtlasResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "frame list is empty"));
    }
    if (maxAtlasDim <= 0) {
        return Core::Result<TextureAtlasResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "maximum atlas dimension must be positive"));
    }

    for (const QImage& frame : frames) {
        if (frame.isNull() || frame.width() <= 0 || frame.height() <= 0) {
            return Core::Result<TextureAtlasResult>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("MaterialTextureSynthesizer", "animation frame is null or has invalid dimensions"));
        }
    }

    auto nextPow2 = [](int value) noexcept -> int {
        if (value <= 1) {
            return 1;
        }
        int p = 1;
        while (p < value && p <= (1 << 29)) {
            p <<= 1;
        }
        return p;
    };

    const int frameCount = static_cast<int>(frames.size());
    const int minCols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(frameCount))));
    const int cols = nextPow2(minCols);
    const int minRows = (frameCount + cols - 1) / cols;
    const int rows = nextPow2(minRows);

    if (cols > maxAtlasDim || rows > maxAtlasDim) {
        return Core::Result<TextureAtlasResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTextureSynthesizer", "grid dimensions exceed maximum atlas dimension"));
    }

    int cellW = frames[0].width();
    int cellH = frames[0].height();
    const double rawTotalW = static_cast<double>(cols) * static_cast<double>(cellW);
    const double rawTotalH = static_cast<double>(rows) * static_cast<double>(cellH);
    if (rawTotalW > static_cast<double>(maxAtlasDim) || rawTotalH > static_cast<double>(maxAtlasDim)) {
        const double scale = std::min(
            static_cast<double>(maxAtlasDim) / rawTotalW,
            static_cast<double>(maxAtlasDim) / rawTotalH);
        cellW = std::max(1, static_cast<int>(std::floor(static_cast<double>(cellW) * scale)));
        cellH = std::max(1, static_cast<int>(std::floor(static_cast<double>(cellH) * scale)));
        cellW = std::min(cellW, std::max(1, maxAtlasDim / cols));
        cellH = std::min(cellH, std::max(1, maxAtlasDim / rows));
    }

    const int atlasW = cols * cellW;
    const int atlasH = rows * cellH;

    QImage colorAtlas(atlasW, atlasH, QImage::Format_RGB888);
    if (colorAtlas.isNull()) {
        return Core::Result<TextureAtlasResult>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate color atlas image"));
    }
    colorAtlas.fill(0u);

    QImage alphaAtlas;
    if (extractAlphaAtlas) {
        alphaAtlas = QImage(atlasW, atlasH, QImage::Format_Grayscale8);
        if (alphaAtlas.isNull()) {
            return Core::Result<TextureAtlasResult>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("MaterialTextureSynthesizer", "failed to allocate alpha atlas image"));
        }
        alphaAtlas.fill(0u);
    }

    bool anyNonOpaqueAlpha = false;

    for (int i = 0; i < frameCount; ++i) {
        if (token.isCancelled()) {
            return Core::Result<TextureAtlasResult>::cancelled();
        }

        auto rgbaRes = toRgba8888OrError(frames[i]);
        if (rgbaRes.isFailure()) {
            return Core::Result<TextureAtlasResult>::failure(rgbaRes.error());
        }
        QImage cell = std::move(rgbaRes.value());
        if (cell.width() != cellW || cell.height() != cellH) {
            cell = cell.scaled(cellW, cellH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (cell.isNull()) {
                return Core::Result<TextureAtlasResult>::failure(
                    Core::Error::ErrorCode::OperationFailed,
                    QCoreApplication::translate("MaterialTextureSynthesizer", "failed to scale animation frame for atlas"));
            }
            if (cell.format() != QImage::Format_RGBA8888) {
                cell = cell.convertToFormat(QImage::Format_RGBA8888);
            }
        }

        const int col = i % cols;
        const int row = i / cols;
        const int dstX = col * cellW;
        const int dstY = row * cellH;

        for (int y = 0; y < cellH; ++y) {
            if ((y & kCancelCheckRowMask) == 0 && token.isCancelled()) {
                return Core::Result<TextureAtlasResult>::cancelled();
            }
            const unsigned char* srcRow = cell.constScanLine(y);
            unsigned char* dstColorRow = colorAtlas.scanLine(dstY + y) + dstX * 3;
            unsigned char* dstAlphaRow = extractAlphaAtlas ? (alphaAtlas.scanLine(dstY + y) + dstX) : nullptr;
            for (int x = 0; x < cellW; ++x) {
                const unsigned char* sp = srcRow + x * 4;
                unsigned char* dp = dstColorRow + x * 3;
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                if (dstAlphaRow != nullptr) {
                    dstAlphaRow[x] = sp[3];
                    if (sp[3] < 255u) {
                        anyNonOpaqueAlpha = true;
                    }
                }
            }
        }
    }

    TextureAtlasResult result;
    result.colorAtlas = std::move(colorAtlas);
    if (extractAlphaAtlas && anyNonOpaqueAlpha) {
        result.alphaAtlas = std::move(alphaAtlas);
    }
    result.cols = cols;
    result.rows = rows;
    result.cellCount = frameCount;
    return Core::Result<TextureAtlasResult>::success(std::move(result));
}

} // namespace Domain::Material
