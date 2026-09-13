#include "Domain/Material/TextureProcess/HeightGenerator.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Domain/Material/TextureProcess/ColorMath.h"
#include "Domain/Material/TextureProcess/TextureBlur.h"

using Domain::Material::TextureProcess::ColorMath::rgbToHsl;
using Domain::Material::TextureProcess::ColorMath::sampleRandom;
using Domain::Material::TextureProcess::ColorMath::smoothStep;

namespace {

// HSL color-sample mask of Blit_Sample.shader (circular hue distance).
float sampleMask(const Domain::Material::TextureProcess::HeightColorSample& sample,
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

float remapFinalGain(float uiGain)
{
    // HeightFromDiffuseGui: negative UI gain inverts the curve via 1/(g-1).
    if (uiGain < 0.0f) {
        return std::abs(1.0f / (uiGain - 1.0f));
    }
    return uiGain + 1.0f;
}

float applyFinalGain(float height, float realGain)
{
    if (height > 0.5f) {
        return std::pow(std::clamp(height * 2.0f - 1.0f, 0.0f, 1.0f), realGain) * 0.5f + 0.5f;
    }
    return 1.0f
        - (std::pow(std::clamp((1.0f - height) * 2.0f - 1.0f, 0.0f, 1.0f), realGain) * 0.5f + 0.5f);
}

} // namespace

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> HeightGenerator::generateFromDiffuse(
    const TextureImage& linearDiffuse, const HeightParams& params,
    const Core::Async::CancellationToken& token, const std::function<void(float)>& progress)
{
    if (!linearDiffuse.isValid() || linearDiffuse.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("HeightGenerator",
                "height-from-diffuse requires a valid color image (3-4 channels)"));
    }

    const int width = linearDiffuse.width();
    const int height = linearDiffuse.height();

