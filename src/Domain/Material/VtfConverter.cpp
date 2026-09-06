#include "Domain/Material/VtfConverter.h"

#include <exception>
#include <filesystem>

#include <QByteArray>

#include <vtfpp/VTF.h>

#include "Core/Error/Exception.h"
#include "Core/FileSystem/FileSystem.h"

namespace {

/**
 * @brief Exception boundary: translates Core/third-party/std exceptions into
 *        structured Result failures instead of letting them escape Domain.
 */
template<typename Fn>
auto runGuarded(Fn&& fn) -> decltype(fn()) {
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

std::filesystem::path toNativePath(const Core::Path::FilesystemPath& path) {
    return std::filesystem::path{path.toString().toStdWString()};
}

Core::Result<std::vector<std::byte>> validateVtfPath(const Core::Path::FilesystemPath& vtfPath) {
    if (vtfPath.isEmpty() || !vtfPath.isValid()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("VTF file path is empty or invalid"));
    }
    if (!vtfPath.exists() || !vtfPath.isFile()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("VTF file not found"),
            vtfPath.toString());
    }
    return Core::Result<std::vector<std::byte>>::success({});
}

vtfpp::ImageConversion::FileFormat toVtfppFormat(Domain::Material::ImageFileFormat format) {
    switch (format) {
        case Domain::Material::ImageFileFormat::Png:
            return vtfpp::ImageConversion::FileFormat::PNG;
        case Domain::Material::ImageFileFormat::Tga:
            return vtfpp::ImageConversion::FileFormat::TGA;
        case Domain::Material::ImageFileFormat::Jpg:
            return vtfpp::ImageConversion::FileFormat::JPG;
        case Domain::Material::ImageFileFormat::Bmp:
            return vtfpp::ImageConversion::FileFormat::BMP;
        case Domain::Material::ImageFileFormat::Hdr:
            return vtfpp::ImageConversion::FileFormat::HDR;
    }
    return vtfpp::ImageConversion::FileFormat::PNG;
}

} // namespace

namespace Domain::Material {

QString VtfConverter::formatExtension(ImageFileFormat format) {
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
    ImageFileFormat format) {
    if (auto validated = validateVtfPath(vtfPath); validated.isFailure()) {
        return validated;
    }

    return runGuarded([&]() -> Core::Result<std::vector<std::byte>> {
        // May throw on malformed VTF data; guarded above.
        vtfpp::VTF vtf{toNativePath(vtfPath)};

        // Mip 0 is the highest resolution mip; first frame and first face.
        auto fileBytes = vtf.saveImageToFile(0, 0, 0, 0, toVtfppFormat(format));
        if (fileBytes.empty()) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("failed to decode VTF image or encode to target format"),
                vtfPath.toString());
        }
        return Core::Result<std::vector<std::byte>>::success(std::move(fileBytes));
    });
}

Core::Result<void> VtfConverter::convertToImageFile(
    const Core::Path::FilesystemPath& vtfPath,
    const Core::Path::FilesystemPath& destImagePath,
    ImageFileFormat format) {
    if (destImagePath.isEmpty() || !destImagePath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("destination image path is empty or invalid"));
    }

    auto imageBytes = convertToImageBuffer(vtfPath, format);
    if (imageBytes.isFailure()) {
        return Core::Result<void>::failure(imageBytes.error());
    }

    return runGuarded([&]() -> Core::Result<void> {
        // Core::FileSystem helpers throw Core::Error::Exception on failure; guarded above.
        Core::FileSystem::FileSystem::createDirectory(destImagePath.parentPath().toString());
        Core::FileSystem::FileSystem::writeAll(destImagePath.toString(), QByteArray(
            reinterpret_cast<const char*>(imageBytes.value().data()),
            static_cast<qsizetype>(imageBytes.value().size())));
        return Core::Result<void>::success();
    });
}

} // namespace Domain::Material
