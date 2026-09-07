#include "Workflow/Common/BspEmbeddedExtractor.h"

#include <utility>

#include "Domain/Package/BspPackExtractor.h"

namespace Workflow::Common {

Core::Result<std::size_t> BspEmbeddedExtractor::extract(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& destDir,
    const CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (bspPath.isEmpty() || !bspPath.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("BSP file path is empty or invalid"));
    }
    if (!bspPath.exists()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("BSP file not found"),
            bspPath.toString());
    }
    if (destDir.isEmpty() || !destDir.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("destination directory is empty or invalid"));
    }

    if (token.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(
            QStringLiteral("BSP embedded file extraction cancelled"), 0);
    }

    Domain::Package::BspExtractOptions options;
    options.isCancelled = [&token]() {
        return token.isCancelled();
    };

    auto result = Domain::Package::BspPackExtractor::extractAll(bspPath, destDir, options);

    if (taskCtx) {
        if (result.isSuccess()) {
            taskCtx->info(QStringLiteral("Extracted %1 embedded file(s) from '%2' to '%3'")
                              .arg(result.value())
                              .arg(bspPath.toString(), destDir.toString()));
        } else if (result.isCancelled()) {
            taskCtx->warning(QStringLiteral("BSP embedded file extraction cancelled after %1 file(s)")
                                 .arg(result.valueOr(0)));
        } else if (result.isFailure()) {
            taskCtx->error(QStringLiteral("Failed to extract embedded files from '%1': %2")
                               .arg(bspPath.toString(), result.message()));
        }
    }

    return result;
}

} // namespace Workflow::Common
