#pragma once

#include <functional>
#include <vector>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief Separable cosine-window blur and the frequency-band pyramid built
 *        from it.
 *
 * C++/Qt port of the blur math in Materialize (GPLv3,
 * https://github.com/maikramer/Materialize), file
 * Assets/Shaders/Resources/Blit_Shader.shader (fragBlur) plus the blur-chain
 * orchestration of HeightFromDiffuseGui.cs / NormalFromHeightGui.cs.
 *
 * The blur samples with repeat wrapping and bilinear filtering, exactly like
 * the reference GPU pipeline; the frequency pyramid cascades H+V blur passes
 * with growing spread over the previous band's output.
 */
class TextureBlur {
public:
    enum class Axis { Horizontal, Vertical };

    /// Canonical tap count used by the frequency chains.
    static constexpr int kFrequencySamples = 4;

    /// Builds one axis of the blur: per-tap offset is @p spreadTexels in
    /// source texel units; @p contrast is the optional post-blur contrast
    /// (1.0 = disabled). Output size equals the source size.
    static Core::Result<TextureImage> blurAxis(const TextureImage& source, Axis axis, int samples,
                                               float spreadTexels, float contrast,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress = {});

    /**
     * @brief The 7-band frequency pyramid shared by height/normal/edge
     *        generation: band 0 is the source itself, bands 1..6 are the
     *        cascading blurs with the canonical spread chain.
     */
    static Core::Result<std::vector<TextureImage>> buildFrequencyBands(
        const TextureImage& source, const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress = {});

    /**
     * @brief The 256x256 large-radius average map (the low-frequency
     *        reference of height-from-diffuse).
     */
    static Core::Result<TextureImage> buildAverageMap(
        const TextureImage& source, const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress = {});

    /// (w + h) * 0.5 / 1024 — the reference pipeline's spread unit.
    static float extraSpreadFor(int width, int height);

private:
    /// Blur with an explicit destination size; @p stepTexels is expressed in
    /// SOURCE texel units (matches the reference shader's UV-space offsets).
    static Core::Result<TextureImage> blurAxisResampled(
        const TextureImage& source, Axis axis, int samples, float stepTexels, float contrast,
        int destWidth, int destHeight, const Core::Async::CancellationToken& token,
        const std::function<void(float)>& progress);
};

} // namespace Domain::Material::TextureProcess
