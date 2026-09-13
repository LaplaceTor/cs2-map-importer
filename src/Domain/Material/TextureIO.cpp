#include "Domain/Material/TextureIO.h"

#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QString>

#include <cmath>

#include "Core/FileSystem/FileSystem.h"
#include "Domain/Material/TgaCodec.h"
#include "Domain/Material/VtfCodec.h"

namespace {

/**
 * @brief sRGB EOTF (decode): 8-bit container values -> linear light floats.
 *        Matches the piecewise transfer the GPU applies when sampling an
 *        sRGB texture.
 */
float srgbDecodeChannel(unsigned char encoded)
{
    const float c = static_cast<float>(encoded) / 255.0f;
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/**
 * @brief sRGB OETF (encode): linear light float -> container value.
 */
unsigned char srgbEncodeChannel(float linear)
{
    const float c = std::clamp(linear, 0.0f, 1.0f);
    const float encoded = c <= 0.0031308f
        ? c * 12.92f
        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    return static_cast<unsigned char>(std::lround(encoded * 255.0f));
}

unsigned char quantizeLinear(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<unsigned char>(std::lround(clamped * 255.0f));
}

QString lowerCaseExtensionOf(const Core::Path::FilesystemPath& path)
{
    return path.extension().toLower();
}

} // namespace

namespace Domain::Material {

bool TextureIO::isSupportedLoadExtension(const QString& lowerCaseExtension)
{
    return lowerCaseExtension == QLatin1String("png")
        || lowerCaseExtension == QLatin1String("jpg")
        || lowerCaseExtension == QLatin1String("jpeg")
        || lowerCaseExtension == QLatin1String("bmp")
        || TgaCodec::isTgaExtension(lowerCaseExtension)
        || VtfCodec::isVtfExtension(lowerCaseExtension);
}

bool TextureIO::isSupportedWriteExtension(const QString& lowerCaseExtension)
{
    // Export is intentionally narrow: PNG only.
    return lowerCaseExtension == QLatin1String("png");
}

Core::Result<TextureImage> TextureIO::loadTexture(const Core::Path::FilesystemPath& path,
                                                  bool srgbDecode)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("TextureIO", "texture path is empty or invalid"));
    }
    if (!path.exists() || !path.isFile()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("TextureIO", "texture file not found"),
            path.toString());
    }

    const QString extension = lowerCaseExtensionOf(path);
    if (!isSupportedLoadExtension(extension)) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("TextureIO", "unsupported texture file extension"),
            path.toString());
    }

    QImage source;
    if (VtfCodec::isVtfExtension(extension)) {
        auto vtfResult = VtfCodec::read(path);
        if (vtfResult.isFailure()) {
            return Core::Result<TextureImage>::failure(vtfResult.error());
        }
        source = std::move(vtfResult.value());
    } else if (TgaCodec::isTgaExtension(extension)) {
        auto tgaResult = TgaCodec::read(path);
        if (tgaResult.isFailure()) {
            return Core::Result<TextureImage>::failure(tgaResult.error());
        }
        source = std::move(tgaResult.value());
    } else {
        QImageReader reader(path.toString());
        reader.setAutoTransform(true);
        QImage read = reader.read();
        if (read.isNull()) {
            return Core::Result<TextureImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("TextureIO", "failed to decode image file"),
                path.toString() + QStringLiteral(" | ") + reader.errorString());
        }
        source = std::move(read);
    }

    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    TextureImage image(rgba.width(), rgba.height(), 4);
    if (!image.isValid()) {
        return Core::Result<TextureImage>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("TextureIO", "image has invalid dimensions"),
            path.toString());
    }

    float* planes[4] = {
        image.planeData(0), image.planeData(1), image.planeData(2), image.planeData(3)
    };
    const int width = image.width();

    for (int y = 0; y < image.height(); ++y) {
        const auto* row = rgba.constScanLine(y);
        const qsizetype rowStart = static_cast<qsizetype>(y) * width;
        for (int x = 0; x < width; ++x) {
            const qsizetype index = rowStart + x;
            const auto* pixel = row + x * 4;
            if (srgbDecode) {
                planes[0][index] = srgbDecodeChannel(pixel[0]);
                planes[1][index] = srgbDecodeChannel(pixel[1]);
                planes[2][index] = srgbDecodeChannel(pixel[2]);
            } else {
                planes[0][index] = static_cast<float>(pixel[0]) / 255.0f;
                planes[1][index] = static_cast<float>(pixel[1]) / 255.0f;
                planes[2][index] = static_cast<float>(pixel[2]) / 255.0f;
            }
            planes[3][index] = static_cast<float>(pixel[3]) / 255.0f;
        }
    }
    return Core::Result<TextureImage>::success(std::move(image));
}

Core::Result<void> TextureIO::writeTexture(const Core::Path::FilesystemPath& path,
                                           const TextureImage& image,
                                           bool srgbEncode)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("TextureIO", "destination path is empty or invalid"));
    }
    if (!image.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("TextureIO", "texture image is not valid"));
    }

    const QString extension = lowerCaseExtensionOf(path);
    if (!isSupportedWriteExtension(extension)) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("TextureIO", "unsupported texture write extension"),
            path.toString());
    }

    const int width = image.width();
    const int height = image.height();
    const int channels = image.channelCount();

    QImage target;
    if (channels == 1) {
        target = QImage(width, height, QImage::Format_Grayscale8);
    } else if (channels == 3) {
        target = QImage(width, height, QImage::Format_RGB888);
    } else {
        target = QImage(width, height, QImage::Format_RGBA8888);
    }

    for (int y = 0; y < height; ++y) {
        auto* row = target.scanLine(y);
        for (int x = 0; x < width; ++x) {
            if (channels == 1) {
                const float value = image.at(0, x, y);
                row[x] = srgbEncode ? srgbEncodeChannel(value) : quantizeLinear(value);
            } else {
                auto* pixel = row + x * (channels == 3 ? 3 : 4);
                for (int c = 0; c < channels; ++c) {
                    const float value = image.at(c, x, y);
                    pixel[c] = srgbEncode ? srgbEncodeChannel(value) : quantizeLinear(value);
                }
                if (channels == 4) {
                    // Alpha travels raw; it is not color-managed.
                    pixel[3] = quantizeLinear(image.at(3, x, y));
                }
            }
        }
    }

    QImageWriter writer(path.toString(), "png");
    if (!writer.write(target)) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::WriteFailed,
            QCoreApplication::translate("TextureIO", "failed to write PNG file"),
            path.toString() + QStringLiteral(" | ") + writer.errorString());
    }
    return Core::Result<void>::success();
}

} // namespace Domain::Material
