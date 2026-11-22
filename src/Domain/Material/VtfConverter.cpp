#include "Domain/Material/VtfConverter.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QIODevice>
#include <QImage>
#include <QImageWriter>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Core/FileSystem/FileSystem.h"
#include "Domain/Material/VtfCodec.h"

namespace Domain::Material {

namespace Detail {
Core::Result<std::vector<float>> readVtfMip0RgbaFloat(
    const Core::Path::FilesystemPath& path,
    int& outWidth,
    int& outHeight);
} // namespace Detail

namespace {

struct QtImageEncodeSpec {
    const char* format = nullptr;
    int quality = -1;
};

QtImageEncodeSpec qtEncodeSpecFor(ImageFileFormat format)
{
    switch (format) {
        case ImageFileFormat::Png:
            return {"png", -1};
        case ImageFileFormat::Jpg:
            return {"jpg", 100};
        case ImageFileFormat::Bmp:
            return {"bmp", -1};
        case ImageFileFormat::Tga:
        case ImageFileFormat::Hdr:
            break;
    }
    return {};
}

QImage prepareImageForQtFormat(const QImage& image, ImageFileFormat format)
{
    switch (format) {
        case ImageFileFormat::Jpg:
            return image.convertToFormat(QImage::Format_RGB888);
        case ImageFileFormat::Bmp:
            return image.convertToFormat(QImage::Format_ARGB32);
        case ImageFileFormat::Png:
        case ImageFileFormat::Tga:
        case ImageFileFormat::Hdr:
            break;
    }
    return image;
}

Core::Result<void> writeWithQImageWriter(
    const QImage& image,
    const char* qtFormat,
    int quality,
    QIODevice* device,
    const Core::Path::FilesystemPath& vtfPath)
{
    QImageWriter writer(device, qtFormat);
    if (quality >= 0) {
        writer.setQuality(quality);
    }
    if (!writer.write(image)) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("VtfConverter", "Failed to encode VTF image to target format"),
            vtfPath.toString() + QStringLiteral(" | ") + writer.errorString());
    }
    return Core::Result<void>::success();
}

Core::Result<std::vector<std::byte>> encodeWithQImageWriter(
    const QImage& image,
    const char* qtFormat,
    int quality,
    const Core::Path::FilesystemPath& vtfPath)
{
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("VtfConverter", "Failed to open memory buffer for image encoding"),
            vtfPath.toString());
    }

    auto writeRes = writeWithQImageWriter(image, qtFormat, quality, &buffer, vtfPath);
    if (writeRes.isFailure()) {
        return Core::Result<std::vector<std::byte>>::failure(writeRes.error());
    }

    std::vector<std::byte> out(static_cast<std::size_t>(encoded.size()));
    if (!out.empty()) {
        std::memcpy(out.data(), encoded.constData(), out.size());
    }
    return Core::Result<std::vector<std::byte>>::success(std::move(out));
}

std::vector<std::byte> encodeTga32(const QImage& rgbaImage)
{
    const int width = rgbaImage.width();
    const int height = rgbaImage.height();
    constexpr std::size_t kTgaHeaderSize = 18;
    std::vector<std::byte> out(
        kTgaHeaderSize + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u,
        std::byte{0});

    out[2] = std::byte{2}; // Uncompressed true-color
    out[12] = static_cast<std::byte>(width & 0xFF);
    out[13] = static_cast<std::byte>((width >> 8) & 0xFF);
    out[14] = static_cast<std::byte>(height & 0xFF);
    out[15] = static_cast<std::byte>((height >> 8) & 0xFF);
    out[16] = std::byte{32};   // 32 bits per pixel (BGRA)
    out[17] = std::byte{0x28}; // Top-down origin (0x20) + 8-bit alpha (0x08)

    std::byte* dst = out.data() + kTgaHeaderSize;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row = rgbaImage.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* px = row + x * 4;
            dst[0] = static_cast<std::byte>(px[2]); // B
            dst[1] = static_cast<std::byte>(px[1]); // G
            dst[2] = static_cast<std::byte>(px[0]); // R
            dst[3] = static_cast<std::byte>(px[3]); // A
            dst += 4;
        }
    }
    return out;
}

std::vector<std::byte> encodeRadianceHdr(const std::vector<float>& rgbaFloat, int width, int height)
{
    const QByteArray header = QByteArrayLiteral("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y ")
        + QByteArray::number(height)
        + QByteArrayLiteral(" +X ")
        + QByteArray::number(width)
        + QByteArrayLiteral("\n");

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::byte> out(static_cast<std::size_t>(header.size()) + pixelCount * 4u);
    std::memcpy(out.data(), header.constData(), static_cast<std::size_t>(header.size()));

    std::byte* dst = out.data() + header.size();
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const float r = std::max(0.0f, rgbaFloat[i * 4u + 0]);
        const float g = std::max(0.0f, rgbaFloat[i * 4u + 1]);
        const float b = std::max(0.0f, rgbaFloat[i * 4u + 2]);
        const float maxComp = std::max({r, g, b});

        if (!std::isfinite(maxComp) || maxComp < 1e-32f) {
            dst[0] = std::byte{0};
            dst[1] = std::byte{0};
            dst[2] = std::byte{0};
            dst[3] = std::byte{0};
        } else {
            int exponent = 0;
            const float normalize = std::frexp(maxComp, &exponent) * 256.0f / maxComp;
            dst[0] = static_cast<std::byte>(std::clamp(r * normalize, 0.0f, 255.0f));
            dst[1] = static_cast<std::byte>(std::clamp(g * normalize, 0.0f, 255.0f));
            dst[2] = static_cast<std::byte>(std::clamp(b * normalize, 0.0f, 255.0f));
            dst[3] = static_cast<std::byte>(std::clamp(exponent + 128, 0, 255));
        }
        dst += 4;
    }
    return out;
}

} // namespace

