#include "Workflow/Common/AssetLocator.h"

#include <utility>
#include <QCoreApplication>

#include "Domain/Asset/AssetLocateStrategy.h"
#include "Domain/Package/PackArchivePool.h"

namespace {

Core::Result<Domain::Asset::AssetLocateOutcome> runLocate(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const Workflow::Common::AssetLocateOptions& options,
    const Core::Async::CancellationToken& token) {
    Domain::Package::PackArchivePool localPool;
    Domain::Package::PackArchivePool& pool = options.archivePool ? *options.archivePool : localPool;

    return Domain::Asset::AssetLocateStrategy::locate(
        relativeAssetPath,
        targets,
        pool,
        options.vpkIndex,
        options.cs2Index,
        token);
}

} // namespace

namespace Workflow::Common {

Core::Result<std::optional<Domain::Asset::AssetLocation>> AssetLocator::locate(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto res = runLocate(targets, relativeAssetPath, options, token);
    if (res.isCancelled()) {
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::cancelled(res.message());
    }
    if (!res.isSuccess()) {
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::failure(res.error(), res.message());
    }

    auto outcome = std::move(res.value());
    const QString entryPath = Domain::Asset::AssetLocateStrategy::normalizeRelativePath(relativeAssetPath);

    switch (outcome.kind) {
    case Domain::Asset::AssetMatchKind::LooseFile:
        if (taskCtx && outcome.location.has_value()) {
            taskCtx->debug(QCoreApplication::translate("AssetLocator", "Found loose file '%1' in '%2'")
                               .arg(entryPath, outcome.location->sourceTargetPath.toString()));
        }
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::success(std::move(outcome.location));

    case Domain::Asset::AssetMatchKind::PackEntry:
        if (taskCtx && outcome.location.has_value()) {
            taskCtx->debug(QCoreApplication::translate("AssetLocator", "Found pack entry '%1' in '%2'")
                               .arg(entryPath, outcome.location->sourceTargetPath.toString()));
        }
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::success(std::move(outcome.location));

    case Domain::Asset::AssetMatchKind::NativeCs2: {
        const QString msg = QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2, skipping extraction")
                                .arg(entryPath);
        if (taskCtx) {
            taskCtx->info(msg);
        }
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::skipped(msg);
    }

    case Domain::Asset::AssetMatchKind::NotFound:
    default: {
        const QString msg = QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in any search target")
                                .arg(entryPath);
        if (taskCtx) {
            taskCtx->debug(msg);
        }
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::skipped(msg);
    }
    }
}

Core::Result<bool> AssetLocator::exists(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto res = runLocate(targets, relativeAssetPath, options, token);
    if (res.isCancelled()) {
        return Core::Result<bool>::cancelled(res.message());
    }
    if (!res.isSuccess()) {
        return Core::Result<bool>::failure(res.error(), res.message());
    }

    const auto& outcome = res.value();
    const bool found = outcome.kind != Domain::Asset::AssetMatchKind::NotFound;
    if (taskCtx && outcome.kind == Domain::Asset::AssetMatchKind::NativeCs2) {
        const QString entryPath = Domain::Asset::AssetLocateStrategy::normalizeRelativePath(relativeAssetPath);
        taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2").arg(entryPath));
    }
    return Core::Result<bool>::success(found);
}

} // namespace Workflow::Common
