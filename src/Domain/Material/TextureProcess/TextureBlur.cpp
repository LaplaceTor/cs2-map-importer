#include "Domain/Material/TextureProcess/TextureBlur.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kAverageMapSize = 256;
constexpr int kAverageSamples = 32;
constexpr float kAverageSpreadScale = 64.0f;

float cosineTapWeight(int tapIndex, int samples)
{
    // fragBlur: cos(i / (2 * samples) * 2PI) * 0.5 + 0.5
    const float normalized = static_cast<float>(tapIndex) / static_cast<float>(samples * 2);
    return std::cos(normalized * 6.28318530718f) * 0.5f + 0.5f;
}

float applyContrast(float value, float contrast)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return std::clamp((clamped - 0.5f) * contrast + 0.5f, 0.0f, 1.0f);
}

} // namespace

namespace Domain::Material::TextureProcess {

float TextureBlur::extraSpreadFor(int width, int height)
{
    return (static_cast<float>(width) + static_cast<float>(height)) * 0.5f / 1024.0f;
}

Core::Result<TextureImage> TextureBlur::blurAxis(const TextureImage& source, Axis axis, int samples,
                                                 float spreadTexels, float contrast,
                                                 const Core::Async::CancellationToken& token,
                                                 const std::function<void(float)>& progress)
{
    return blurAxisResampled(source, axis, samples, spreadTexels, contrast, source.width(),
                             source.height(), token, progress);
}

Core::Result<TextureImage> TextureBlur::blurAxisResampled(
    const TextureImage& source, Axis axis, int samples, float stepTexels, float contrast,
    int destWidth, int destHeight, const Core::Async::CancellationToken& token,
    const std::function<void(float)>& progress)
{
    if (!source.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("TextureBlur", "source texture is not valid"));
    }
    if (samples <= 0 || destWidth <= 0 || destHeight <= 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("TextureBlur", "invalid blur parameters"));
    }

    std::vector<float> weights(static_cast<std::size_t>(samples) * 2 + 1);
    float weightSum = 0.0f;
    for (int i = -samples; i <= samples; ++i) {
        weights[static_cast<std::size_t>(i + samples)] = cosineTapWeight(i, samples);
        weightSum += weights[static_cast<std::size_t>(i + samples)];
    }

    const int srcMainSize = axis == Axis::Horizontal ? source.width() : source.height();
    const int srcOtherSize = axis == Axis::Horizontal ? source.height() : source.width();
    const int destMainSize = axis == Axis::Horizontal ? destWidth : destHeight;
    const int destOtherSize = axis == Axis::Horizontal ? destHeight : destWidth;
    const float mainScale = static_cast<float>(srcMainSize) / static_cast<float>(destMainSize);
    const float otherScale = static_cast<float>(srcOtherSize) / static_cast<float>(destOtherSize);

    TextureImage destination(destWidth, destHeight, source.channelCount());
    if (!destination.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("TextureBlur", "failed to allocate blur output"));
    }

    for (int other = 0; other < destOtherSize; ++other) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (other & 0x3F) == 0) {
            progress(static_cast<float>(other) / static_cast<float>(destOtherSize));
        }

        // Destination texel center mapped into source texel coordinates.
        const float otherPos = (static_cast<float>(other) + 0.5f) * otherScale - 0.5f;

        for (int main = 0; main < destMainSize; ++main) {
            const float mainPos = (static_cast<float>(main) + 0.5f) * mainScale - 0.5f;

            for (int channel = 0; channel < source.channelCount(); ++channel) {
                float accumulator = 0.0f;
                for (int i = -samples; i <= samples; ++i) {
                    const float tap = mainPos + static_cast<float>(i) * stepTexels;
                    const float sampled = axis == Axis::Horizontal
                        ? source.sampleBilinearWrapped(channel, tap, otherPos)
                        : source.sampleBilinearWrapped(channel, otherPos, tap);
                    accumulator += sampled * weights[static_cast<std::size_t>(i + samples)];
                }
                float value = accumulator / weightSum;
                if (contrast != 1.0f) {
                    value = applyContrast(value, contrast);
                }
                if (axis == Axis::Horizontal) {
                    destination.at(channel, main, other) = value;
                } else {
                    destination.at(channel, other, main) = value;
                }
            }
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(destination));
}

