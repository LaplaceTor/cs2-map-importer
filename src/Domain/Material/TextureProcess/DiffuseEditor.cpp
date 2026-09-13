#include "Domain/Material/TextureProcess/DiffuseEditor.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

#include "Domain/Material/TextureProcess/ColorMath.h"
#include "Domain/Material/TextureProcess/TextureBlur.h"

using Domain::Material::TextureProcess::ColorMath::hslToRgb;
using Domain::Material::TextureProcess::ColorMath::luminance;
using Domain::Material::TextureProcess::ColorMath::rgbToHsl;
using Domain::Material::TextureProcess::ColorMath::saturate;
using Domain::Material::TextureProcess::ColorMath::smoothStep;

namespace {

// H+V separable blur matching the reference blur chain setup (edit contrast
// is 1.0 while blurring; the user's overlay contrast only feeds the shader).
Core::Result<Domain::Material::TextureImage> blurHv(
    const Domain::Material::TextureImage& source, int samples, float spread,
    const Core::Async::CancellationToken& token)
{
    using TextureBlur = Domain::Material::TextureProcess::TextureBlur;
    auto horizontal = TextureBlur::blurAxis(source, TextureBlur::Axis::Horizontal, samples, spread,
        1.0f, token, {});
    if (horizontal.isFailure()) {
        return horizontal;
    }
    return TextureBlur::blurAxis(horizontal.value(), TextureBlur::Axis::Vertical, samples, spread,
        1.0f, token, {});
}

} // namespace

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> DiffuseEditor::edit(const TextureImage& linearDiffuse,
                                               const DiffuseEditParams& params,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress)
{
    if (!linearDiffuse.isValid() || linearDiffuse.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("DiffuseEditor",
                "diffuse editing requires a valid color image (3-4 channels)"));
    }
    if (params.overlayBlurSize <= 0 || params.avgColorBlurSize <= 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("DiffuseEditor", "blur sizes must be positive"));
    }

    const int width = linearDiffuse.width();
    const int height = linearDiffuse.height();

    // Reference orchestration: overlay blur (Overlay Blur Size, spread 1.0),
    // then the average map built from two cascaded H+V blur rounds
    // (AvgColorBlurSize samples; second round spread = AvgColorBlurSize / 5).
    auto overlayBlur = blurHv(linearDiffuse, params.overlayBlurSize, 1.0f, token);
    if (overlayBlur.isFailure()) {
        return Core::Result<TextureImage>::failure(overlayBlur.error());
    }
    if (progress) {
        progress(0.2f);
    }
    auto averageFirst = blurHv(linearDiffuse, params.avgColorBlurSize, 1.0f, token);
    if (averageFirst.isFailure()) {
        return Core::Result<TextureImage>::failure(averageFirst.error());
    }
    auto average = blurHv(averageFirst.value(), params.avgColorBlurSize,
        params.avgColorBlurSize / 5.0f, token);
    if (average.isFailure()) {
        return Core::Result<TextureImage>::failure(average.error());
    }
    if (progress) {
        progress(0.6f);
    }

    const TextureImage& blurMap = overlayBlur.value();
    const TextureImage& avgMap = average.value();
    const float overlayContrast = params.overlayBlurContrast;

    TextureImage edited(width, height, 3);
    if (!edited.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("DiffuseEditor", "failed to allocate working buffers"));
    }

    // Mask-power exponent mapping from the shader (V-shape around 0.5).
    auto maskPow = [](float uiPower) {
        const float exponent = saturate((uiPower - 0.5f) * 2.0f) + 1.0f;
        return exponent - (1.0f - 1.0f / (saturate((uiPower - 0.5f) * -2.0f) + 1.0f));
    };
    const float lightPowExp = maskPow(params.lightMaskPower);
    const float darkPowExp = maskPow(params.shadowMaskPower);

    const float hotSpot = saturate(params.hotSpotRemoval);
    const float darkSpot = saturate(params.darkSpotRemoval);
    const float spotLightEdge0 = 1.0f - hotSpot;
    const float spotLightEdge1 = spotLightEdge0 + std::pow(hotSpot, 0.5f) + 0.01f;
    const float spotDarkEdge0 = darkSpot - std::pow(darkSpot, 0.5f) - 0.01f;

    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (y & 0x3F) == 0) {
            progress(0.6f + static_cast<float>(y) / static_cast<float>(height) * 0.4f);
        }
        for (int x = 0; x < width; ++x) {
            float r = linearDiffuse.at(0, x, y);
            float g = linearDiffuse.at(1, x, y);
            float b = linearDiffuse.at(2, x, y);

            float initialH = 0.0f;
            float initialS = 0.0f;
            float initialL = 0.0f;
            rgbToHsl(saturate(r), saturate(g), saturate(b), initialH, initialS, initialL);

            const float avgLum = luminance(r, g, b);

            // Spot removal: pull blown-out / shadowed pixels toward the
            // average color.
            const float spotLightMask =
                smoothStep(spotLightEdge0, spotLightEdge1, avgLum);
            const float spotDarkMask = smoothStep(spotDarkEdge0, darkSpot, avgLum);
            const float spotBlend = 1.0f - (1.0f - spotLightMask) * spotDarkMask;
            r += (avgMap.at(0, x, y) - r) * spotBlend;
            g += (avgMap.at(1, x, y) - g) * spotBlend;
            b += (avgMap.at(2, x, y) - b) * spotBlend;

            // Lighting removal over the high-pass against the average color.
            float highPassedR = r - avgMap.at(0, x, y);
            float highPassedG = g - avgMap.at(1, x, y);
            float highPassedB = b - avgMap.at(2, x, y);
            const float highPassedGrey =
                luminance(highPassedR, highPassedG, highPassedB);
            const float highMask = std::pow(
                std::clamp(highPassedGrey * 2.0f, 0.001f, 0.99f), lightPowExp);
            const float lowMask = std::pow(
                std::clamp(-highPassedGrey * 2.0f, 0.001f, 0.99f), darkPowExp);
            highPassedR += 0.5f;
            highPassedG += 0.5f;
            highPassedB += 0.5f;

            highPassedR += (highPassedR * (1.0f - params.removeLight) - highPassedR) * highMask;
            highPassedG += (highPassedG * (1.0f - params.removeLight) - highPassedG) * highMask;
            highPassedB += (highPassedB * (1.0f - params.removeLight) - highPassedB) * highMask;
            highPassedR += (1.0f - (1.0f - highPassedR) * (1.0f - params.removeShadow)
                               - highPassedR)
                * lowMask;
            highPassedG += (1.0f - (1.0f - highPassedG) * (1.0f - params.removeShadow)
                               - highPassedG)
                * lowMask;
            highPassedB += (1.0f - (1.0f - highPassedB) * (1.0f - params.removeShadow)
                               - highPassedB)
                * lowMask;

            float desaturateMask = highMask * params.removeLight
                + lowMask * params.removeShadow * 2.0f + spotBlend;
            desaturateMask = 1.0f - saturate(desaturateMask);

            // Detail re-injection (overlay).
            float overlayMask = 1.0f
                - (1.0f - highMask * params.removeLight) * (1.0f - lowMask * params.removeShadow);
            overlayMask = saturate(overlayMask * 2.0f + 0.1f);
            const float overlayR = linearDiffuse.at(0, x, y) - blurMap.at(0, x, y);
            const float overlayG = linearDiffuse.at(1, x, y) - blurMap.at(1, x, y);
            const float overlayB = linearDiffuse.at(2, x, y) - blurMap.at(2, x, y);
            highPassedR *= 1.0f
                + (overlayR * overlayContrast * 10.0f + 1.0f - 1.0f) * overlayMask;
            highPassedG *= 1.0f
                + (overlayG * overlayContrast * 10.0f + 1.0f - 1.0f) * overlayMask;
            highPassedB *= 1.0f
                + (overlayB * overlayContrast * 10.0f + 1.0f - 1.0f) * overlayMask;
            highPassedR = saturate(highPassedR);
            highPassedG = saturate(highPassedG);
            highPassedB = saturate(highPassedB);

            // Maintain color: restore the initial hue/saturation where
            // lighting was removed.
            float editedH = 0.0f;
            float editedS = 0.0f;
            float editedL = 0.0f;
            rgbToHsl(highPassedR, highPassedG, highPassedB, editedH, editedS, editedL);
            float restoredR = 0.0f;
            float restoredG = 0.0f;
            float restoredB = 0.0f;
            hslToRgb(initialH, initialS, editedL, restoredR, restoredG, restoredB);
            const float keepColor = params.keepOriginalColor * desaturateMask;
            r = highPassedR + (restoredR - highPassedR) * keepColor;
            g = highPassedG + (restoredG - highPassedG) * keepColor;
            b = highPassedB + (restoredB - highPassedB) * keepColor;

            // Brightness/contrast, then saturation.
            r = saturate((r - 0.5f) * params.finalContrast + 0.5f + params.finalBias);
            g = saturate((g - 0.5f) * params.finalContrast + 0.5f + params.finalBias);
            b = saturate((b - 0.5f) * params.finalContrast + 0.5f + params.finalBias);
            const float grey = luminance(r, g, b);
            r = grey + (r - grey) * params.saturation;
            g = grey + (g - grey) * params.saturation;
            b = grey + (b - grey) * params.saturation;

            edited.at(0, x, y) = saturate(r);
            edited.at(1, x, y) = saturate(g);
            edited.at(2, x, y) = saturate(b);
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(edited));
}

} // namespace Domain::Material::TextureProcess
