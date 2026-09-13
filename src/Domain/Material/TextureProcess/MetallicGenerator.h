#pragma once

#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief Parameters of metallic-from-diffuse generation.
 *
 * Defaults mirror the reference tool's initial UI state. The HSL distance is
 * measured between the (optionally blurred) diffuse and the picked metal
 * color, which is expected in linear space. @ref highPassOverlay scales the
 * high-pass detail re-injection (scratches/details get metal), and
 * @ref blurSize controls the blur applied before the HSL comparison
 * (0 = unblurred, matching the reference default).
 */
struct MetallicParams {
    float metalColorR = 0.0f;
    float metalColorG = 0.0f;
    float metalColorB = 0.0f;
    float hueWeight = 1.0f;
    float satWeight = 0.5f;
    float lumWeight = 0.2f;
    float maskLow = 0.0f;
    float maskHigh = 1.0f;
    int blurSize = 0;
    int overlayBlurSize = 30;
    float highPassOverlay = 1.0f;
    float finalContrast = 1.0f;
    float finalBias = 0.0f;

    bool operator==(const MetallicParams&) const = default;
};

/**
 * @brief Generates a metallic map from a diffuse texture.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * Blit_Metallic.shader fragMetallic — HSL-distance mask against the picked
 * metal color computed on the blurred diffuse, shaped by contrast/bias, then
 * multiplied by a high-pass detail overlay. Orchestrated by MetallicGui.cs.
 *
 * Input must be a linear-space color image (>= 3 channels). Output is a
 * single-channel metallic image in [0, 1].
 */
class MetallicGenerator {
public:
    static Core::Result<TextureImage> generate(const TextureImage& linearDiffuse,
                                               const MetallicParams& params,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
