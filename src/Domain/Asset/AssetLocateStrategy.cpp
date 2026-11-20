#include "Domain/Asset/AssetLocateStrategy.h"

#include <utility>
#include <QCoreApplication>

#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"
#include "Domain/Package/VpkIndex.h"

namespace Domain::Asset {

namespace {

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

    // 3. Fast-lookup via VpkIndex (priority probe if indexed)
    Core::Path::FilesystemPath probedWinnerVpk;
    if (vpkIndex) {
        auto winningVpkOpt = vpkIndex->findVpkForEntry(entryPath);
        if (winningVpkOpt.has_value()) {
            if (token.isCancelled()) {
                return Core::Result<AssetLocateOutcome>::cancelled(
                    QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
            }

            probedWinnerVpk = *winningVpkOpt;
            auto packRes = probePackEntry(pool, probedWinnerVpk, entryPath, token);
            if (packRes.isCancelled()) {
                return Core::Result<AssetLocateOutcome>::cancelled(packRes.message());
            }
            if (packRes.isFailure()) {
                return Core::Result<AssetLocateOutcome>::failure(packRes.error(), packRes.message());
            }
            if (packRes.value()) {
                AssetLocation location;
                location.sourceTargetPath = probedWinnerVpk;
                location.relativePath = entryPath;
                location.isInsidePack = true;
                return Core::Result<AssetLocateOutcome>::success(
                    AssetLocateOutcome{AssetMatchKind::PackEntry, std::move(location)});
            }
        }
    }

    // 4. Fallback search across search targets
    for (const auto& target : targets) {
        if (token.isCancelled()) {
            return Core::Result<AssetLocateOutcome>::cancelled(
                QCoreApplication::translate("AssetLocateStrategy", "Asset location cancelled"));
        }

        const Core::Path::FilesystemPath packPath = target.isVpk()
            ? target.path()
            : target.path() / QStringLiteral("pak01_dir.vpk");

        if (!probedWinnerVpk.isEmpty()
            && packPath.toString().compare(probedWinnerVpk.toString(), Qt::CaseInsensitive) == 0) {
            continue;
        }

        auto packRes = probePackEntry(pool, packPath, entryPath, token);
        if (packRes.isCancelled()) {
            return Core::Result<AssetLocateOutcome>::cancelled(packRes.message());
        }
        if (packRes.isFailure()) {
            return Core::Result<AssetLocateOutcome>::failure(packRes.error(), packRes.message());
        }
        if (packRes.value()) {
            AssetLocation location;
            location.sourceTargetPath = packPath;
            location.relativePath = entryPath;
            location.isInsidePack = true;
            return Core::Result<AssetLocateOutcome>::success(
                AssetLocateOutcome{AssetMatchKind::PackEntry, std::move(location)});
        }
    }

    return Core::Result<AssetLocateOutcome>::success(
        AssetLocateOutcome{AssetMatchKind::NotFound, std::nullopt});
}

} // namespace Domain::Asset
