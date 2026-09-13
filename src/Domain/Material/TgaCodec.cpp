#include "Domain/Material/TgaCodec.h"

#include <QCoreApplication>
#include <QImage>

#include <cstring>
#include <exception>
#include <vector>

#include "Core/Error/Exception.h"
#include "Core/FileSystem/FileSystem.h"

namespace {

constexpr int kTgaHeaderSize = 18;

constexpr std::uint8_t kImageTypeUncompressedTrueColor = 2;
constexpr std::uint8_t kImageTypeUncompressedGrayscale = 3;
constexpr std::uint8_t kImageTypeRleTrueColor = 10;
constexpr std::uint8_t kImageTypeRleGrayscale = 11;

constexpr std::uint8_t kDescriptorTopDown = 0x20;

std::uint16_t readLittleEndianU16(const unsigned char* bytes)
{
    return static_cast<std::uint16_t>(bytes[0]) | (static_cast<std::uint16_t>(bytes[1]) << 8);
}

/**
 * @brief Exception boundary for filesystem access inside the codec.
 */
template<typename Fn>
auto runGuarded(Fn&& fn) -> decltype(fn())
{
    try {
        return fn();
    } catch (const Core::Error::Exception& ex) {
        return decltype(fn())::failure(ex.error());
    } catch (const std::exception& ex) {
        return decltype(fn())::failure(
            Core::Error::ErrorCode::OperationFailed,
            QString::fromUtf8(ex.what()));
    }
}

struct TgaHeader {
    std::uint8_t idLength = 0;
    std::uint8_t colorMapType = 0;
    std::uint8_t imageType = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t bitsPerPixel = 0;
    std::uint8_t descriptor = 0;
};

bool parseHeader(const QByteArray& data, TgaHeader& header)
{
    if (data.size() < kTgaHeaderSize) {
        return false;
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    header.idLength = bytes[0];
    header.colorMapType = bytes[1];
    header.imageType = bytes[2];
    header.width = readLittleEndianU16(bytes + 12);
    header.height = readLittleEndianU16(bytes + 14);
    header.bitsPerPixel = bytes[16];
    header.descriptor = bytes[17];
    return true;
}

bool isSupportedImageType(std::uint8_t imageType)
{
    switch (imageType) {
        case kImageTypeUncompressedTrueColor:
        case kImageTypeUncompressedGrayscale:
        case kImageTypeRleTrueColor:
        case kImageTypeRleGrayscale:
            return true;
        default:
            return false;
    }
}

bool isTrueColorType(std::uint8_t imageType)
{
    return imageType == kImageTypeUncompressedTrueColor || imageType == kImageTypeRleTrueColor;
}

/**
 * @brief Expands the pixel block (uncompressed or RLE) into a tightly packed
 *        row-major buffer of `bytesPerPixel` samples, top-down order.
 */
bool decodePixelBlock(const QByteArray& data, const TgaHeader& header, int bytesPerPixel,
                      std::vector<unsigned char>& outPixels)
{
    const std::size_t pixelCount =
        static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height);
    const bool rle = header.imageType == kImageTypeRleTrueColor
        || header.imageType == kImageTypeRleGrayscale;

    const auto* cursor = reinterpret_cast<const unsigned char*>(data.constData()) + kTgaHeaderSize
        + header.idLength;
    const auto* end = reinterpret_cast<const unsigned char*>(data.constData()) + data.size();

    outPixels.assign(pixelCount * bytesPerPixel, 0);
    std::size_t written = 0;

    if (!rle) {
        if (static_cast<std::size_t>(end - cursor) < pixelCount * bytesPerPixel) {
            return false;
        }
        std::memcpy(outPixels.data(), cursor, pixelCount * bytesPerPixel);
        return true;
    }

    std::vector<unsigned char> rlePixel(bytesPerPixel);
    while (written < pixelCount * bytesPerPixel) {
        if (cursor >= end) {
            return false;
        }
        const std::uint8_t packetHeader = *cursor++;
        const int runLength = static_cast<int>(packetHeader & 0x7F) + 1;
        const std::size_t runBytes = static_cast<std::size_t>(runLength) * bytesPerPixel;
        if (packetHeader & 0x80) {
            // RLE packet: one pixel repeated runLength times.
            if (static_cast<std::size_t>(end - cursor) < bytesPerPixel) {
                return false;
            }
            std::memcpy(rlePixel.data(), cursor, bytesPerPixel);
            cursor += bytesPerPixel;
            if (written + runBytes > outPixels.size()) {
                return false;
            }
            for (int i = 0; i < runLength; ++i) {
                std::memcpy(outPixels.data() + written, rlePixel.data(), bytesPerPixel);
                written += bytesPerPixel;
            }
        } else {
            // Raw packet: runLength literal pixels.
            if (static_cast<std::size_t>(end - cursor) < runBytes) {
                return false;
            }
            if (written + runBytes > outPixels.size()) {
                return false;
            }
            std::memcpy(outPixels.data() + written, cursor, runBytes);
            cursor += runBytes;
            written += runBytes;
        }
    }
    return true;
}

} // namespace

