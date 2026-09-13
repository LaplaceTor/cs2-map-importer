#include "Domain/Material/TextureImage.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Domain::Material {

TextureImage::TextureImage(int width, int height, int channelCount)
{
    if (width <= 0 || height <= 0 || !isChannelCountValid(channelCount)) {
        return; // stays empty and therefore invalid
    }
    m_width = width;
    m_height = height;
    m_channelCount = channelCount;
    m_data.assign(static_cast<std::size_t>(m_width) * m_height * m_channelCount, 0.0f);
}

bool TextureImage::isValid() const noexcept
{
    return m_width > 0 && m_height > 0 && isChannelCountValid(m_channelCount)
        && m_data.size() == static_cast<std::size_t>(m_width) * m_height * m_channelCount;
}

bool TextureImage::isChannelCountValid(int channelCount) noexcept
{
    return channelCount >= 1 && channelCount <= 4;
}

float* TextureImage::planeData(int channel) noexcept
{
    Q_ASSERT(channel >= 0 && channel < m_channelCount);
    return m_data.data() + static_cast<std::size_t>(channel) * m_width * m_height;
}

const float* TextureImage::planeData(int channel) const noexcept
{
    Q_ASSERT(channel >= 0 && channel < m_channelCount);
    return m_data.data() + static_cast<std::size_t>(channel) * m_width * m_height;
}

float& TextureImage::at(int channel, int x, int y) noexcept
{
    Q_ASSERT(channel >= 0 && channel < m_channelCount);
    Q_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height);
    return planeData(channel)[static_cast<std::size_t>(y) * m_width + x];
}

float TextureImage::at(int channel, int x, int y) const noexcept
{
    Q_ASSERT(channel >= 0 && channel < m_channelCount);
    Q_ASSERT(x >= 0 && x < m_width && y >= 0 && y < m_height);
    return planeData(channel)[static_cast<std::size_t>(y) * m_width + x];
}

void TextureImage::fill(float value) noexcept
{
    std::fill(m_data.begin(), m_data.end(), value);
}

float TextureImage::sampleBilinearWrapped(int channel, float texelX, float texelY) const noexcept
{
    Q_ASSERT(channel >= 0 && channel < m_channelCount);

    // Wrap into [0, size) so negative and out-of-range coordinates repeat,
    // mirroring the GPU repeat wrap mode of the reference pipeline.
    float wrappedX = texelX - m_width * std::floor(texelX / static_cast<float>(m_width));
    float wrappedY = texelY - m_height * std::floor(texelY / static_cast<float>(m_height));

    const int x0 = static_cast<int>(std::floor(wrappedX));
    const int y0 = static_cast<int>(std::floor(wrappedY));
    const int x1 = (x0 + 1) % m_width;
    const int y1 = (y0 + 1) % m_height;
    const int x0w = x0 % m_width;
    const int y0w = y0 % m_height;

    const float fracX = wrappedX - static_cast<float>(x0);
    const float fracY = wrappedY - static_cast<float>(y0);

    const float* plane = planeData(channel);
    const float p00 = plane[static_cast<std::size_t>(y0w) * m_width + x0w];
    const float p10 = plane[static_cast<std::size_t>(y0w) * m_width + x1];
    const float p01 = plane[static_cast<std::size_t>(y1) * m_width + x0w];
    const float p11 = plane[static_cast<std::size_t>(y1) * m_width + x1];

    const float top = p00 + (p10 - p00) * fracX;
    const float bottom = p01 + (p11 - p01) * fracX;
    return top + (bottom - top) * fracY;
}

bool TextureImage::operator==(const TextureImage& other) const
{
    return m_width == other.m_width && m_height == other.m_height
        && m_channelCount == other.m_channelCount && m_data == other.m_data;
}

bool TextureImage::operator!=(const TextureImage& other) const
{
    return !(*this == other);
}

} // namespace Domain::Material
