#include "Domain/Material/TextureProcess/ChannelPacker.h"

#include <QCoreApplication>

#include <algorithm>

#include "Domain/Material/TextureProcess/ColorMath.h"

namespace {

using Domain::Material::TextureImage;
using Domain::Material::TextureProcess::ColorMath::luminance;
using Domain::Material::TextureProcess::PackChannel;
using Domain::Material::TextureProcess::PackSource;

int planeIndexOf(PackSource source, int channelCount, bool& ok)
{
    ok = true;
    switch (source) {
        case PackSource::Red:
            ok = channelCount >= 1;
            return 0;
        case PackSource::Green:
            ok = channelCount >= 2;
            return 1;
        case PackSource::Blue:
            ok = channelCount >= 3;
            return 2;
        case PackSource::Alpha:
            ok = channelCount >= 4;
            return 3;
        case PackSource::Luminance:
            return 0;
    }
    ok = false;
    return 0;
}

float extractScalar(const TextureImage& image, PackSource source, int x, int y)
{
    if (source == PackSource::Luminance) {
        return image.channelCount() >= 3
            ? luminance(image.at(0, x, y), image.at(1, x, y), image.at(2, x, y))
            : image.at(0, x, y);
    }
    bool ok = false;
    const int plane = planeIndexOf(source, image.channelCount(), ok);
    return ok ? image.at(plane, x, y) : 0.0f;
}

} // namespace

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> ChannelPacker::pack(const ChannelPackParams& params,
                                               const Core::Async::CancellationToken& token,
                                               const std::function<void(float)>& progress)
{
    const PackChannel* assignments[4] = {&params.red, &params.green, &params.blue, &params.alpha};

    int assignedCount = 0;
    for (const auto* channel : assignments) {
        if (channel->image != nullptr) {
            ++assignedCount;
        }
    }
    if (assignedCount == 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("ChannelPacker", "at least one channel must be assigned"));
    }

    // Validate geometry: all assigned images must be valid and share size.
    int width = 0;
    int height = 0;
    for (const auto* channel : assignments) {
        if (channel->image == nullptr) {
            continue;
        }
        if (!channel->image->isValid()) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("ChannelPacker",
                    "a packed source image is not valid"));
        }
        if (width == 0) {
            width = channel->image->width();
            height = channel->image->height();
        } else if (channel->image->width() != width || channel->image->height() != height) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("ChannelPacker",
                    "packed source images must share the same dimensions"));
        }
    }

    // Channel requirement check (e.g. Alpha needs a 4-channel source).
    for (const auto* channel : assignments) {
        if (channel->image == nullptr) {
            continue;
        }
        bool ok = false;
        planeIndexOf(channel->source, channel->image->channelCount(), ok);
        if (!ok) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("ChannelPacker",
                    "a source image lacks the requested channel"));
        }
    }

    const int outChannels = params.alpha.image != nullptr ? 4 : 3;
    TextureImage packed(width, height, outChannels);
    if (!packed.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("ChannelPacker", "failed to allocate packed output"));
    }

    for (int channel = 0; channel < outChannels; ++channel) {
        const auto* assignment = assignments[channel];
        const float defaultValue = channel == 3 ? params.defaultAlpha : 0.0f;
        for (int y = 0; y < height; ++y) {
            if (token.isCancelled()) {
                return Core::Result<TextureImage>::cancelled();
            }
            for (int x = 0; x < width; ++x) {
                packed.at(channel, x, y) = assignment->image != nullptr
                    ? extractScalar(*assignment->image, assignment->source, x, y)
                    : defaultValue;
            }
        }
        if (progress) {
            progress(static_cast<float>(channel + 1) / static_cast<float>(outChannels));
        }
    }
    return Core::Result<TextureImage>::success(std::move(packed));
}

} // namespace Domain::Material::TextureProcess
