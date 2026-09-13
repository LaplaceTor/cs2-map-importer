#pragma once

#include <array>
#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief Parameters of normal-from-height generation.
 *
 * Defaults mirror the reference tool's initial UI state. @ref preContrast is
 * the derivative strength ("Pre Contrast"), @ref shapeRecognition blends the
 * derivative normal toward the shape-from-diffuse reconstruction, and
 * @ref flipGreenChannel implements the "Maya style" green-channel convention
 * (off by default: +Y up / DirectX-style storage with green up).
 */
struct NormalParams {
    float preContrast = 20.0f;
    std::array<float, 7> bandWeights = {0.30f, 0.35f, 0.50f, 0.80f, 1.00f, 0.95f, 0.80f};
    float angularIntensity = 0.5f;
    float angularity = 0.0f;
    float finalContrast = 5.0f;
    bool flipGreenChannel = false;

    bool shapeFromColorSource = true;
    float shapeRecognition = 0.0f;
    float lightRotation = 0.0f;
    float shapeBias = 0.5f;
    int slopeBlurSamples = 50;

    bool operator==(const NormalParams&) const = default;
};

/**
 * @brief Generates a tangent-space normal map from a height map.
 *
 * C++/Qt port of Materialize (GPLv3, https://github.com/maikramer/Materialize):
 * "Normal From Height" pipeline — Blit_Shader.shader fragNormal (central
 * difference gradient + optional shape-from-diffuse) and fragCombineNormal
 * (frequency-band combine, angularity, final contrast), orchestrated by
 * NormalFromHeightGui.cs.
 *
 * The height input is a linear-space single-channel image; the optional color
 * source is the linear-space diffuse used for shape reconstruction. Output is
 * a 3-channel normal image with components encoded in [0, 1] (z up), ready to
 * be written as a data map (no sRGB encode).
 */
class NormalGenerator {
public:
    static Core::Result<TextureImage> generateFromHeight(
        const TextureImage& linearHeight, const TextureImage* linearColorSource,
        const NormalParams& params, const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
