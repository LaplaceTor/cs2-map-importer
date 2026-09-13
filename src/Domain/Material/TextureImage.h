#pragma once

#include <vector>

namespace Domain::Material {

/**
 * @brief Planar float32 pixel buffer used as the working image for texture
 *        generation pipelines.
 *
 * Channels are stored as separate planes (channel-major, row-major within a
 * plane) so grayscale pipelines (height, AO, roughness...) only pay for one
 * plane. All pixel values are expected in the [0, 1] range unless a pipeline
 * stage documents otherwise; working color space is linear.
 *
 * This is a plain value type: no Qt dependency, no third-party types.
 */
class TextureImage {
public:
    TextureImage() = default;
    TextureImage(int width, int height, int channelCount);

    int width() const noexcept { return m_width; }
    int height() const noexcept { return m_height; }
    int channelCount() const noexcept { return m_channelCount; }

    /// True when allocated with a usable geometry (1..4 channels, non-zero size).
    bool isValid() const noexcept;

    [[nodiscard]] static bool isChannelCountValid(int channelCount) noexcept;

    /// Direct pointer access to a channel plane (row-major, stride == width).
    float* planeData(int channel) noexcept;
    const float* planeData(int channel) const noexcept;

    /// Linear index access within a plane.
    float& at(int channel, int x, int y) noexcept;
    float at(int channel, int x, int y) const noexcept;

    void fill(float value) noexcept;

    /**
     * @brief Bilinear sample with repeat wrapping, in texel coordinates.
     *
     * Texel centers live at integer coordinates (texel (0,0) covers [0,1)).
     * Coordinates outside the image wrap modulo the plane size, matching the
     * repeat wrap mode the original GPU pipeline renders with.
     */
    float sampleBilinearWrapped(int channel, float texelX, float texelY) const noexcept;

    bool operator==(const TextureImage& other) const;
    bool operator!=(const TextureImage& other) const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_channelCount = 0;
    std::vector<float> m_data;
};

} // namespace Domain::Material
