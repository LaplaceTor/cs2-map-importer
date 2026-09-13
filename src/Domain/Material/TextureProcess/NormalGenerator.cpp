#include "Domain/Material/TextureProcess/NormalGenerator.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Domain/Material/TextureProcess/TextureBlur.h"

namespace {

float wrappedHeightAt(const Domain::Material::TextureImage& height, int x, int y)
{
    const int wrappedX = (x % height.width() + height.width()) % height.width();
    const int wrappedY = (y % height.height() + height.height()) % height.height();
    return height.at(0, wrappedX, wrappedY);
}

float luminanceAt(const Domain::Material::TextureImage& color, int x, int y)
{
    if (color.channelCount() >= 3) {
        return std::clamp(
            color.at(0, x, y) * 0.3f + color.at(1, x, y) * 0.5f + color.at(2, x, y) * 0.2f, 0.0f,
            1.0f);
    }
    return color.at(0, x, y);
}

} // namespace

namespace Domain::Material::TextureProcess {

Core::Result<TextureImage> NormalGenerator::generateFromHeight(
    const TextureImage& linearHeight, const TextureImage* linearColorSource,
    const NormalParams& params, const Core::Async::CancellationToken& token,
    const std::function<void(float)>& progress)
{
    if (!linearHeight.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("NormalGenerator", "height texture is not valid"));
    }
    if (params.slopeBlurSamples <= 0) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("NormalGenerator", "slope blur samples must be positive"));
    }

    const int width = linearHeight.width();
    const int height = linearHeight.height();

    // Step 1 — blurred shape source for "Shape from Diffuse".
    const bool shapeEnabled = params.shapeRecognition > 0.0f;
    TextureImage shapeOriginal;
    TextureImage shapeBlur;
    if (shapeEnabled) {
        const bool useColor = params.shapeFromColorSource && linearColorSource != nullptr
            && linearColorSource->isValid() && linearColorSource->channelCount() >= 3;
        shapeOriginal = TextureImage(width, height, 1);
        if (!shapeOriginal.isValid()) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("NormalGenerator",
                    "failed to allocate working buffers"));
        }
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                shapeOriginal.at(0, x, y) = useColor ? luminanceAt(*linearColorSource, x, y)
                                                     : linearHeight.at(0, x, y);
            }
        }

        // Cascade of slope blurs: H/V at spread 1.0, then H/V at spread 3.0.
        auto blurPass = [&](const TextureImage& source,
                            float spread) -> Core::Result<TextureImage> {
            auto horizontal = TextureBlur::blurAxis(source, TextureBlur::Axis::Horizontal,
                params.slopeBlurSamples, spread, 1.0f, token, {});
            if (horizontal.isFailure()) {
                return horizontal;
            }
            return TextureBlur::blurAxis(horizontal.value(), TextureBlur::Axis::Vertical,
                params.slopeBlurSamples, spread, 1.0f, token, {});
        };
        auto firstPass = blurPass(shapeOriginal, 1.0f);
        if (firstPass.isFailure()) {
            return Core::Result<TextureImage>::failure(firstPass.error());
        }
        auto secondPass = blurPass(firstPass.value(), 3.0f);
        if (secondPass.isFailure()) {
            return Core::Result<TextureImage>::failure(secondPass.error());
        }
        shapeBlur = std::move(secondPass.value());
    }
    if (progress) {
        progress(shapeEnabled ? 0.15f : 0.05f);
    }

    // Step 2 — derivative normals (fragNormal), stored in [0, 1] encoding.
    TextureImage normal0(width, height, 3);
    if (!normal0.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("NormalGenerator", "failed to allocate working buffers"));
    }

    const float sinRotation = std::sin(params.lightRotation);
    const float cosRotation = std::cos(params.lightRotation);

    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (y & 0x3F) == 0) {
            progress((shapeEnabled ? 0.15f : 0.05f)
                + static_cast<float>(y) / static_cast<float>(height) * 0.2f);
        }
        for (int x = 0; x < width; ++x) {
            const float center = linearHeight.at(0, x, y);
            // The reference samples the next texel with repeat wrapping.
            const float ddx = wrappedHeightAt(linearHeight, x + 1, y) - center;
            const float ddy = wrappedHeightAt(linearHeight, x, y + 1) - center;

            const float slopeX = ddx * params.preContrast;
            const float slopeY = ddy * params.preContrast;
            const float length = std::sqrt(slopeX * slopeX + slopeY * slopeY + 1.0f);
            float nx = -slopeX / length;
            float ny = -slopeY / length;
            float nz = 1.0f / length;

            if (shapeEnabled) {
                float highPass = shapeOriginal.at(0, x, y) - shapeBlur.at(0, x, y);
                highPass = (highPass + params.shapeBias) * 2.0f - 1.0f;

                const float lightCrossX = cosRotation;
                const float lightCrossY = -sinRotation;
                const float crossDot = nx * lightCrossX + ny * lightCrossY;

                float shapeX = highPass * sinRotation + crossDot * lightCrossX;
                float shapeY = highPass * cosRotation + crossDot * lightCrossY;
                float shapeZ = std::sqrt(1.0f - std::clamp(shapeX * shapeX + shapeY * shapeY,
                                                 0.0f, 1.0f));
                const float shapeLength =
                    std::sqrt(shapeX * shapeX + shapeY * shapeY + shapeZ * shapeZ);
                shapeX /= shapeLength;
                shapeY /= shapeLength;
                shapeZ /= shapeLength;

                nx += (shapeX - nx) * params.shapeRecognition;
                ny += (shapeY - ny) * params.shapeRecognition;
                nz += (shapeZ - nz) * params.shapeRecognition;
                const float nLength = std::sqrt(nx * nx + ny * ny + nz * nz);
                nx /= nLength;
                ny /= nLength;
                nz /= nLength;
            }

            normal0.at(0, x, y) = nx * 0.5f + 0.5f;
            normal0.at(1, x, y) = ny * 0.5f + 0.5f;
            normal0.at(2, x, y) = nz * 0.5f + 0.5f;
        }
    }
    if (progress) {
        progress(0.35f);
    }

    // Step 3 — frequency bands over the derivative normals.
    auto bands = TextureBlur::buildFrequencyBands(normal0, token, [&](float fraction) {
        if (progress) {
            progress(0.35f + fraction * 0.45f);
        }
    });
    if (!bands.isSuccess()) {
        return Core::Result<TextureImage>::failure(bands.error());
    }
    if (progress) {
        progress(0.8f);
    }

    // Step 4 — frequency combine, angularity and final contrast
    // (fragCombineNormal).
    float weightSum = 0.0f;
    for (int band = 0; band < 7; ++band) {
        weightSum += std::max(params.bandWeights[static_cast<std::size_t>(band)], 0.0f);
    }
    if (weightSum <= 0.0f) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("NormalGenerator", "band weights must not all be zero"));
    }

    TextureImage combined(width, height, 3);
    if (!combined.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("NormalGenerator", "failed to allocate working buffers"));
    }

    for (int y = 0; y < height; ++y) {
        if (token.isCancelled()) {
            return Core::Result<TextureImage>::cancelled();
        }
        if (progress && (y & 0x3F) == 0) {
            progress(0.8f + static_cast<float>(y) / static_cast<float>(height) * 0.2f);
        }
        for (int x = 0; x < width; ++x) {
            float nx = 0.0f;
            float ny = 0.0f;
            float nz = 0.0f;
            for (int band = 0; band < 7; ++band) {
                const float weight = std::max(
                    params.bandWeights[static_cast<std::size_t>(band)], 0.0f);
                const auto& bandImage = bands.value()[static_cast<std::size_t>(band)];
                nx += bandImage.at(0, x, y) * weight;
                ny += bandImage.at(1, x, y) * weight;
                nz += bandImage.at(2, x, y) * weight;
            }
            nx = nx / weightSum * 2.0f - 1.0f;
            ny = ny / weightSum * 2.0f - 1.0f;
            nz = nz / weightSum * 2.0f - 1.0f;

            float length = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= length;
            ny /= length;
            nz /= length;

            if (params.angularity > 0.0f) {
                const float xyLength = std::sqrt(nx * nx + ny * ny + 0.001f * 0.001f);
                const float dirX = (nx / xyLength) * params.angularIntensity;
                const float dirY = (ny / xyLength) * params.angularIntensity;
                const float dirZ = std::max(1.0f - params.angularIntensity, 0.001f);
                const float dirLength = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
                nx += (dirX / dirLength - nx) * params.angularity;
                ny += (dirY / dirLength - ny) * params.angularity;
                nz += (dirZ / dirLength - nz) * params.angularity;
            }

            nx *= params.finalContrast;
            ny *= params.finalContrast;
            nz = std::pow(std::clamp(nz, 0.0f, 1.0f), params.finalContrast);

            length = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= length;
            ny /= length;
            nz /= length;

            float outX = nx * 0.5f + 0.5f;
            float outY = ny * 0.5f + 0.5f;
            if (params.flipGreenChannel) {
                outY = 1.0f - outY;
            }
            combined.at(0, x, y) = outX;
            combined.at(1, x, y) = outY;
            combined.at(2, x, y) = nz * 0.5f + 0.5f;
        }
    }
    if (progress) {
        progress(1.0f);
    }
    return Core::Result<TextureImage>::success(std::move(combined));
}

} // namespace Domain::Material::TextureProcess
