#pragma once

#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief One HSL color-sample of smoothness generation ("Use Color Sample").
 *
 * Pixels whose HSL distance to @ref color (measured on the optionally
 * blurred diffuse) falls inside [maskLow, maskHigh] get their smoothness
 * pulled toward @ref smoothness. Colors are expected in linear space. The
 * @ref smoothness default (0.5) mirrors the reference's Sample1Smoothness;
 * SmoothnessParams overrides sample2 to the reference's Sample2Smoothness
 * ctor default of 0.3.
 */
struct SmoothnessColorSample {
    bool enabled = false;
    float colorR = 0.0f;
    float colorG = 0.0f;
    float colorB = 0.0f;
    float hueWeight = 1.0f;
    float satWeight = 0.5f;
    float lumWeight = 0.2f;
    float maskLow = 0.0f;
    float maskHigh = 1.0f;
    float smoothness = 0.5f;
    bool isolate = false;
};

/**
 * @brief Parameters of smoothness-from-diffuse generation.
 *
 * Defaults mirror the reference tool's initial UI state: base smoothness
 * 0.1, metal smoothness 0.7 (gated by the optional metallic map, applied
 * last), sample 1 smoothness 0.5, sample 2 smoothness 0.3, high-pass overlay
 * strength 3.0. Note the reference works in smoothness space; roughness is
 * obtained downstream as 1 - smoothness.
 */
struct SmoothnessParams {
    float baseSmoothness = 0.1f;
    float metalSmoothness = 0.7f;
    SmoothnessColorSample sample2{.smoothness = 0.3f};
    SmoothnessColorSample sample1;
    int blurSize = 0;
    int overlayBlurSize = 30;
    float highPassOverlay = 3.0f;
    float finalContrast = 1.0f;
    float finalBias = 0.0f;

    bool operator==(const SmoothnessParams&) const = default;
};

/**
 * @brief Generates a smoothness map from a diffuse texture and an optional
 *        metallic map.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * Blit_Smoothness.shader fragSmoothness — base smoothness shaped by HSL
 * sample masks, gated by the metallic map, contrast/bias, then a high-pass
 * detail overlay; isolate overrides are applied last. Orchestrated by
 * SmoothnessGui.cs.
 *
 * Input must be a linear-space color image (>= 3 channels); the metallic
 * input is a single-channel (or >= 1 channel, plane 0) image in [0, 1].
 * Output is a single-channel smoothness image in [0, 1].
 */
class SmoothnessGenerator {
public:
    static Core::Result<TextureImage> generate(const TextureImage& linearDiffuse,
                                               const TextureImage* metallicMap,
                                               const SmoothnessParams& params,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