QString VtfConverter::formatExtension(ImageFileFormat format)
{
    switch (format) {
        case ImageFileFormat::Png:
            return QStringLiteral("png");
        case ImageFileFormat::Tga:
            return QStringLiteral("tga");
        case ImageFileFormat::Jpg:
            return QStringLiteral("jpg");
        case ImageFileFormat::Bmp:
            return QStringLiteral("bmp");
        case ImageFileFormat::Hdr:
            return QStringLiteral("hdr");
    }
    return QStringLiteral("png");
}

Core::Result<std::vector<std::byte>> VtfConverter::convertToImageBuffer(
    const Core::Path::FilesystemPath& vtfPath,
    ImageFileFormat format)
{
    if (format == ImageFileFormat::Hdr) {
        int width = 0;
        int height = 0;
        auto floatRes = Detail::readVtfMip0RgbaFloat(vtfPath, width, height);
        if (floatRes.isFailure()) {
            return Core::Result<std::vector<std::byte>>::failure(floatRes.error());
        }
        return Core::Result<std::vector<std::byte>>::success(
            encodeRadianceHdr(floatRes.value(), width, height));
    }

    auto readRes = VtfCodec::read(vtfPath);
    if (readRes.isFailure()) {
        return Core::Result<std::vector<std::byte>>::failure(readRes.error());
    }

    const QImage& image = readRes.value();
    const QtImageEncodeSpec spec = qtEncodeSpecFor(format);
    if (spec.format != nullptr) {
        return encodeWithQImageWriter(
            prepareImageForQtFormat(image, format), spec.format, spec.quality, vtfPath);
    }
    if (format == ImageFileFormat::Tga) {
        return Core::Result<std::vector<std::byte>>::success(encodeTga32(image));
    }

    return Core::Result<std::vector<std::byte>>::failure(
        Core::Error::ErrorCode::NotSupported,
        QCoreApplication::translate("VtfConverter", "Unsupported target image format"),
        vtfPath.toString());
}

Core::Result<void> VtfConverter::convertToImageFile(
    const Core::Path::FilesystemPath& vtfPath,
    const Core::Path::FilesystemPath& destImagePath,
    ImageFileFormat format,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    if (destImagePath.isEmpty() || !destImagePath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfConverter", "destination image path is empty or invalid"));
    }

    const QtImageEncodeSpec qtSpec = qtEncodeSpecFor(format);
    if (qtSpec.format == nullptr && format != ImageFileFormat::Tga && format != ImageFileFormat::Hdr) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("VtfConverter", "Unsupported target image format"),
            vtfPath.toString());
    }

    // Decode and prepare image/buffer BEFORE opening AtomicFile so failed decodes never touch disk.
    QImage preparedImage;
    std::vector<std::byte> encodedBytes;
    if (format == ImageFileFormat::Hdr) {
        int width = 0;
        int height = 0;
        auto floatRes = Detail::readVtfMip0RgbaFloat(vtfPath, width, height);
        if (floatRes.isFailure()) {
            return Core::Result<void>::failure(floatRes.error());
        }
        encodedBytes = encodeRadianceHdr(floatRes.value(), width, height);
    } else {
        auto readRes = VtfCodec::read(vtfPath);
        if (readRes.isFailure()) {
            return Core::Result<void>::failure(readRes.error());
        }
        if (format == ImageFileFormat::Tga) {
            encodedBytes = encodeTga32(readRes.value());
        } else {
            preparedImage = prepareImageForQtFormat(readRes.value(), format);
        }
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Writing converted VTF image"),
        .resourcePath = vtfPath.toString(),
        .targetPath = destImagePath.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
        // Core::FileSystem helpers throw Core::Error::Exception on failure; guarded above.
        // Tier 1 + Tier 2 base directory boundary is enforced by AtomicFile::open() / commit().
        Core::FileSystem::FileSystem::createDirectory(destImagePath.parentPath().toString());
        Core::FileSystem::AtomicFile atomic(destImagePath.toString(), expectedBaseDir);
        atomic.open();

        if (qtSpec.format != nullptr) {
            auto writeRes = writeWithQImageWriter(
                preparedImage, qtSpec.format, qtSpec.quality, atomic.device(), vtfPath);
            if (writeRes.isFailure()) {
                return writeRes;
            }
        } else {
            QIODevice* device = atomic.device();
            const qint64 expectedSize = static_cast<qint64>(encodedBytes.size());
            const qint64 written = device->write(
                reinterpret_cast<const char*>(encodedBytes.data()), expectedSize);
            if (written != expectedSize) {
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("VtfConverter", "Failed to write converted VTF image to disk"),
                    destImagePath.toString() + QStringLiteral(" | ") + device->errorString());
            }
        }

        atomic.commit();
        return Core::Result<void>::success();
    }, ctx);
}

} // namespace Domain::Material
