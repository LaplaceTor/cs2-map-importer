#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Game/SearchTarget.h"

namespace Domain::Package {
class PackArchivePool;
class VpkIndex;
}

namespace Domain::Asset {

struct AssetLocateOptions {
    /**
     * @brief Optional archive pool for session-wide reuse of open pack files (VPKs).
     *        If nullptr, a call-scoped pool is used.
     */
    Domain::Package::PackArchivePool* archivePool = nullptr;

    /**
     * @brief Optional VpkIndex for fast Source 1 VPK point-lookup.
     *        When provided, eliminates blind trial-and-error across multiple VPKs.
     */
    const Domain::Package::VpkIndex* vpkIndex = nullptr;

    /**
     * @brief Optional CS2 VpkIndex for deduplication.
     *        When provided, if the asset exists natively in CS2, discovery is skipped.
     */
    const Domain::Package::VpkIndex* cs2Index = nullptr;
};

/**
 * @brief Represents the discovery location of an asset across search targets.
 */
struct AssetLocation {
    /** Target path (directory or VPK) where the asset resides. */
    Core::Path::FilesystemPath sourceTargetPath;
    /** Game-relative normalized asset path. */
    QString relativePath;
    /** True if found inside a VPK archive, false if on disk as a loose file. */
    bool isInsidePack = false;
    /** Full filesystem path on disk if it is a loose file. */
    Core::Path::FilesystemPath looseFilePath;
};

/**
 * @brief Pure detection/location service: probes search targets for an asset
 *        without performing any extraction, unpacking, or write I/O.
 */
class AssetLocator {
public:
    static Core::Result<std::optional<AssetLocation>> locate(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const AssetLocateOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);

    static bool exists(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const AssetLocateOptions& options = {},
        const Core::Async::CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr)
    {
        auto res = locate(targets, relativeAssetPath, options, token, taskCtx);
        return res.isSuccess() && res.value().has_value();
    }
};

} // namespace Domain::Asset
