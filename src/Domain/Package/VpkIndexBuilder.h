#pragma once

#include <vector>
#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Package/VpkIndex.h"

namespace Domain::Package {

/**
 * @brief Builder for scanning VPK archives using sourcepp and producing a VpkIndex.
 */
class VpkIndexBuilder {
public:
    /**
     * @brief Builds a VpkIndex from a prioritized list of VPK archives.
     * @param vpkPaths List of VPK archives ordered by priority (highest priority first).
     * @param isCs2 Whether this is an index for CS2 (enables native stem indexing).
     * @param token Cancellation token.
     * @param taskCtx Optional logging context for progress reporting.
     * @return Result containing the constructed VpkIndex.
     */
    static Core::Result<VpkIndex> build(
        const std::vector<Core::Path::FilesystemPath>& vpkPaths,
        bool isCs2 = false,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken(),
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Domain::Package
