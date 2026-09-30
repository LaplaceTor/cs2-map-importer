#include <QCoreApplication>
#include "Workflow/Common/VtfExtractor.h"

#include <QFileInfo>

#include "Core/Error/ExecutionGuard.h"
#include "Core/Temp/TempDirectory.h"
#include "Domain/Material/VtfConverter.h"

namespace Workflow::Common {

Core::Result<AssetExtraction> VtfExtractor::extract(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeVtfPath,
    const Core::Path::FilesystemPath& destImageDir,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<AssetExtraction>::cancelled(
            QCoreApplication::translate("VtfExtractor", "VTF extraction cancelled"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Extracting and converting VTF to image"),
        .resourcePath = relativeVtfPath,
        .targetPath = destImageDir.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<AssetExtraction> {
        if (relativeVtfPath.isEmpty()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("VtfExtractor", "relative VTF path is empty"));
        }
        if (destImageDir.isEmpty() || !destImageDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("VtfExtractor", "destination image directory is empty or invalid"));
        }

        // The intermediate VTF lands in a RAII temporary directory.
        Core::Temp::TempDirectory tempDir;
        auto extraction = AssetExtractor::extract(
            targets, relativeVtfPath, Core::Path::FilesystemPath(tempDir.path()), {}, token, taskCtx);
        if (!extraction.isSuccess()) {
            return extraction;
        }

        // Image export is fixed to PNG at this use-case level.
        const QString imageName = QFileInfo(relativeVtfPath).baseName() + QStringLiteral(".png");
        auto destImageFileOpt = destImageDir.resolveBelow(imageName);
        if (!destImageFileOpt.has_value()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("VtfExtractor", "destination image path traverses outside destination directory"),
                imageName);
        }
        const Core::Path::FilesystemPath destImageFile = *destImageFileOpt;

        auto converted = Domain::Material::VtfConverter::convertToImageFile(
            extraction.value().extractedFilePath, destImageFile,
            Domain::Material::ImageFileFormat::Png,
            destImageDir);
        if (converted.isFailure()) {
            return Core::Result<AssetExtraction>::failure(converted.error());
        }

        extraction.value().extractedFilePath = destImageFile;
        return extraction;
    }, ctx);
}

} // namespace Workflow::Common
