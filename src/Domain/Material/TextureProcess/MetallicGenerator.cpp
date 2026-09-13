#include "Domain/Material/TextureProcess/MetallicGenerator.h"

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

Core::Result<TextureImage> MetallicGenerator::generate(const TextureImage& linearDiffuse,
                                                       const MetallicParams& params,
                                                       const Core::Async::CancellationToken& token,
                                                       const std::function<void(float)>& progress)
{
    if (!linearDiffuse.isValid() || linearDiffuse.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MetallicGenerator",
                "metallic generation requires a valid color image (3-4 channels)"));
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

    const float weightSum = params.hueWeight + params.satWeight + params.lumWeight;
    if (weightSum <= 0.0f) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MetallicGenerator", "HSL weights must not all be zero"));
    }

    float metalH = 0.0f;
    float metalS = 0.0f;
    float metalL = 0.0f;
    rgbToHsl(saturate(params.metalColorR), saturate(params.metalColorG),
        saturate(params.metalColorB), metalH, metalS, metalL);

    TextureImage metallic(width, height, 1);
    if (!metallic.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("MetallicGenerator",
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
            // High-pass detail overlay (grey of diffuse - blurred diffuse).
            const float overlayGrey = luminance(linearDiffuse.at(0, x, y) - overlayMap.at(0, x, y),
                linearDiffuse.at(1, x, y) - overlayMap.at(1, x, y),
                linearDiffuse.at(2, x, y) - overlayMap.at(2, x, y));

            // HSL distance on the blurred diffuse (linear space).
            float h = 0.0f;
            float s = 0.0f;
            float l = 0.0f;
            rgbToHsl(saturate(blurMap.at(0, x, y)), saturate(blurMap.at(1, x, y)),
                saturate(blurMap.at(2, x, y)), h, s, l);
            const float hueDistance = std::min(
                std::min(std::abs(h - metalH), std::abs((h + 1.0f) - metalH)),
                std::abs((h - 1.0f) - metalH));
            const float hueDif = 1.0f - hueDistance * 2.0f;
            const float satDif = 1.0f - std::abs(s - metalS);
            const float lumDif = 1.0f - std::abs(l - metalL);

            float mask =
                (hueDif * params.hueWeight + satDif * params.satWeight + lumDif * params.lumWeight)
                / weightSum;
            mask = smoothStep(params.maskLow, params.maskHigh, mask);

            float value = saturate((mask - 0.5f) * params.finalContrast + 0.5f + params.finalBias);
            value *= std::clamp(overlayGrey * params.highPassOverlay + 1.0f, 0.0f, 10.0f);
            metallic.at(0, x, y) = saturate(value);
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(metallic));
}

} // namespace Domain::Material::TextureProcess