Core::Result<std::vector<TextureImage>> TextureBlur::buildFrequencyBands(
    const TextureImage& source, const Core::Async::CancellationToken& token,
    const std::function<void(float)>& progress)
{
    if (!source.isValid()) {
        return Core::Result<std::vector<TextureImage>>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("TextureBlur", "source texture is not valid"));
    }

    // Spread chain from the reference orchestration: 1, +e, +2e, +4e, +8e, +16e
    // accumulated onto the previous band's spread, cascading from band 0.
    const float extra = extraSpreadFor(source.width(), source.height());
    const std::array<float, 6> spreads = {
        1.0f,
        1.0f + extra,
        1.0f + extra + 2.0f * extra,
        1.0f + extra + 2.0f * extra + 4.0f * extra,
        1.0f + extra + 2.0f * extra + 4.0f * extra + 8.0f * extra,
        1.0f + extra + 2.0f * extra + 4.0f * extra + 8.0f * extra + 16.0f * extra,
    };

    std::vector<TextureImage> bands;
    bands.reserve(7);
    bands.push_back(source);

    for (int band = 1; band <= 6; ++band) {
        auto horizontal = blurAxisResampled(bands.back(), Axis::Horizontal, kFrequencySamples,
                                            spreads[static_cast<std::size_t>(band - 1)], 1.0f,
                                            source.width(), source.height(), token, {});
        if (!horizontal.isSuccess()) {
            return Core::Result<std::vector<TextureImage>>::failure(horizontal.error());
        }
        auto vertical = blurAxisResampled(horizontal.value(), Axis::Vertical, kFrequencySamples,
                                          spreads[static_cast<std::size_t>(band - 1)], 1.0f,
                                          source.width(), source.height(), token, {});
        if (vertical.isFailure()) {
            return Core::Result<std::vector<TextureImage>>::failure(vertical.error());
        }
        bands.push_back(std::move(vertical.value()));
        if (progress) {
            progress(static_cast<float>(band) / 6.0f);
        }
    }
    return Core::Result<std::vector<TextureImage>>::success(std::move(bands));
}

Core::Result<TextureImage> TextureBlur::buildAverageMap(
    const TextureImage& source, const Core::Async::CancellationToken& token,
    const std::function<void(float)>& progress)
{
    if (!source.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("TextureBlur", "source texture is not valid"));
    }

    const float extra = extraSpreadFor(source.width(), source.height());
    const float averageSpread = kAverageSpreadScale * extra;

    // Horizontal pass samples the full-resolution source and writes into the
    // 256x256 average target (downsampling happens through the tap sampling).
    auto horizontal = blurAxisResampled(source, Axis::Horizontal, kAverageSamples, averageSpread,
                                        1.0f, kAverageMapSize, kAverageMapSize, token, {});
    if (horizontal.isFailure()) {
        return horizontal;
    }

    // Vertical pass keeps operating on the 256x256 map, but offsets are still
    // derived from the full-resolution pixel size (UV-space step).
    const float verticalStep = averageSpread * static_cast<float>(kAverageMapSize)
        / static_cast<float>(source.height());
    auto vertical = blurAxisResampled(horizontal.value(), Axis::Vertical, kAverageSamples,
                                      verticalStep, 1.0f, kAverageMapSize, kAverageMapSize, token,
                                      {});
    if (vertical.isFailure()) {
        return vertical;
    }
    if (progress) {
        progress(1.0f);
    }
    return vertical;
}

} // namespace Domain::Material::TextureProcess
