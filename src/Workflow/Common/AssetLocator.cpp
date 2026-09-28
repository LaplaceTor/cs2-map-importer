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

} // namespace

namespace Workflow::Common {

Core::Result<std::optional<Domain::Asset::AssetLocation>> AssetLocator::locate(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<std::optional<Domain::Asset::AssetLocation>>::cancelled(
            QCoreApplication::translate("AssetLocator", "Asset extraction cancelled"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Locating asset"),
        .resourcePath = relativeAssetPath
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<std::optional<Domain::Asset::AssetLocation>> {
        if (relativeAssetPath.isEmpty()) {
            return Core::Result<std::optional<Domain::Asset::AssetLocation>>::failure(
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
    }, ctx);
}

Core::Result<bool> AssetLocator::exists(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const AssetLocateOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto res = locate(targets, relativeAssetPath, options, token, taskCtx);
    if (res.isCancelled()) {
        return Core::Result<bool>::cancelled(res.message());
    }
    if (res.isSkipped()) {
        return Core::Result<bool>::skipped(res.message(), false);
    }
    if (!res.isSuccess()) {
        return Core::Result<bool>::failure(res.error(), res.message());
    }
    return Core::Result<bool>::success(res.value().has_value(), res.message());
}

} // namespace Workflow::Common
