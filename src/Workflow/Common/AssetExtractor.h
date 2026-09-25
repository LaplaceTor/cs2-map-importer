#pragma once

#include <vector>

#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Asset/AssetLocator.h"
#include "Domain/Game/SearchTarget.h"

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

struct AssetExtractOptions {
    Domain::Asset::AssetLocateOptions locateOptions;
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
     */
    static Core::Result<AssetExtraction> extractLocated(
        const Domain::Asset::AssetLocation& location,
        const Core::Path::FilesystemPath& destContentDir,
        const std::vector<QString>& companionExtensions = {},
        Domain::Package::PackArchivePool* archivePool = nullptr,
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    /**
     * @brief Locates an asset across search targets and extracts it into destContentDir.
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
