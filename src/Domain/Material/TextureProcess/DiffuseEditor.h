#pragma once

#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief Parameters of the diffuse edit pass (color texture pre-processing).
 *
 * Defaults mirror the reference tool's initial UI state; with the defaults
 * the pass is a near passthrough (mild overlay re-injection and half-strength
 * color preservation). All removable-lighting strengths are 0 by default.
 * Names follow the reference UI: "Hot Spot Removal"/"Dark Spot Removal" pull
 * blown-out/shadowed pixels toward the local average color, "Remove Light"/
 * "Remove Shadow" attenuate lighting gradients, "Keep Original Color"
 * restores the initial hue/saturation where lighting was removed, and
 * "Saturation" scales it at the end.
 */
struct DiffuseEditParams {
    int avgColorBlurSize = 50;        // 5-100
    int overlayBlurSize = 20;         // 5-100 ("Overlay Blur Size")
    float overlayBlurContrast = 0.0f; // -1..1 ("Overlay Blur Contrast")
    float lightMaskPower = 0.5f;      // 0-1
    float removeLight = 0.0f;         // 0-1 ("Remove Light")
    float shadowMaskPower = 0.5f;     // 0-1
    float removeShadow = 0.0f;        // 0-1 ("Remove Shadow")
    float hotSpotRemoval = 0.0f;      // 0-1
    float darkSpotRemoval = 0.0f;     // 0-1
    float finalContrast = 1.0f;       // -2..2
    float finalBias = 0.0f;           // -0.5..0.5
    float keepOriginalColor = 0.5f;   // 0-1 ("Keep Original Color")
    float saturation = 1.0f;          // 0-1

    bool operator==(const DiffuseEditParams&) const = default;
};

/**
 * @brief Edits a diffuse (color) texture: removes lighting gradients, hot
 *        spots and shadow spots, re-injects detail, then adjusts saturation
 *        and contrast.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * Blit_Shader.shader fragEditDiffuse (pass 11), orchestrated by
 * EditDiffuseGui.cs (overlay blur + two-round average-color blur).
 *
 * Input must be a linear-space color image (>= 3 channels); the output is a
 * same-size linear color image.
 */
class DiffuseEditor {
public:
    static Core::Result<TextureImage> edit(const TextureImage& linearDiffuse,
                                           const DiffuseEditParams& params,
                                           const Core::Async::CancellationToken& token,
                                           const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
