#include "Domain/Asset/AssetLocateStrategy.h"

#include <utility>
#include <QCoreApplication>

#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"
#include "Domain/Package/VpkIndex.h"

namespace Domain::Asset {

namespace {

Core::Result<AssetLocateOutcome> makePackEntryOutcome(
    Core::Path::FilesystemPath sourceTargetPath,
    QString entryPath) {
    AssetLocation location;
    location.sourceTargetPath = std::move(sourceTargetPath);
    location.relativePath = std::move(entryPath);
    location.isInsidePack = true;
    return Core::Result<AssetLocateOutcome>::success(
        AssetLocateOutcome{AssetMatchKind::PackEntry, std::move(location)});
}

Core::Result<bool> probePackEntry(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& packPath,
    const QString& entryPath,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        return Core::Result<bool>::cancelled(
            QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
    }
    if (entryPath.isEmpty() || !packPath.exists()) {
        return Core::Result<bool>::success(false);
    }

    auto archiveRes = pool.getOrOpen(packPath, token);
    if (archiveRes.isCancelled()) {
        return Core::Result<bool>::cancelled(archiveRes.message());
    }
    if (archiveRes.isFailure()) {
        return Core::Result<bool>::failure(archiveRes.error(), archiveRes.message());
    }

    return archiveRes.value()->hasEntry(entryPath, token);
}

} // namespace

QString AssetLocateStrategy::normalizeRelativePath(const QString& relativePath) {
    QString normalized = relativePath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    return normalized;
}

Core::Result<AssetLocateOutcome> AssetLocateStrategy::locate(
    const QString& relativeAssetPath,
    const std::vector<Domain::Game::SearchTarget>& targets,
    Domain::Package::PackArchivePool& pool,
    const Domain::Package::VpkIndex* vpkIndex,
    const Domain::Package::VpkIndex* cs2Index,
    const Core::Async::CancellationToken& token) {

    if (token.isCancelled()) {
        return Core::Result<AssetLocateOutcome>::cancelled(
            QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
    }

    if (relativeAssetPath.isEmpty()) {
        return Core::Result<AssetLocateOutcome>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AssetLocateStrategy", "Relative asset path is empty"));
    }

    const QString entryPath = normalizeRelativePath(relativeAssetPath);
    if (entryPath.isEmpty()) {
        return Core::Result<AssetLocateOutcome>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AssetLocateStrategy", "Relative asset path is empty"));
    }

    // 1. CS2 native asset deduplication check
    if (cs2Index && cs2Index->hasCs2NativeAsset(entryPath)) {
        return Core::Result<AssetLocateOutcome>::success(
            AssetLocateOutcome{AssetMatchKind::NativeCs2, std::nullopt});
    }

    // 2. Loose files across directory targets (highest precedence)
    for (const auto& target : targets) {
        if (token.isCancelled()) {
            return Core::Result<AssetLocateOutcome>::cancelled(
                QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
        }

        if (!target.isDirectory()) {
            continue;
        }

        auto resolvedOpt = target.path().resolveBelow(entryPath);
        if (resolvedOpt.has_value() && resolvedOpt->exists()) {
            AssetLocation location;
            location.sourceTargetPath = target.path();
            location.relativePath = entryPath;
            location.isInsidePack = false;
            location.looseFilePath = *resolvedOpt;
            return Core::Result<AssetLocateOutcome>::success(
                AssetLocateOutcome{AssetMatchKind::LooseFile, std::move(location)});
        }
    }

    // 3. Pre-query VpkIndex once in O(1) (zero VPK opens)
    std::optional<Core::Path::FilesystemPath> indexedWinner;
    QString indexedWinnerKey;
    if (vpkIndex) {
        indexedWinner = vpkIndex->findVpkForEntry(entryPath);
        if (indexedWinner.has_value()) {
            indexedWinnerKey = Domain::Package::VpkIndex::normalizeArchivePathKey(*indexedWinner);
        }
    }

    // 4. Walk search targets in strict Valve priority order
    bool indexedWinnerMatchedTarget = false;
    for (const auto& target : targets) {
        if (token.isCancelled()) {
            return Core::Result<AssetLocateOutcome>::cancelled(
                QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
        }

        const Core::Path::FilesystemPath packPath = target.isVpk()
            ? target.path()
            : target.path() / QStringLiteral("pak01_dir.vpk");

        if (vpkIndex && vpkIndex->coversVpk(packPath)) {
            if (!indexedWinnerKey.isEmpty()
                && Domain::Package::VpkIndex::normalizeArchivePathKey(packPath) == indexedWinnerKey) {
                indexedWinnerMatchedTarget = true;
                return makePackEntryOutcome(packPath, entryPath);
            }
            // Covered by verified VpkIndex and not the winning archive -> skip without opening VPK.
            continue;
        }

        // Unindexed VPK target -> fallback probe via PackArchivePool
        auto packRes = probePackEntry(pool, packPath, entryPath, token);
        if (packRes.isCancelled()) {
            return Core::Result<AssetLocateOutcome>::cancelled(packRes.message());
        }
        if (packRes.isFailure()) {
            return Core::Result<AssetLocateOutcome>::failure(packRes.error(), packRes.message());
        }
        if (packRes.value()) {
            return makePackEntryOutcome(packPath, entryPath);
        }
    }

    // Preserve vpkIndex-only lookups (e.g. caller passed empty targets or omitted an indexed VPK)
    if (indexedWinner.has_value() && !indexedWinnerMatchedTarget) {
        return makePackEntryOutcome(*indexedWinner, entryPath);
    }

    return Core::Result<AssetLocateOutcome>::success(
        AssetLocateOutcome{AssetMatchKind::NotFound, std::nullopt});
}

} // namespace Domain::Asset
