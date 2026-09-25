#include <QCoreApplication>
#include "Domain/Asset/AssetLocator.h"

#include <exception>
#include <utility>

#include "Core/Error/Exception.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"
#include "Domain/Package/VpkIndex.h"

namespace {

template<typename Fn>
auto runGuarded(Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const Core::Error::Exception& ex) {
        return decltype(fn())::failure(ex.error());
    } catch (const std::exception& ex) {
        return decltype(fn())::failure(
            Core::Error::ErrorCode::OperationFailed,
            QString::fromUtf8(ex.what()));
    }
}

QString normalizeRelativePath(const QString& relativePath) {
    QString normalized = relativePath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    return normalized;
}

Core::Result<bool> probeEntryInPack(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& packPath,
    const QString& entryPath) {
    if (!packPath.exists()) {
        return Core::Result<bool>::success(false);
    }

    auto archiveRes = pool.getOrOpen(packPath);
    if (archiveRes.isFailure()) {
        return Core::Result<bool>::failure(archiveRes.error());
    }
    return Core::Result<bool>::success(archiveRes.value()->hasEntry(entryPath));
}

} // namespace

namespace Domain::Asset {

Core::Result<std::optional<AssetLocation>> AssetLocator::locate(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    return runGuarded([&]() -> Core::Result<std::optional<AssetLocation>> {
        if (relativeAssetPath.isEmpty()) {
            return Core::Result<std::optional<AssetLocation>>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("AssetLocator", "relative asset path is empty"));
        }

        Domain::Package::PackArchivePool localPool;
        Domain::Package::PackArchivePool& pool = options.archivePool ? *options.archivePool : localPool;

        const QString entryPath = normalizeRelativePath(relativeAssetPath);

        // 1. CS2 native asset deduplication check
        if (options.cs2Index && options.cs2Index->hasCs2NativeAsset(entryPath)) {
            if (taskCtx) {
                taskCtx->info(QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2, skipping extraction").arg(entryPath));
            }
            return Core::Result<std::optional<AssetLocation>>::skipped(
                QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2, skipping extraction").arg(entryPath));
        }

        // 2. Search loose files on disk across directory targets first
        for (const auto& target : targets) {
            if (token.isCancelled()) {
                return Core::Result<std::optional<AssetLocation>>::cancelled(
                    QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));
            }

            if (target.isDirectory()) {
                const Core::Path::FilesystemPath looseFile = target.path() / entryPath;
                if (looseFile.exists()) {
                    if (taskCtx) {
                        taskCtx->debug(QCoreApplication::translate("AssetLocator", "Found loose file '%1' in '%2'")
                                           .arg(entryPath, target.pathString()));
                    }

                    AssetLocation location;
                    location.sourceTargetPath = target.path();
                    location.relativePath = entryPath;
                    location.isInsidePack = false;
                    location.looseFilePath = looseFile;
                    return Core::Result<std::optional<AssetLocation>>::success(std::move(location));
                }
            }
        }

        // 3. Fast-lookup via VpkIndex (eliminates blind search / misses)
        if (options.vpkIndex) {
            auto winningVpkOpt = options.vpkIndex->findVpkForEntry(entryPath);
            if (!winningVpkOpt.has_value()) {
                if (taskCtx) {
                    taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' not in VPK index").arg(entryPath));
                }
                return Core::Result<std::optional<AssetLocation>>::skipped(
                    QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in any search target").arg(entryPath));
            }

            const Core::Path::FilesystemPath& winningVpk = winningVpkOpt.value();
            auto probeRes = probeEntryInPack(pool, winningVpk, entryPath);
            if (probeRes.isFailure()) {
                return Core::Result<std::optional<AssetLocation>>::failure(
                    probeRes.error(),
                    QCoreApplication::translate("AssetLocator", "Failed to probe archive '%1'").arg(winningVpk.toString()));
            }

            if (probeRes.value()) {
                AssetLocation location;
                location.sourceTargetPath = winningVpk;
                location.relativePath = entryPath;
                location.isInsidePack = true;
                return Core::Result<std::optional<AssetLocation>>::success(std::move(location));
            }

            return Core::Result<std::optional<AssetLocation>>::skipped(
                QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in winning VPK").arg(entryPath));
        }

        // 4. Fallback search (when no VpkIndex is provided)
        for (const auto& target : targets) {
            if (token.isCancelled()) {
                return Core::Result<std::optional<AssetLocation>>::cancelled(
                    QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));
            }

            const Core::Path::FilesystemPath packPath = target.isVpk()
                ? target.path()
                : target.path() / QStringLiteral("pak01_dir.vpk");

            auto probeRes = probeEntryInPack(pool, packPath, entryPath);
            if (probeRes.isFailure()) {
                return Core::Result<std::optional<AssetLocation>>::failure(
                    probeRes.error(),
                    QCoreApplication::translate("AssetLocator", "Failed to probe archive '%1'").arg(packPath.toString()));
            }

            if (!probeRes.value()) {
                if (taskCtx) {
                    taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' not found in target '%2'")
                                       .arg(entryPath, target.pathString()));
                }
                continue;
            }

            AssetLocation location;
            location.sourceTargetPath = target.path();
            location.relativePath = entryPath;
            location.isInsidePack = true;
            return Core::Result<std::optional<AssetLocation>>::success(std::move(location));
        }

        return Core::Result<std::optional<AssetLocation>>::skipped(
            QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in any search target").arg(entryPath));
    });
}

} // namespace Domain::Asset
