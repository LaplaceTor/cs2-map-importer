#pragma once

#include <optional>
#include <vector>
#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"
#include "Domain/Asset/AssetLocation.h"
#include "Domain/Game/SearchTarget.h"

namespace Domain::Package {
class PackArchivePool;
class VpkIndex;
}

namespace Domain::Asset {

enum class AssetMatchKind : int {
    NotFound,
    LooseFile,
    PackEntry,
    NativeCs2,
};

struct AssetLocateOutcome {
    AssetMatchKind kind = AssetMatchKind::NotFound;
    std::optional<AssetLocation> location;
};

/**
 * @brief Pure domain locating strategy.
 * Evaluates candidates according to Valve Source precedence rules:
 * 1. Native CS2 asset deduplication
 * 2. Loose files across directory targets
 * 3. Fast lookup via VPK index (probes winning VPK)
 * 4. Fallback scan across all targets (ignoring already probed winner VPK)
 */
class AssetLocateStrategy {
public:
    static QString normalizeRelativePath(const QString& relativePath);

    static Core::Result<AssetLocateOutcome> locate(
        const QString& relativeAssetPath,
        const std::vector<Domain::Game::SearchTarget>& targets,
        Domain::Package::PackArchivePool& pool,
        const Domain::Package::VpkIndex* vpkIndex = nullptr,
        const Domain::Package::VpkIndex* cs2Index = nullptr,
        const Core::Async::CancellationToken& token = {});
};

} // namespace Domain::Asset