namespace Domain::Material {

bool TgaCodec::isTgaExtension(const QString& lowerCaseExtension)
{
    return lowerCaseExtension == QLatin1String("tga");
}

Core::Result<QImage> TgaCodec::read(const Core::Path::FilesystemPath& path)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("TgaCodec", "TGA file path is empty or invalid"));
    }
    if (!path.exists() || !path.isFile()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("TgaCodec", "TGA file not found"),
            path.toString());
    }

    return runGuarded([&]() -> Core::Result<QImage> {
        // Throws Core::Error::Exception on IO failure; guarded above.
        const QByteArray data = Core::FileSystem::FileSystem::readAll(path.toString());

        TgaHeader header;
        if (!parseHeader(data, header) || !isSupportedImageType(header.imageType)
            || header.colorMapType != 0) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("TgaCodec", "unsupported or malformed TGA header"),
                path.toString());
        }
        if (header.width == 0 || header.height == 0) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("TgaCodec", "TGA image has zero dimensions"),
                path.toString());
        }

        const bool trueColor = isTrueColorType(header.imageType);
        const int bytesPerPixel = header.bitsPerPixel / 8;
        const bool validBpp = trueColor
            ? (header.bitsPerPixel == 24 || header.bitsPerPixel == 32)
            : header.bitsPerPixel == 8;
        if (!validBpp || bytesPerPixel <= 0) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::NotSupported,
                QCoreApplication::translate("TgaCodec", "unsupported TGA bit depth"),
                path.toString() + QStringLiteral(" (bpp=%1)").arg(header.bitsPerPixel));
        }

        std::vector<unsigned char> pixels;
        if (!decodePixelBlock(data, header, bytesPerPixel, pixels)) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("TgaCodec", "TGA pixel data is truncated or corrupted"),
                path.toString());
        }

        const bool bottomUp = (header.descriptor & kDescriptorTopDown) == 0;
        const int width = header.width;
        const int height = header.height;

        QImage image;
        if (trueColor) {
            image = QImage(width, height, QImage::Format_RGBA8888);
            const bool hasAlpha = header.bitsPerPixel == 32;
            for (int y = 0; y < height; ++y) {
                const int sourceRow = bottomUp ? (height - 1 - y) : y;
                auto* destRow = image.scanLine(y);
                const auto* sourcePixels = pixels.data()
                    + static_cast<std::size_t>(sourceRow) * width * bytesPerPixel;
                for (int x = 0; x < width; ++x) {
                    destRow[x * 4 + 0] = sourcePixels[x * bytesPerPixel + 2]; // R from BGR(A)
                    destRow[x * 4 + 1] = sourcePixels[x * bytesPerPixel + 1]; // G
                    destRow[x * 4 + 2] = sourcePixels[x * bytesPerPixel + 0]; // B
                    destRow[x * 4 + 3] = hasAlpha ? sourcePixels[x * bytesPerPixel + 3] : 0xFF;
                }
            }
        } else {
            image = QImage(width, height, QImage::Format_Grayscale8);
            for (int y = 0; y < height; ++y) {
                const int sourceRow = bottomUp ? (height - 1 - y) : y;
                std::memcpy(image.scanLine(y),
                            pixels.data() + static_cast<std::size_t>(sourceRow) * width,
                            static_cast<std::size_t>(width));
            }
        }
        return Core::Result<QImage>::success(std::move(image));
    });
}

} // namespace Domain::Material
