#pragma once

#include <vector>
#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Asset/AssetLocation.h"
#include "Domain/Game/SearchTarget.h"
#include "Workflow/Common/AssetLocator.h"

namespace Domain::Package {
class PackArchivePool;
}

namespace Workflow::Common {

/**
 * @brief Outcome payload of a successful asset extraction.
 */
struct AssetExtraction {
    /** File path where the extracted asset landed inside the destination directory. */
    Core::Path::FilesystemPath extractedFilePath;
    /** Search target the asset was found in. */
    Core::Path::FilesystemPath sourceTargetPath;
    /** True when the asset came from inside a pack archive rather than a loose folder. */
    bool fromPack = false;
};

/**
 * @brief Configuration and borrowed resources for asset extraction.
 *
 * ### Lifecycle & Concurrency Contract:
 * - **`locateOptions`**:
 *     - `locateOptions.archivePool`: Borrowed pointer to thread-safe PackArchivePool.
 *       If non-null, caller guarantees it outlives the extraction call.
 *       If nullptr, a local call-scoped pool is instantiated.
 *     - `locateOptions.vpkIndex` & `cs2Index`: Borrowed pointers to immutable indices.
 *       Safe for concurrent multi-threaded queries.
 * - **`companionExtensions`**: List of companion file extensions (e.g. "vvd", "phy", "dx90.vtx")
 *   to extract alongside the primary asset.
 */
struct AssetExtractOptions {
    AssetLocateOptions locateOptions;
    /**
     * @brief Additional file extensions (without dot) extracted alongside the
     *        asset from the winning target, e.g. model companion files
     *        ("vvd", "phy", ...). Companions are best-effort: missing or
     *        failing companions are logged as warnings, never failures.
     */
    std::vector<QString> companionExtensions;
};

/**
 * @brief Use case: extract an asset into a destination content directory,
 *        either from an already determined AssetLocation or by discovering it first.
 */
class AssetExtractor {
public:
    /**
     * @brief Extracts an already located asset directly without repeating discovery.
     *
     * @param location Previously located asset record.
     * @param destContentDir Destination root directory for extraction.
     * @param options Extraction options (borrowed archive pool, companion extensions).
     * @param token Cancellation token checked before and during heavy stream operations.
     * @param taskCtx Optional task logging context (single-task lifecycle, not thread-safe).
     * @return Result containing AssetExtraction metadata, or failure/cancelled.
     */
    static Core::Result<AssetExtraction> extractLocated(
        const Domain::Asset::AssetLocation& location,
        const Core::Path::FilesystemPath& destContentDir,
        const AssetExtractOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    /**
     * @brief Locates an asset across search targets and extracts it into destContentDir.
     *
     * @param targets Ordered list of candidate search targets.
     * @param relativeAssetPath Path to the asset relative to game root.
     * @param destContentDir Destination root directory for extraction.
     * @param options Discovery and extraction options.
     * @param token Cancellation token checked before and during heavy stream operations.
     * @param taskCtx Optional task logging context (single-task lifecycle, not thread-safe).
     * @return Result containing AssetExtraction metadata, or failure/cancelled.
     */
    static Core::Result<AssetExtraction> extract(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const Core::Path::FilesystemPath& destContentDir,
        const AssetExtractOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Workflow::Common
