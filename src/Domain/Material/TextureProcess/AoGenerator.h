#pragma once

#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief Parameters of ambient-occlusion generation.
 *
 * Defaults mirror the reference tool's initial UI state. @ref spread is the
 * occlusion radius in pixels ("AO pixel Spread", 10-100), @ref pixelDepth the
 * height relief scale for the geometric term ("Pixel Depth", 0-256) and
 * @ref normalDepthBlend the mix between the normal-horizon term (0) and the
 * depth term (1); the reference defaults it to 1 when a height map exists.
 *
 * The CPU quality dial (@ref iterations x @ref samplesPerIteration) replaces
 * the reference's fixed 99x50 GPU loop; iteration angles always cover the
 * full circle, so lower values reduce angular resolution only.
 */
struct AoParams {
    float spread = 50.0f;
    float pixelDepth = 100.0f;
    float normalDepthBlend = 1.0f;
    float aoPower = 1.0f;
    float aoBias = 0.0f;
    bool flipGreenChannel = false;

    int iterations = 32;
    int samplesPerIteration = 32;

    bool operator==(const AoParams&) const = default;
};

/**
 * @brief Generates an ambient-occlusion map from a tangent-space normal map
 *        plus an optional height map.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * "Normal + Depth to AO" pipeline — Blit_Shader.shader fragAO (rotating sweep
 * of radial samples: normal-horizon term + geometric depth term, progressively
 * averaged over iterations) and fragCombineAO (term blend, bias, power).
 * The deterministic per-sample randomizer hash is ported verbatim.
 *
 * The normal input is a [0, 1]-encoded tangent-space normal image (3 channels);
 * the height input is a linear-space single-plane image. Output is a
 * single-channel AO image in [0, 1] (1 = unoccluded).
 */
class AoGenerator {
public:
    static Core::Result<TextureImage> generate(const TextureImage& encodedNormal,
                                               const TextureImage* linearHeight,
                                               const AoParams& params,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
