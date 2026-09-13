#include "Domain/Material/VtfCodec.h"

#include <QCoreApplication>
#include <QImage>

#include <cstring>
#include <exception>
#include <filesystem>
#include <vector>

#include <vtfpp/VTF.h>

#include "Core/Error/Exception.h"

namespace {

/**
 * @brief Exception boundary: translates Core/third-party/std exceptions into
 *        structured Result failures instead of letting them escape Domain.
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

std::filesystem::path toNativePath(const Core::Path::FilesystemPath& path)
{
    return std::filesystem::path{path.toString().toStdWString()};
}

} // namespace

namespace Domain::Material {

bool VtfCodec::isVtfExtension(const QString& lowerCaseExtension)
{
    return lowerCaseExtension == QLatin1String("vtf");
}

Core::Result<QImage> VtfCodec::read(const Core::Path::FilesystemPath& path)
{
    if (path.isEmpty() || !path.isValid()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfCodec", "VTF file path is empty or invalid"));
    }
    if (!path.exists() || !path.isFile()) {
        return Core::Result<QImage>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VtfCodec", "VTF file not found"),
            path.toString());
    }

    return runGuarded([&]() -> Core::Result<QImage> {
        // May throw on malformed VTF data; guarded above.
        vtfpp::VTF vtf{toNativePath(path)};
        if (!vtf) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "failed to parse VTF file"),
                path.toString());
        }

        // Mip 0 is the highest resolution mip; first frame and first face
        // (same convention as VtfConverter).
        const std::vector<std::byte> pixels = vtf.getImageDataAsRGBA8888(0, 0, 0, 0);
        const int width = vtf.getWidth(0);
        const int height = vtf.getHeight(0);
        const qsizetype expectedSize = static_cast<qsizetype>(width) * height * 4;
        if (width <= 0 || height <= 0
            || static_cast<qsizetype>(pixels.size()) != expectedSize) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("VtfCodec", "VTF image data is missing or has unexpected size"),
                path.toString());
        }

        QImage image(width, height, QImage::Format_RGBA8888);
        const auto* source = reinterpret_cast<const unsigned char*>(pixels.data());
        for (int y = 0; y < height; ++y) {
            // RGBA8888 rows are tightly packed in both buffers.
            std::memcpy(image.scanLine(y), source + static_cast<qsizetype>(y) * width * 4,
                static_cast<std::size_t>(width) * 4);
        }
        return Core::Result<QImage>::success(std::move(image));
    });
}

} // namespace Domain::Material
