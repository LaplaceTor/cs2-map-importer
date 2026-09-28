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
 * @brief Workflow service for asset discovery and reporting.
 * Coordinates Domain::Asset::AssetLocateStrategy with Domain::Asset::ArchiveAssetSourceProber,
 * handling CancellationToken checks, user-visible translations, and TaskLoggingContext output.
 */
class AssetLocator {
public:
    static Core::Result<std::optional<Domain::Asset::AssetLocation>> locate(
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

} // namespace Workflow::Common
