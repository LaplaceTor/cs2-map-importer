#include <QCoreApplication>
#include "Domain/Material/VtfConverter.h"

#include <filesystem>

#include <QByteArray>

#include <vtfpp/VTF.h>

#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/FileSystem.h"

namespace {

std::filesystem::path toNativePath(const Core::Path::FilesystemPath& path) {
    return std::filesystem::path{path.toString().toStdWString()};
}

Core::Result<std::vector<std::byte>> validateVtfPath(const Core::Path::FilesystemPath& vtfPath) {
    if (vtfPath.isEmpty() || !vtfPath.isValid()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfConverter", "VTF file path is empty or invalid"));
    }
    if (!vtfPath.exists() || !vtfPath.isFile()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VtfConverter", "VTF file not found"),
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

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Converting VTF to image buffer"),
        .targetPath = vtfPath.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<std::vector<std::byte>> {
        // May throw on malformed VTF data; guarded above.
        vtfpp::VTF vtf{toNativePath(vtfPath)};

        // Mip 0 is the highest resolution mip; first frame and first face.
        auto fileBytes = vtf.saveImageToFile(0, 0, 0, 0, toVtfppFormat(format));
        if (fileBytes.empty()) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("VtfConverter", "failed to decode VTF image or encode to target format"),
                vtfPath.toString());
        }
        return Core::Result<std::vector<std::byte>>::success(std::move(fileBytes));
    }, ctx);
}

Core::Result<void> VtfConverter::convertToImageFile(
    const Core::Path::FilesystemPath& vtfPath,
    const Core::Path::FilesystemPath& destImagePath,
    ImageFileFormat format,
    const Core::Path::FilesystemPath& expectedBaseDir) {
    if (destImagePath.isEmpty() || !destImagePath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtfConverter", "destination image path is empty or invalid"));
    }

    auto imageBytes = convertToImageBuffer(vtfPath, format);
    if (imageBytes.isFailure()) {
        return Core::Result<void>::failure(imageBytes.error());
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Writing converted VTF image"),
        .resourcePath = vtfPath.toString(),
        .targetPath = destImagePath.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
        // Core::FileSystem helpers throw Core::Error::Exception on failure; guarded above.
        // Tier 1 + Tier 2 base directory boundary is enforced by FileSystem::writeAll -> AtomicFile.
        Core::FileSystem::FileSystem::createDirectory(destImagePath.parentPath().toString());
        Core::FileSystem::FileSystem::writeAll(destImagePath.toString(), QByteArray(
            reinterpret_cast<const char*>(imageBytes.value().data()),
            static_cast<qsizetype>(imageBytes.value().size())),
            expectedBaseDir);
        return Core::Result<void>::success();
    }, ctx);
}

} // namespace Domain::Material
