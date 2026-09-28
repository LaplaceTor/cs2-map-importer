#pragma once

#include <optional>
#include <vector>
#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include "Domain/Asset/AssetLocation.h"
#include "Domain/Game/SearchTarget.h"

namespace Domain::Package {
class PackArchivePool;
class VpkIndex;
}

namespace Workflow::Common {

/**
 * @brief Configuration and borrowed resources for asset discovery.
 *
 * ### Lifecycle & Concurrency Contract:
 * - **`archivePool`**:
 *     - Lifecycle: Borrowed pointer. If non-null, caller guarantees the pool remains
 *       valid for the entire duration of the locate() / exists() call. If nullptr, a
 *       temporary call-scoped PackArchivePool is instantiated internally.
 *     - Thread Safety: PackArchivePool is internally thread-safe (mutex-protected).
 *       It may be safely shared concurrently across multiple worker threads and tasks.
 * - **`vpkIndex` & `cs2Index`**:
 *     - Lifecycle & Mutability: Borrowed read-only pointers. Must remain valid and
 *       IMMUTABLE during the entire duration of the call.
 *     - Thread Safety: Read-only queries on VpkIndex are thread-safe and reentrant;
 *       the same index instance may be queried concurrently from multiple worker threads.
 */
struct AssetLocateOptions {
    Domain::Package::PackArchivePool* archivePool = nullptr;
    const Domain::Package::VpkIndex* vpkIndex = nullptr;
    const Domain::Package::VpkIndex* cs2Index = nullptr;
};

/**
 * @brief Workflow service for asset discovery and reporting.
 * Coordinates Domain::Asset::AssetLocateStrategy with Domain::Asset::ArchiveAssetSourceProber,
 * handling CancellationToken checks, user-visible translations, and TaskLoggingContext output.
 */
class AssetLocator {
public:
    /**
     * @brief Locates an asset across search targets.
     *
     * @param targets Ordered list of search targets (loose directories and VPK files).
     * @param relativeAssetPath Path to the asset relative to game root (e.g. "materials/brick/wall.vmt").
     * @param options Discovery options (optional shared archive pool and indices).
     * @param token Cancellation token checked before and during location steps.
     * @param taskCtx Optional task logging context for diagnostic reporting (single-task lifecycle, not thread-safe).
     * @return Result containing optional AssetLocation (nullopt if not found), or skipped / cancelled / failure.
     */
    static Core::Result<std::optional<Domain::Asset::AssetLocation>> locate(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const AssetLocateOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    /**
     * @brief Checks whether the asset exists in any of the search targets.
     *
     * Unlike a boolean return value, returns Core::Result<bool> to preserve
     * cancellation, invalid argument, or underlying filesystem/archive error diagnostics.
     *
     * @return Core::Result<bool> containing true if found, false if not found,
     *         or error / cancelled if discovery encountered a failure or cancellation.
     */
    static Core::Result<bool> exists(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const AssetLocateOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Workflow::Common
