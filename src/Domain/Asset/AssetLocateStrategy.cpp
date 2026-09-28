#include "Domain/Asset/AssetLocateStrategy.h"

namespace Domain::Asset {

QString AssetLocateStrategy::normalizeRelativePath(const QString& relativePath) {
    QString normalized = relativePath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    return normalized;
}

LocateResult AssetLocateStrategy::locate(
    const QString& relativeAssetPath,
    const std::vector<Domain::Game::SearchTarget>& targets,
    IAssetSourceProber& prober,
    const std::function<bool()>& isCancelled,
    ILocateObserver* observer) {

    if (relativeAssetPath.isEmpty()) {
        return LocateResult{LocateStatus::EmptyPath, std::nullopt, {}};
    }

    if (isCancelled && isCancelled()) {
        return LocateResult{LocateStatus::Cancelled, std::nullopt, {}};
    }

    const QString entryPath = normalizeRelativePath(relativeAssetPath);

    // 1. CS2 native asset deduplication check
    if (prober.isNativeCs2Asset(entryPath)) {
        if (observer) {
            observer->onNativeCs2Skipped(entryPath);
        }
        return LocateResult{LocateStatus::NativeCs2, std::nullopt, {}};
    }

    // 2. Loose files across directory targets (highest precedence)
    for (const auto& target : targets) {
        if (isCancelled && isCancelled()) {
            return LocateResult{LocateStatus::Cancelled, std::nullopt, {}};
        }

        if (target.isDirectory() && prober.hasLooseFile(target, entryPath)) {
            const Core::Path::FilesystemPath looseFile = target.path() / entryPath;
            if (observer) {
                observer->onLooseFileFound(entryPath, target.path());
            }

            AssetLocation location;
            location.sourceTargetPath = target.path();
            location.relativePath = entryPath;
            location.isInsidePack = false;
            location.looseFilePath = looseFile;
            return LocateResult{LocateStatus::Found, std::move(location), {}};
        }
    }

    // 3. Fast-lookup via VpkIndex (priority probe if indexed)
    Core::Path::FilesystemPath probedWinnerVpk;
    auto winningVpkOpt = prober.queryVpkIndex(entryPath);
    if (winningVpkOpt.has_value()) {
        if (isCancelled && isCancelled()) {
            return LocateResult{LocateStatus::Cancelled, std::nullopt, {}};
        }

        const Core::Path::FilesystemPath& winningVpk = winningVpkOpt.value();
        probedWinnerVpk = winningVpk;

        if (prober.hasPackEntry(winningVpk, entryPath)) {
            AssetLocation location;
            location.sourceTargetPath = winningVpk;
            location.relativePath = entryPath;
            location.isInsidePack = true;
            return LocateResult{LocateStatus::Found, std::move(location), probedWinnerVpk};
        } else {
            if (observer) {
                observer->onWinnerVpkNotFound(entryPath, winningVpk);
            }
        }
    } else {
        if (observer) {
            observer->onVpkIndexMiss(entryPath);
        }
    }

    // 4. Fallback search across search targets
    for (const auto& target : targets) {
        if (isCancelled && isCancelled()) {
            return LocateResult{LocateStatus::Cancelled, std::nullopt, probedWinnerVpk};
        }

        const Core::Path::FilesystemPath packPath = target.isVpk()
            ? target.path()
            : target.path() / QStringLiteral("pak01_dir.vpk");

        if (!probedWinnerVpk.isEmpty() && packPath.toString().compare(probedWinnerVpk.toString(), Qt::CaseInsensitive) == 0) {
            continue;
        }

        if (prober.hasPackEntry(packPath, entryPath)) {
            AssetLocation location;
            location.sourceTargetPath = packPath;
            location.relativePath = entryPath;
            location.isInsidePack = true;
            return LocateResult{LocateStatus::Found, std::move(location), probedWinnerVpk};
        } else {
            if (observer) {
                observer->onTargetVpkNotFound(entryPath, target.path());
            }
        }
    }

    return LocateResult{LocateStatus::NotFound, std::nullopt, probedWinnerVpk};
}

} // namespace Domain::Asset
