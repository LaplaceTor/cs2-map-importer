#pragma once

#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"

#include "Domain/Material/TextureImage.h"

namespace Domain::Material::TextureProcess {

/**
 * @brief How to extract a scalar value from a source image for packing.
 */
enum class PackSource {
    Luminance, // 0.3 R + 0.5 G + 0.2 B (plane 0 for grayscale sources)
    Red,
    Green,
    Blue,
    Alpha,
};

/**
 * @brief One output-channel assignment; a null image leaves the channel at
 *        its default (0 for RGB, @ref ChannelPackParams defaultAlpha for A).
 */
struct PackChannel {
    const TextureImage* image = nullptr;
    PackSource source = PackSource::Luminance;
};

/**
 * @brief Channel assignments for packing. All assigned images must share the
 *        same dimensions.
 */
struct ChannelPackParams {
    PackChannel red;
    PackChannel green;
    PackChannel blue;
    PackChannel alpha;
    float defaultAlpha = 1.0f;
};

/**
 * @brief Packs existing maps into a single image, one source per channel
 *        (Materialize "Save Property Map" equivalent, CPU path).
 *
 * This is the generic form of game-convention packed maps (e.g. a CS2 MRAO
 * preset would assign metallic -> R, roughness -> G, AO -> B). Source images
 * are referenced, not copied; the output is a new 3-channel image, or
 * 4-channel when an alpha assignment is present.
 */
class ChannelPacker {
public:
    static Core::Result<TextureImage> pack(const ChannelPackParams& params,
                                           const Core::Async::CancellationToken& token,
                                           const std::function<void(float)>& progress = {});
};

} // namespace Domain::Material::TextureProcess
