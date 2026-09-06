#include "Workflow/Common/BspEmbeddedExtractor.h"

#include <utility>

#include "Domain/Package/PackArchive.h"

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
    if (destDir.isEmpty() || !destDir.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("destination directory is empty or invalid"));
    }

    auto archive = Domain::Package::PackArchive::open(bspPath);
    if (archive.isFailure()) {
        return Core::Result<std::size_t>::failure(archive.error());
    }

    auto entries = archive.value().listEntries();
    if (entries.isFailure()) {
        return Core::Result<std::size_t>::failure(entries.error());
    }
    if (entries.value().empty()) {
        return Core::Result<std::size_t>::skipped(
            QStringLiteral("BSP contains no embedded files"));
    }

    std::size_t extractedCount = 0;
    for (const QString& entryPath : entries.value()) {
        if (token.isCancelled()) {
            return Core::Result<std::size_t>::cancelled(
                QStringLiteral("BSP embedded file extraction cancelled"), extractedCount);
        }

        auto extracted = archive.value().extractEntryToFile(entryPath, destDir / entryPath);
        if (extracted.isFailure()) {
            if (taskCtx) {
                taskCtx->warning(QStringLiteral("Failed to extract embedded file '%1': %2")
                                     .arg(entryPath, extracted.message()));
            }
            continue;
        }
        extractedCount++;
    }

    if (taskCtx) {
        taskCtx->info(QStringLiteral("Extracted %1 embedded file(s) from '%2' to '%3'")
                          .arg(extractedCount)
                          .arg(bspPath.toString(), destDir.toString()));
    }
    return Core::Result<std::size_t>::success(extractedCount);
}

} // namespace Workflow::Common
