#pragma once

#include <array>
#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief One HSL color-sample mask of height-from-diffuse ("Use Color Sample").
 *
 * Pixels whose HSL distance to @ref color falls inside [maskLow, maskHigh]
 * get their height pulled toward @ref height by @ref blendAmount (applied by
 * HeightGenerator). The picked color is expected in linear space, matching
 * what the reference shader samples. The @ref height default (0.5) mirrors
 * the reference's Sample1Height; HeightParams overrides sample2 to the
 * reference's Sample2Height ctor default of 0.3.
 */
struct HeightColorSample {
    bool enabled = false;
    float colorR = 0.0f;
    float colorG = 0.0f;
    float colorB = 0.0f;
    float hueWeight = 1.0f;
    float satWeight = 0.5f;
    float lumWeight = 0.2f;
    float maskLow = 0.0f;
    float maskHigh = 1.0f;
    float height = 0.5f;
    bool isolate = false;
};

/**
 * @brief Parameters of height-from-diffuse generation.
 *
 * Defaults mirror the reference tool's initial UI state (frequency-band
 * weights, per-band contrasts, final contrast/bias/gain). @ref finalGain is
 * the raw UI value; the pipeline remaps it internally (negative gain inverts
 * the curve) exactly like the reference implementation.
 *
 * Color samples are applied sample-2 first, then sample-1 (sample 1 wins on
 * overlap), mirroring Blit_Sample.shader; @ref sampleBlend scales both masks.
 */
struct HeightParams {
    std::array<float, 7> bandWeights = {0.15f, 0.19f, 0.30f, 0.50f, 0.70f, 0.90f, 1.00f};
    std::array<float, 7> bandContrasts = {{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};
    float finalContrast = 1.5f;
    float finalBias = 0.0f;
    float finalGain = 0.0f;

    HeightColorSample sample2{.height = 0.3f};
    HeightColorSample sample1;
    float sampleBlend = 0.5f;

    bool operator==(const HeightParams&) const = default;
};

/**
 * @brief Parameters of height-from-normal generation ("Use Normal" mode).
 *
 * The CPU quality dial (@ref iterations) replaces the reference's fixed 99
 * GPU iterations; sweep directions always cover the full circle.
 *
 * The accumulated result passes through the same final stage as
 * height-from-diffuse in the reference (fragCombineHeight, HeightFromNormal
 * branch): @ref finalContrast / @ref finalBias then the @ref finalGain curve
 * (finalGain is the raw UI value, remapped internally exactly like the
 * reference). Defaults mirror the reference height settings (contrast 1.5).
 */
struct HeightFromNormalParams {
    float spread = 50.0f;
    float spreadBoost = 1.0f;
    bool flipGreenChannel = false;
    int iterations = 32;
    float finalContrast = 1.5f;
    float finalBias = 0.0f;
    float finalGain = 0.0f;

    bool operator==(const HeightFromNormalParams&) const = default;
};

/**
 * @brief Generates a height map from a diffuse texture or from a normal map.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * "Height From Diffuse" pipeline — Assets/Shaders/Resources/Blit_Sample.shader
 * (desaturation, HSL color-sample masks, gamma re-encode) and Blit_Shader.shader
 * fragCombineHeight (frequency-band equalizer combine), orchestrated by
 * HeightFromDiffuseGui.cs; "Use Normal" mode ports Blit_Height_From_Normal.shader
 * with its 99-iteration progressive average.
 *
 * Inputs must be linear-space; the diffuse variant requires >= 3 channels.
 * Outputs are single-channel height images in [0, 1].
 */
class HeightGenerator {
public:
    static Core::Result<TextureImage> generateFromDiffuse(
        const TextureImage& linearDiffuse, const HeightParams& params,
        const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress = {});

    static Core::Result<TextureImage> generateFromNormal(
        const TextureImage& encodedNormal, const HeightFromNormalParams& params,
        const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