    // Step 1 — desaturate, optional HSL color-sample masks, and re-encode
    // (Blit_Sample fragSample): height0 = pow(finalHeight, 2.2). The reference
    // works on the luminance of the sRGB-decoded (linear) diffuse.
    TextureImage height0(width, height, 1);
    if (!height0.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("HeightGenerator", "failed to allocate working buffers"));
    }
    {
        const float* r = linearDiffuse.planeData(0);
        const float* g = linearDiffuse.planeData(1);
        const float* b = linearDiffuse.planeData(2);
        float* out = height0.planeData(0);
        const qsizetype pixelCount = static_cast<qsizetype>(width) * height;

        // Picked colors are pre-converted to HSL once.
        float s1H = 0.0f, s1S = 0.0f, s1L = 0.0f;
        float s2H = 0.0f, s2S = 0.0f, s2L = 0.0f;
        if (params.sample1.enabled) {
            rgbToHsl(params.sample1.colorR, params.sample1.colorG, params.sample1.colorB, s1H, s1S,
                s1L);
        }
        if (params.sample2.enabled) {
            rgbToHsl(params.sample2.colorR, params.sample2.colorG, params.sample2.colorB, s2H, s2S,
                s2L);
        }

        for (qsizetype i = 0; i < pixelCount; ++i) {
            if (token.isCancelled()) {
                return Core::Result<TextureImage>::cancelled();
            }
            const float lum = std::clamp(r[i] * 0.3f + g[i] * 0.5f + b[i] * 0.2f, 0.0f, 1.0f);

            float finalHeight = lum;
            float mask1 = 0.0f;
            float mask2 = 0.0f;
            if (params.sample1.enabled || params.sample2.enabled) {
                float h = 0.0f, s = 0.0f, l = 0.0f;
                rgbToHsl(std::clamp(r[i], 0.0f, 1.0f), std::clamp(g[i], 0.0f, 1.0f),
                    std::clamp(b[i], 0.0f, 1.0f), h, s, l);
                if (params.sample2.enabled) {
                    mask2 = sampleMask(params.sample2, s2H, s2S, s2L, h, s, l);
                    finalHeight += (params.sample2.height - finalHeight)
                        * (mask2 * std::clamp(params.sampleBlend, 0.0f, 1.0f));
                }
                if (params.sample1.enabled) {
                    mask1 = sampleMask(params.sample1, s1H, s1S, s1L, h, s, l);
                    finalHeight += (params.sample1.height - finalHeight)
                        * (mask1 * std::clamp(params.sampleBlend, 0.0f, 1.0f));
                }
            }
            // Isolate overrides are not gated by the sample enable flags in the
            // reference shader (fragSample checks _IsolateSample* on the raw
            // masks, which are 0 while the sample is disabled -> black output).
            if (params.sample1.isolate) {
                finalHeight = mask1;
            } else if (params.sample2.isolate) {
                finalHeight = mask2;
            }
            out[i] = std::pow(std::clamp(finalHeight, 0.0f, 1.0f), 2.2f);
        }
    }
    if (progress) {
        progress(0.1f);
    }

    // Step 2 — 7-band frequency pyramid.
    auto bands = TextureBlur::buildFrequencyBands(height0, token, [&](float fraction) {
        if (progress) {
            progress(0.1f + fraction * 0.45f);
        }
    });
    if (!bands.isSuccess()) {
        return Core::Result<TextureImage>::failure(bands.error());
    }

    // Step 3 — large-radius average map.
    auto average = TextureBlur::buildAverageMap(bands.value()[6], token, [&](float fraction) {
        if (progress) {
            progress(0.55f + fraction * 0.15f);
        }
    });
    if (!average.isSuccess()) {
        return Core::Result<TextureImage>::failure(average.error());
    }
    if (progress) {
        progress(0.7f);
    }

    // Step 4 — frequency equalizer combine (fragCombineHeight).
    TextureImage combined(width, height, 1);
    if (!combined.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("HeightGenerator", "failed to allocate working buffers"));
    }

    float weightSum = 0.0f;
    for (int band = 0; band < 7; ++band) {
        weightSum += std::max(params.bandWeights[static_cast<std::size_t>(band)], 0.0f);
    }
    if (weightSum <= 0.0f) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("HeightGenerator", "band weights must not all be zero"));
    }

    const float realGain = remapFinalGain(params.finalGain);
    const int avgSize = average.value().width();
    const float avgToUv = static_cast<float>(avgSize);

    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (y & 0x3F) == 0) {
            progress(0.7f + static_cast<float>(y) / static_cast<float>(height) * 0.3f);
        }
        const float vTexel = (static_cast<float>(y) + 0.5f) / static_cast<float>(height) * avgToUv
            - 0.5f;
        for (int x = 0; x < width; ++x) {
            const float uTexel = (static_cast<float>(x) + 0.5f) / static_cast<float>(width)
                    * avgToUv
                - 0.5f;
            float avgValue = average.value().sampleBilinearWrapped(0, uTexel, vTexel);
            avgValue = std::pow(std::clamp(avgValue, 0.0f, 1.0f), 0.45f);

            float accumulator = 0.0f;
            for (int band = 0; band < 7; ++band) {
                const float weight = std::max(
                    params.bandWeights[static_cast<std::size_t>(band)], 0.0f);
                const float bandValue = std::pow(
                    std::clamp(bands.value()[static_cast<std::size_t>(band)].at(0, x, y), 0.0f,
                        1.0f),
                    0.45f);
                const float contrast = params.bandContrasts[static_cast<std::size_t>(band)];
                accumulator += ((bandValue - avgValue) * contrast + 0.5f) * weight;
            }

            float h = accumulator / weightSum;
            h = std::clamp((h - 0.5f) * params.finalContrast + 0.5f + params.finalBias, 0.0f, 1.0f);
            combined.at(0, x, y) = applyFinalGain(h, realGain);
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(combined));
}

