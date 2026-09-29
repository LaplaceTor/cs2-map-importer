#include <QCoreApplication>
#include "Workflow/Common/AssetLocator.h"

#include <utility>

#include "Core/Error/ExecutionGuard.h"
#include "Domain/Asset/ArchiveAssetSourceProber.h"
#include "Domain/Asset/AssetLocateStrategy.h"
#include "Domain/Package/PackArchivePool.h"

namespace {

class WorkflowLocateObserver : public Domain::Asset::ILocateObserver {
public:
    explicit WorkflowLocateObserver(Core::Logging::TaskLoggingContext* taskCtx)
        : m_taskCtx(taskCtx) {}

    void onLooseFileFound(const QString& entryPath, const Core::Path::FilesystemPath& targetPath) override {
        if (m_taskCtx) {
            m_taskCtx->debug(QCoreApplication::translate("AssetLocator", "Found loose file '%1' in '%2'")
                                 .arg(entryPath, targetPath.toString()));
        }
    }

    void onVpkIndexMiss(const QString& entryPath) override {
        if (m_taskCtx) {
            m_taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' not in VPK index").arg(entryPath));
        }
    }

    void onWinnerVpkNotFound(const QString& entryPath, const Core::Path::FilesystemPath& vpkPath) override {
        Q_UNUSED(vpkPath);
        if (m_taskCtx) {
            m_taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in winning VPK")
                                 .arg(entryPath));
        }
    }

    void onTargetVpkNotFound(const QString& entryPath, const Core::Path::FilesystemPath& targetPath) override {
        if (m_taskCtx) {
            m_taskCtx->debug(QCoreApplication::translate("AssetLocator", "Asset '%1' not found in target '%2'")
                                 .arg(entryPath, targetPath.toString()));
        }
    }

    void onNativeCs2Skipped(const QString& entryPath) override {
        if (m_taskCtx) {
            m_taskCtx->info(QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2, skipping extraction").arg(entryPath));
        }
    }

private:
    Core::Logging::TaskLoggingContext* m_taskCtx = nullptr;
};

Core::Result<Domain::Asset::LocateResult> executeLocateStrategy(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const Workflow::Common::AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx,
    const QString& stage) {
    if (token.isCancelled()) {
        return Core::Result<Domain::Asset::LocateResult>::cancelled(
            QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = stage,
        .resourcePath = relativeAssetPath
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<Domain::Asset::LocateResult> {
        if (relativeAssetPath.isEmpty()) {
            return Core::Result<Domain::Asset::LocateResult>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("AssetLocator", "relative asset path is empty"));
        }

        Domain::Package::PackArchivePool localPool;
        Domain::Package::PackArchivePool& pool = options.archivePool ? *options.archivePool : localPool;

        Domain::Asset::ArchiveAssetSourceProber prober(pool, options.vpkIndex, options.cs2Index);
        WorkflowLocateObserver observer(taskCtx);

        auto result = Domain::Asset::AssetLocateStrategy::locate(
            relativeAssetPath,
            targets,
            prober,
            token,
            &observer);

        return Core::Result<Domain::Asset::LocateResult>::success(std::move(result));
    }, ctx);
}

} // namespace

namespace Workflow::Common {

Core::Result<std::optional<Domain::Asset::AssetLocation>> AssetLocator::locate(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto execRes = executeLocateStrategy(
        targets,
        relativeAssetPath,
        options,
        token,
        taskCtx,
        QStringLiteral("Locating asset"));

    if (execRes.isCancelled()) {
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::cancelled(execRes.message());
    }
    if (!execRes.isSuccess()) {
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::failure(execRes.error(), execRes.message());
    }

    auto result = execRes.value();
    const QString entryPath = Domain::Asset::AssetLocateStrategy::normalizeRelativePath(relativeAssetPath);

    switch (result.status) {
    case Domain::Asset::LocateStatus::Found:
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::success(std::move(result.location));

    case Domain::Asset::LocateStatus::NativeCs2:
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::skipped(
            QCoreApplication::translate("AssetLocator", "Asset '%1' exists natively in CS2, skipping extraction").arg(entryPath));

    case Domain::Asset::LocateStatus::Cancelled:
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::cancelled(
            QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));

    case Domain::Asset::LocateStatus::EmptyPath:
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AssetLocator", "relative asset path is empty"));

    case Domain::Asset::LocateStatus::NotFound:
    default:
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::skipped(
            QCoreApplication::translate("AssetLocator", "Asset '%1' was not found in any search target").arg(entryPath));
    }
}

Core::Result<bool> AssetLocator::exists(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto execRes = executeLocateStrategy(
        targets,
        relativeAssetPath,
        options,
        token,
        taskCtx,
        QStringLiteral("Checking asset existence"));

    if (execRes.isCancelled()) {
        return Core::Result<bool>::cancelled(execRes.message());
    }
    if (!execRes.isSuccess()) {
        return Core::Result<bool>::failure(execRes.error(), execRes.message());
    }

    const auto& result = execRes.value();
    switch (result.status) {
    case Domain::Asset::LocateStatus::NativeCs2:
    case Domain::Asset::LocateStatus::Found:
        return Core::Result<bool>::success(true);

    case Domain::Asset::LocateStatus::NotFound:
        return Core::Result<bool>::success(false);

    case Domain::Asset::LocateStatus::Cancelled:
        return Core::Result<bool>::cancelled(
            QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));

    case Domain::Asset::LocateStatus::EmptyPath:
        return Core::Result<bool>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("AssetLocator", "relative asset path is empty"));

    default:
        return Core::Result<bool>::success(false);
    }
}

} // namespace Workflow::Common
