#pragma once

#include <functional>
#include <optional>
#include <vector>
#include <QString>

#include "Domain/Asset/AssetLocation.h"
#include "Domain/Asset/IAssetSourceProber.h"
#include "Domain/Game/SearchTarget.h"

namespace Domain::Asset {

enum class LocateStatus {
    Found,
    NativeCs2,
    NotFound,
    EmptyPath,
    Cancelled
};

struct LocateResult {
    LocateStatus status = LocateStatus::NotFound;
    std::optional<AssetLocation> location;
    Core::Path::FilesystemPath probedWinnerVpk;
};

/**
 * @brief Observer interface for location strategy diagnostic events.
 * Decouples domain strategy from logging frameworks and string translations.
 */
class ILocateObserver {
public:
    virtual ~ILocateObserver() = default;
    virtual void onLooseFileFound(const QString& entryPath, const Core::Path::FilesystemPath& targetPath) { Q_UNUSED(entryPath); Q_UNUSED(targetPath); }
    virtual void onVpkIndexMiss(const QString& entryPath) { Q_UNUSED(entryPath); }
    virtual void onWinnerVpkNotFound(const QString& entryPath, const Core::Path::FilesystemPath& vpkPath) { Q_UNUSED(entryPath); Q_UNUSED(vpkPath); }
    virtual void onTargetVpkNotFound(const QString& entryPath, const Core::Path::FilesystemPath& targetPath) { Q_UNUSED(entryPath); Q_UNUSED(targetPath); }
    virtual void onNativeCs2Skipped(const QString& entryPath) { Q_UNUSED(entryPath); }
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

    static LocateResult locate(
        const QString& relativeAssetPath,
        const std::vector<Domain::Game::SearchTarget>& targets,
        IAssetSourceProber& prober,
        const std::function<bool()>& isCancelled = nullptr,
        ILocateObserver* observer = nullptr);
};

} // namespace Domain::Asset
