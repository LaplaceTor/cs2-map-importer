#include "Domain/Material/TextureProcess/AoGenerator.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

#include "Domain/Material/TextureProcess/ColorMath.h"

using Domain::Material::TextureProcess::ColorMath::sampleRandom;
using Domain::Material::TextureProcess::ColorMath::saturate;

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> AoGenerator::generate(const TextureImage& encodedNormal,
                                                 const TextureImage* linearHeight,
                                                 const AoParams& params,
                                                 const Core::Async::CancellationToken& token,
                                                 const std::function<void(float)>& progress)
{
    if (!encodedNormal.isValid() || encodedNormal.channelCount() < 3) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AoGenerator",
                "AO generation requires a valid normal image (3 channels)"));
    }
    if (params.iterations <= 0 || params.samplesPerIteration <= 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AoGenerator",
                "iteration and sample counts must be positive"));
    }
    if (linearHeight != nullptr && !linearHeight->isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AoGenerator", "height texture is not valid"));
    }

    const int width = encodedNormal.width();
    const int height = encodedNormal.height();
    const bool hasHeight = linearHeight != nullptr;
    // Without a height map the depth term is unavailable; the reference UI
    // forces the blend to the normal term in that case.
    const float termBlend = hasHeight ? std::clamp(params.normalDepthBlend, 0.0f, 1.0f) : 0.0f;

    // Progressive accumulation planes (RG pair of the reference pipeline):
    // normal-horizon AO in plane 0, geometric depth AO in plane 1.
    TextureImage accumulation(width, height, 2);
    if (!accumulation.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("AoGenerator", "failed to allocate working buffers"));
    }

    const float uScale = static_cast<float>(width);
    const float vScale = static_cast<float>(height);

    for (int iteration = 1; iteration <= params.iterations; ++iteration) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress) {
            progress(static_cast<float>(iteration - 1) / static_cast<float>(params.iterations));
        }

        // Full-circle angular coverage; the reference spreads 99 steps over
        // 2PI, we compress the same circle into the iteration budget.
        const float iterationProgress = static_cast<float>(iteration)
            / static_cast<float>(params.iterations);
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

                const float mainHeight = hasHeight ? linearHeight->at(0, x, y) : 0.0f;
                float normalAo = 0.0f;
                float normalAccum = 0.0f;
                float depthAo = 0.0f;

                for (int sample = 1; sample <= params.samplesPerIteration; ++sample) {
                    const float passProgress = static_cast<float>(sample)
                        / static_cast<float>(params.samplesPerIteration);
                    const float randomizerX =
                        sampleRandom(u, v, static_cast<float>(sample)) * passProgress * 0.1f;
                    const float randomizerY =
                        sampleRandom(v, u, static_cast<float>(sample)) * passProgress * 0.1f;

                    const float offsetX = directionX * params.spread * passProgress + randomizerX;
                    const float offsetY = directionY * params.spread * passProgress + randomizerY;
                    const float offsetLength =
                        std::sqrt(offsetX * offsetX + offsetY * offsetY);
                    if (offsetLength <= 0.0f) {
                        continue;
                    }
                    const float trueDirX = offsetX / offsetLength;
                    const float trueDirY = offsetY / offsetLength;

                    // Sampling in texel units: UV offset of N pixels == N texels.
                    // The normal's z is not sampled: the reference dots against
                    // (trueDir, 0), so only the xy slope terms matter.
                    const float sampleNx = encodedNormal.sampleBilinearWrapped(0, u * uScale - 0.5f + offsetX, v * vScale - 0.5f + offsetY) * 2.0f - 1.0f;
                    const float sampleNy = encodedNormal.sampleBilinearWrapped(1, u * uScale - 0.5f + offsetX, v * vScale - 0.5f + offsetY) * 2.0f - 1.0f;
                    // Stored +Y up normals are decoded with flipped Y unless the
                    // Maya convention is active (fragAO flipTex). A zero-length
                    // sample normal contributes 0 to the numerator while its
                    // importance still counts in the denominator, exactly like
                    // the reference shader (no per-sample normalization).
                    const float flippedNy = params.flipGreenChannel ? sampleNy : -sampleNy;

                    // Normal-horizon term with sqrt(1-progress) importance.
                    const float importance = std::sqrt(1.0f - passProgress);
                    normalAo += (sampleNx * trueDirX + flippedNy * trueDirY) * importance;
                    normalAccum += importance;

                    // Geometric depth term.
                    if (hasHeight) {
                        const float sampleHeight = linearHeight->sampleBilinearWrapped(
                            0, u * uScale - 0.5f + offsetX, v * vScale - 0.5f + offsetY);
                        const float posZ = (sampleHeight - mainHeight) * params.pixelDepth;
                        const float posX = trueDirX * params.spread * passProgress;
                        const float posY = trueDirY * params.spread * passProgress;
                        const float posLength =
                            std::sqrt(posX * posX + posY * posY + posZ * posZ);
                        if (posLength > 0.0f) {
                            const float sampleAo = saturate(posZ / posLength);
                            const float sampleDist = saturate(posLength * 0.1f);
                            depthAo = std::max(sampleAo * sampleDist, depthAo);
                        }
                    }
                }

                if (normalAccum > 0.0f) {
                    normalAo /= normalAccum;
                }
                const float aoX1 = saturate(normalAo + 1.0f);
                const float aoX2 = saturate(normalAo + 0.5f);
                normalAo = std::sqrt(std::pow(aoX1, 5.0f) * std::pow(aoX2, 0.2f));
                const float depthInverted = 1.0f - depthAo;

                accumulation.at(0, x, y) = accumulation.at(0, x, y)
                    + (normalAo - accumulation.at(0, x, y)) * blendAmount;
                accumulation.at(1, x, y) = accumulation.at(1, x, y)
                    + (depthInverted - accumulation.at(1, x, y)) * blendAmount;
            }
        }
    }
    if (progress) {
        progress(0.95f);
    }

    // Combine (fragCombineAO): term blend, bias, power.
    TextureImage combined(width, height, 1);
    if (!combined.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("AoGenerator", "failed to allocate working buffers"));
    }
    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        for (int x = 0; x < width; ++x) {
            float ao = accumulation.at(0, x, y)
                + (accumulation.at(1, x, y) - accumulation.at(0, x, y)) * termBlend;
            ao += params.aoBias;
            // The reference pow()s the biased value directly; negative inputs
            // are clamped here to avoid NaNs (GPUs saturate NaN to 0 anyway).
            ao = std::pow(saturate(ao), params.aoPower);
            combined.at(0, x, y) = saturate(ao);
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(combined));
}

} // namespace Domain::Material::TextureProcess
