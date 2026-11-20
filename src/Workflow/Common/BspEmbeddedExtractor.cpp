#include <QCoreApplication>
#include "Workflow/Common/BspEmbeddedExtractor.h"

#include "Domain/Package/BspPackExtractor.h"

namespace Workflow::Common {

Core::Result<std::size_t> BspEmbeddedExtractor::extract(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& destDir,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    Domain::Package::BspExtractOptions options;
    options.token = token;

    auto result = Domain::Package::BspPackExtractor::extractAll(bspPath, destDir, options);

    if (taskCtx) {
        if (result.isSuccess()) {
            taskCtx->info(QCoreApplication::translate("BspEmbeddedExtractor", "Extracted %1 embedded file(s) from '%2' to '%3'")
                              .arg(result.value())
                              .arg(bspPath.toString(), destDir.toString()));
        } else if (result.isCancelled()) {
            taskCtx->warning(QCoreApplication::translate("BspEmbeddedExtractor", "BSP embedded file extraction cancelled after %1 file(s)")
                                 .arg(result.valueOr(0)));
        } else if (result.isFailure()) {
            taskCtx->error(QCoreApplication::translate("BspEmbeddedExtractor", "Failed to extract embedded files from '%1': %2")
                               .arg(bspPath.toString(), result.message()));
        }
    }

    return result;
}

} // namespace Workflow::Common
