#include "Workflow/Common/VtfExtractor.h"

#include <exception>

#include <QFileInfo>

#include "Core/Error/Exception.h"
#include "Core/Temp/TempDirectory.h"
#include "Domain/Material/VtfConverter.h"

namespace {

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

} // namespace

namespace Workflow::Common {

Core::Result<AssetExtraction> VtfExtractor::extract(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeVtfPath,
    const Core::Path::FilesystemPath& destImageDir,
    Domain::Material::ImageFileFormat targetFormat,
    const CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    return runGuarded([&]() -> Core::Result<AssetExtraction> {
        if (relativeVtfPath.isEmpty()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("relative VTF path is empty"));
        }
        if (destImageDir.isEmpty() || !destImageDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QStringLiteral("destination image directory is empty or invalid"));
        }

        // The intermediate VTF lands in a RAII temporary directory.
        Core::Temp::TempDirectory tempDir;
        auto extraction = AssetExtractor::extract(
            targets, relativeVtfPath, Core::Path::FilesystemPath(tempDir.path()), {}, token, taskCtx);
        if (!extraction.isSuccess()) {
            return extraction;
        }

        const QString ext = Domain::Material::VtfConverter::formatExtension(targetFormat);
        const QString imageName = QFileInfo(relativeVtfPath).baseName() + u'.' + ext;
        const Core::Path::FilesystemPath destImageFile = destImageDir / imageName;

        auto converted = Domain::Material::VtfConverter::convertToImageFile(
            extraction.value().extractedFilePath, destImageFile, targetFormat);
        if (converted.isFailure()) {
            return Core::Result<AssetExtraction>::failure(converted.error());
        }

        extraction.value().extractedFilePath = destImageFile;
        return extraction;
    });
}

} // namespace Workflow::Common