Core::Result<TextureImage> HeightGenerator::generateFromNormal(
    const TextureImage& encodedNormal, const HeightFromNormalParams& params,
    const Core::Async::CancellationToken& token, const std::function<void(float)>& progress)
{
    if (!encodedNormal.isValid() || encodedNormal.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("HeightGenerator",
                "height-from-normal requires a valid normal image (3 channels)"));
    }
    if (params.iterations <= 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("HeightGenerator", "iteration count must be positive"));
    }

    const int width = encodedNormal.width();
    const int height = encodedNormal.height();
    const int samples = std::max(static_cast<int>(params.spread), 1);
    const float uScale = static_cast<float>(width);
    const float vScale = static_cast<float>(height);

    // Progressive accumulation (Blit_Height_From_Normal + the reference's
    // 99-iteration loop; sweep directions always cover the full circle).
    TextureImage accumulation(width, height, 1);
    if (!accumulation.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("HeightGenerator", "failed to allocate working buffers"));
    }

    for (int iteration = 1; iteration <= params.iterations; ++iteration) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress) {
            progress(static_cast<float>(iteration - 1) / static_cast<float>(params.iterations));
        }
        const float iterationProgress =
            static_cast<float>(iteration) / static_cast<float>(params.iterations);
        const float directionX = std::sin(iterationProgress * 6.28318530718f);
        const float directionY = std::cos(iterationProgress * 6.28318530718f);
        const float blendAmount = 1.0f / static_cast<float>(iteration);

        for (int y = 0; y < height; ++y) {
            if (token.isCancelled()) {
                return Core::Result<TextureImage>::cancelled();
            }
            const float v = (static_cast<float>(y) + 0.5f) / vScale;
            for (int x = 0; x < width; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / uScale;
                float ao = 0.0f;

                for (int sample = 1; sample <= samples; ++sample) {
                    const float passProgress = static_cast<float>(sample)
                        / static_cast<float>(samples);
                    const float randomizerX =
                        sampleRandom(u, v, static_cast<float>(sample)) * passProgress * 0.1f;
                    const float randomizerY =
                        sampleRandom(v, u, static_cast<float>(sample)) * passProgress * 0.1f;
                    const float offsetX = directionX * params.spread
                            * (passProgress * params.spreadBoost)
                        + randomizerX;
                    const float offsetY = directionY * params.spread
                            * (passProgress * params.spreadBoost)
                        + randomizerY;
                    const float offsetLength =
                        std::sqrt(offsetX * offsetX + offsetY * offsetY);
                    if (offsetLength <= 0.0f) {
                        continue;
                    }
                    const float trueDirX = offsetX / offsetLength;
                    const float trueDirY = offsetY / offsetLength;

                    const float baseX = u * uScale - 0.5f;
                    const float baseY = v * vScale - 0.5f;
                    const float sampleNx =
                        encodedNormal.sampleBilinearWrapped(0, baseX + offsetX, baseY + offsetY)
                            * 2.0f
                        - 1.0f;
                    const float sampleNy =
                        encodedNormal.sampleBilinearWrapped(1, baseX + offsetX, baseY + offsetY)
                            * 2.0f
                        - 1.0f;
                    const float sampleNz =
                        encodedNormal.sampleBilinearWrapped(2, baseX + offsetX, baseY + offsetY)
                            * 2.0f
                        - 1.0f;
                    // Stored +Y up normals are decoded with flipped Y unless the
                    // Maya convention is active (fragHeight flipTex).
                    const float flippedNy = params.flipGreenChannel ? sampleNy : -sampleNy;

                    ao += sampleNx * trueDirX + flippedNy * trueDirY;
                }

                ao = ao / static_cast<float>(samples)
                    * (static_cast<float>(samples) * params.spreadBoost / 50.0f);
                ao = ao * 0.5f + 0.5f;
                accumulation.at(0, x, y) =
                    accumulation.at(0, x, y) + (ao - accumulation.at(0, x, y)) * blendAmount;
            }
        }
    }
    // Final stage — the reference applies the height settings' contrast, bias
    // and gain to the accumulated result (fragCombineHeight, HeightFromNormal
    // branch; HeightFromDiffuseGui sets the uniforms before the pass-2 blit).
    const float realGain = remapFinalGain(params.finalGain);
    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        for (int x = 0; x < width; ++x) {
            const float value = std::clamp(
                (accumulation.at(0, x, y) - 0.5f) * params.finalContrast + 0.5f + params.finalBias,
                0.0f, 1.0f);
            accumulation.at(0, x, y) = applyFinalGain(value, realGain);
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(accumulation));
}

} // namespace Domain::Material::TextureProcess
