#include "Domain/Material/TextureProcess/SmoothnessGenerator.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

#include "Domain/Material/TextureProcess/ColorMath.h"
#include "Domain/Material/TextureProcess/TextureBlur.h"

using Domain::Material::TextureProcess::ColorMath::luminance;
using Domain::Material::TextureProcess::ColorMath::rgbToHsl;
using Domain::Material::TextureProcess::ColorMath::saturate;
using Domain::Material::TextureProcess::ColorMath::smoothStep;

namespace {

// HSL sample mask of Blit_Smoothness.shader (circular hue distance).
float sampleMask(const Domain::Material::TextureProcess::SmoothnessColorSample& sample,
                 float sampleH, float sampleS, float sampleL, float h, float s, float l)
{
    const float hueDistance = std::min(
        std::min(std::abs(h - sampleH), std::abs((h + 1.0f) - sampleH)),
        std::abs((h - 1.0f) - sampleH));
    const float hueDif = 1.0f - hueDistance * 2.0f;
    const float satDif = 1.0f - std::abs(s - sampleS);
    const float lumDif = 1.0f - std::abs(l - sampleL);

    const float weightSum = sample.hueWeight + sample.satWeight + sample.lumWeight;
    if (weightSum <= 0.0f) {
        return 0.0f;
    }
    const float mask = (hueDif * sample.hueWeight + satDif * sample.satWeight
                           + lumDif * sample.lumWeight)
        / weightSum;
    return smoothStep(sample.maskLow, sample.maskHigh, mask);
}

// The H+V blur of the reference orchestration (spread 1.0, no contrast);
// blur sizes of 0 leave the image untouched (plain copy).
Core::Result<Domain::Material::TextureImage> blurredCopy(
    const Domain::Material::TextureImage& source, int blurSize,
    const Core::Async::CancellationToken& token)
{
    if (blurSize <= 0) {
        return Core::Result<Domain::Material::TextureImage>::success(source);
    }
    using TextureBlur = Domain::Material::TextureProcess::TextureBlur;
    auto horizontal = TextureBlur::blurAxis(source, TextureBlur::Axis::Horizontal, blurSize, 1.0f,
        1.0f, token, {});
    if (horizontal.isFailure()) {
        return horizontal;
    }
    return TextureBlur::blurAxis(horizontal.value(), TextureBlur::Axis::Vertical, blurSize, 1.0f,
        1.0f, token, {});
}

} // namespace

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> SmoothnessGenerator::generate(const TextureImage& linearDiffuse,
                                                         const TextureImage* metallicMap,
                                                         const SmoothnessParams& params,
                                                         const Core::Async::CancellationToken& token,
                                                         const std::function<void(float)>& progress)
{
    if (!linearDiffuse.isValid() || linearDiffuse.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SmoothnessGenerator",
                "smoothness generation requires a valid color image (3-4 channels)"));
    }
    if (metallicMap != nullptr && !metallicMap->isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SmoothnessGenerator", "metallic texture is not valid"));
    }
    if (metallicMap != nullptr
        && (metallicMap->width() != linearDiffuse.width()
            || metallicMap->height() != linearDiffuse.height())) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SmoothnessGenerator",
                "metallic texture size must match the diffuse texture"));
    }

    const int width = linearDiffuse.width();
    const int height = linearDiffuse.height();

    auto maskBlur = blurredCopy(linearDiffuse, params.blurSize, token);
    if (maskBlur.isFailure()) {
        return Core::Result<TextureImage>::failure(maskBlur.error());
    }
    if (progress) {
        progress(0.4f);
    }
    auto overlayBlur = blurredCopy(linearDiffuse, params.overlayBlurSize, token);
    if (overlayBlur.isFailure()) {
        return Core::Result<TextureImage>::failure(overlayBlur.error());
    }
    if (progress) {
        progress(0.8f);
    }

    float s1H = 0.0f, s1S = 0.0f, s1L = 0.0f;
    float s2H = 0.0f, s2S = 0.0f, s2L = 0.0f;
    if (params.sample1.enabled) {
        rgbToHsl(saturate(params.sample1.colorR), saturate(params.sample1.colorG),
            saturate(params.sample1.colorB), s1H, s1S, s1L);
    }
    if (params.sample2.enabled) {
        rgbToHsl(saturate(params.sample2.colorR), saturate(params.sample2.colorG),
            saturate(params.sample2.colorB), s2H, s2S, s2L);
    }

    TextureImage smoothness(width, height, 1);
    if (!smoothness.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("SmoothnessGenerator",
                "failed to allocate working buffers"));
    }

    const TextureImage& blurMap = maskBlur.value();
    const TextureImage& overlayMap = overlayBlur.value();

    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (y & 0x3F) == 0) {
            progress(0.8f + static_cast<float>(y) / static_cast<float>(height) * 0.2f);
        }
        for (int x = 0; x < width; ++x) {
            const float overlayGrey = luminance(linearDiffuse.at(0, x, y) - overlayMap.at(0, x, y),
                linearDiffuse.at(1, x, y) - overlayMap.at(1, x, y),
                linearDiffuse.at(2, x, y) - overlayMap.at(2, x, y));

            float h = 0.0f;
            float s = 0.0f;
            float l = 0.0f;
            rgbToHsl(saturate(blurMap.at(0, x, y)), saturate(blurMap.at(1, x, y)),
                saturate(blurMap.at(2, x, y)), h, s, l);

            float sample1Mask = 0.0f;
            float sample2Mask = 0.0f;
            if (params.sample2.enabled) {
                sample2Mask = sampleMask(params.sample2, s2H, s2S, s2L, h, s, l);
            }
            if (params.sample1.enabled) {
                sample1Mask = sampleMask(params.sample1, s1H, s1S, s1L, h, s, l);
            }
            // Without a metallic map the gate stays closed (reference binds a
            // flat "DefaultMetallicMap").
            const float metalMask =
                metallicMap != nullptr ? saturate(metallicMap->at(0, x, y)) : 0.0f;

            float value = params.baseSmoothness;
            value += (params.sample2.smoothness - value) * sample2Mask;
            value += (params.sample1.smoothness - value) * sample1Mask;
            value += (params.metalSmoothness - value) * metalMask;

            value = saturate((value - 0.5f) * params.finalContrast + 0.5f + params.finalBias);
            value *= std::clamp(overlayGrey * params.highPassOverlay + 1.0f, 0.0f, 10.0f);
            value = saturate(value);

            // Isolate overrides come last in the reference shader.
            if (params.sample1.isolate) {
                value = sample1Mask;
            } else if (params.sample2.isolate) {
                value = sample2Mask;
            }
            smoothness.at(0, x, y) = value;
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(smoothness));
}

} // namespace Domain::Material::TextureProcess
