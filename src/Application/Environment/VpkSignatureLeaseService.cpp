#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Domain/Game/GameType.h"
#include <QDir>
#include <QFileInfo>
#include <utility>

namespace Application::Environment {

VpkSignatureLeaseService::VpkSignatureLeaseService(
    std::shared_ptr<Core::Logging::TaskLoggingContext> loggingContext,
    QObject* parent)
    : QObject(parent)
    , m_loggingContext(std::move(loggingContext))
{
}

VpkSignatureLeaseService::VpkSignatureLeaseService(QObject* parent)
    : QObject(parent)
    , m_loggingContext(nullptr)
{
}

VpkSignatureLeaseService::~VpkSignatureLeaseService()
{
    releaseLease();
}

void VpkSignatureLeaseService::setLoggingContext(std::shared_ptr<Core::Logging::TaskLoggingContext> loggingContext) noexcept
{
    m_loggingContext = std::move(loggingContext);
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::updateInstallation(const GameInstallation& s2Installation)
{
    return Application::Execution::ExecutionGuard::guard<VpkSignatureLeaseResult>([&]() {
        return updateInstallationRaw(s2Installation);
    }, QStringLiteral("Failed to update VPK signature lease"));
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::updateInstallation(const GameInstallationInfo& s2Info)
{
    return Application::Execution::ExecutionGuard::guard<VpkSignatureLeaseResult>([&]() {
        return updateInstallationRaw(s2Info);
    }, QStringLiteral("Failed to update VPK signature lease"));
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::acquireLease(const Core::Path::FilesystemPath& cs2BasePath)
{
    return Application::Execution::ExecutionGuard::guard<VpkSignatureLeaseResult>([&]() {
        return acquireLeaseRaw(cs2BasePath);
    }, QStringLiteral("Failed to acquire VPK signature lease"));
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::acquireLease(const QString& cs2BasePath)
{
    return acquireLease(Core::Path::FilesystemPath(cs2BasePath));
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::retryLease()
{
    return Application::Execution::ExecutionGuard::guard<VpkSignatureLeaseResult>([&]() {
        return retryLeaseRaw();
    }, QStringLiteral("VPK signature lease retry failed"));
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::updateInstallationRaw(const GameInstallation& s2Installation)
{
    m_activeInstallation = s2Installation;

    if (m_activeInstallation.isValid() && m_activeInstallation.type() == Domain::Game::GameType::CS2) {
        return acquireLeaseRaw(m_activeInstallation.baseDirectory());
    }

    releaseLease();
    m_lastStatus = VpkSignatureLeaseStatus::Inactive;
    emit leaseStatusChanged(m_lastStatus, QString(), QString());
    VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QString(), QString()};
    return Core::Result<VpkSignatureLeaseResult>::success(res);
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::updateInstallationRaw(const GameInstallationInfo& s2Info)
{
    if (s2Info.isValid && (s2Info.gameId == QStringLiteral("cs2") || s2Info.gameTitle == QStringLiteral("Counter-Strike 2") || s2Info.displayName == QStringLiteral("Counter-Strike 2"))) {
        return acquireLeaseRaw(Core::Path::FilesystemPath(s2Info.basePath));
    }

    releaseLease();
    m_lastStatus = VpkSignatureLeaseStatus::Inactive;
    emit leaseStatusChanged(m_lastStatus, QString(), QString());
    VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QString(), QString()};
    return Core::Result<VpkSignatureLeaseResult>::success(res);
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::retryLeaseRaw()
{
    if (m_activeInstallation.isValid() && m_activeInstallation.type() == Domain::Game::GameType::CS2) {
        return acquireLeaseRaw(m_activeInstallation.baseDirectory());
    }
    VpkSignatureLeaseResult res{VpkSignatureLeaseStatus::Inactive, QStringLiteral("No active CS2 installation to retry leasing"), QString()};
    return Core::Result<VpkSignatureLeaseResult>::failure(Core::Error::ErrorCode::InvalidArgument, res.systemMessage, QString(), res);
}

Core::Result<VpkSignatureLeaseResult> VpkSignatureLeaseService::acquireLeaseRaw(const Core::Path::FilesystemPath& cs2BasePath)
{
    VpkSignatureLeaseResult result;

    if (cs2BasePath.isEmpty() || !cs2BasePath.exists() || !cs2BasePath.isDirectory()) {
        result.status = VpkSignatureLeaseStatus::NotFound;
        result.systemMessage = QStringLiteral("CS2 base directory is invalid or does not exist");
        m_lastStatus = result.status;
        if (m_loggingContext) {
            m_loggingContext->warning(QStringLiteral("CS2 base directory is invalid or does not exist: %1").arg(cs2BasePath.toString()));
        }
        emit leaseStatusChanged(result.status, QString(), result.systemMessage);
        return Core::Result<VpkSignatureLeaseResult>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            result.systemMessage,
            cs2BasePath.toString(),
            result);
    }

    // Target is strictly: <cs2BasePath>/game/bin/win64/vpk.signatures
    const QString targetPath = QDir(cs2BasePath.toString()).filePath(QStringLiteral("game/bin/win64/vpk.signatures"));
    result.targetPath = targetPath;
    const QFileInfo targetInfo(targetPath);

    if (!targetInfo.exists()) {
        result.status = VpkSignatureLeaseStatus::NotFound;
        result.systemMessage = QStringLiteral("vpk.signatures does not exist at expected path");
        m_lastStatus = result.status;
        if (m_loggingContext) {
            m_loggingContext->warning(QStringLiteral("vpk.signatures does not exist at expected path: %1").arg(targetPath));
        }
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return Core::Result<VpkSignatureLeaseResult>::failure(
            Core::Error::ErrorCode::FileNotFound,
            result.systemMessage,
            targetPath,
            result);
    }

    if (!targetInfo.isFile()) {
        result.status = VpkSignatureLeaseStatus::Failed;
        result.systemMessage = QStringLiteral("Target vpk.signatures is not a regular file");
        m_lastStatus = result.status;
        if (m_loggingContext) {
            m_loggingContext->warning(QStringLiteral("Target vpk.signatures is not a regular file: %1").arg(targetPath));
        }
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return Core::Result<VpkSignatureLeaseResult>::failure(
            Core::Error::ErrorCode::InvalidPath,
            result.systemMessage,
            targetPath,
            result);
    }

    // If we already hold a lease on the exact same path, keep it active
    if (m_lease.isHeld() && m_lease.filePath() == QDir::toNativeSeparators(targetInfo.absoluteFilePath())) {
        result.status = VpkSignatureLeaseStatus::Acquired;
        m_lastStatus = result.status;
        return Core::Result<VpkSignatureLeaseResult>::success(result);
    }

    // Release previous lease before acquiring new one
    releaseLease();

    const Core::FileSystem::FileLeaseResult leaseRes = m_lease.acquireExclusive(targetPath);
    if (!leaseRes.isSuccess()) {
        Core::Error::ErrorCode errCode = Core::Error::ErrorCode::OperationFailed;
        switch (leaseRes.error) {
        case Core::FileSystem::FileLeaseError::AlreadyInUse:
            result.status = VpkSignatureLeaseStatus::AlreadyInUse;
            errCode = Core::Error::ErrorCode::ResourceBusy;
            break;
        case Core::FileSystem::FileLeaseError::AccessDenied:
            result.status = VpkSignatureLeaseStatus::AccessDenied;
            errCode = Core::Error::ErrorCode::PermissionDenied;
            break;
        case Core::FileSystem::FileLeaseError::NotFound:
            result.status = VpkSignatureLeaseStatus::NotFound;
            errCode = Core::Error::ErrorCode::FileNotFound;
            break;
        default:
            result.status = VpkSignatureLeaseStatus::Failed;
            errCode = Core::Error::ErrorCode::OperationFailed;
            break;
        }

        result.systemMessage = leaseRes.message;
        m_lastStatus = result.status;
        if (m_loggingContext) {
            m_loggingContext->warning(QStringLiteral("Failed to acquire exclusive lease for vpk.signatures at '%1': %2")
                .arg(targetPath)
                .arg(leaseRes.message));
        }
        emit leaseStateChanged(false, QString());
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return Core::Result<VpkSignatureLeaseResult>::failure(
            errCode,
            result.systemMessage,
            targetPath,
            result);
    }

    result.status = VpkSignatureLeaseStatus::Acquired;
    m_lastStatus = result.status;
    if (m_loggingContext) {
        m_loggingContext->info(QStringLiteral("Acquired exclusive file lease on %1").arg(m_lease.filePath()));
    }

    emit leaseStateChanged(true, m_lease.filePath());
    emit leaseStatusChanged(result.status, m_lease.filePath(), QString());
    return Core::Result<VpkSignatureLeaseResult>::success(result);
}

void VpkSignatureLeaseService::releaseLease() noexcept
{
    if (m_lease.isHeld()) {
        const QString prevPath = m_lease.filePath();
        m_lease.release();
        m_lastStatus = VpkSignatureLeaseStatus::Inactive;
        if (m_loggingContext) {
            m_loggingContext->info(QStringLiteral("Released exclusive file lease on %1").arg(prevPath));
        }
        emit leaseStateChanged(false, QString());
    }
}

bool VpkSignatureLeaseService::isLeaseHeld() const noexcept
{
    return m_lease.isHeld();
}

QString VpkSignatureLeaseService::leasedFilePath() const
{
    return m_lease.filePath();
}

} // namespace Application::Environment
