#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Logging/Logger.h"
#include "Domain/Game/GameType.h"
#include <QDir>
#include <QFileInfo>

namespace Application::Environment {

VpkSignatureLeaseService::VpkSignatureLeaseService(QObject* parent)
    : QObject(parent)
{
}

VpkSignatureLeaseService::~VpkSignatureLeaseService()
{
    releaseLease();
}

VpkSignatureLeaseResult VpkSignatureLeaseService::updateInstallation(const GameInstallation& s2Installation)
{
    m_activeInstallation = s2Installation;

    if (m_activeInstallation.isValid() && m_activeInstallation.type() == Domain::Game::GameType::CS2) {
        return acquireLeaseInternal(m_activeInstallation.baseDirectory());
    }

    releaseLease();
    m_lastStatus = VpkSignatureLeaseStatus::Inactive;
    emit leaseStatusChanged(m_lastStatus, QString(), QString());
    return {VpkSignatureLeaseStatus::Inactive, QString(), QString()};
}

VpkSignatureLeaseResult VpkSignatureLeaseService::acquireLease(const Core::Path::FilesystemPath& cs2BasePath)
{
    return acquireLeaseInternal(cs2BasePath);
}

VpkSignatureLeaseResult VpkSignatureLeaseService::retryLease()
{
    if (m_activeInstallation.isValid() && m_activeInstallation.type() == Domain::Game::GameType::CS2) {
        return acquireLeaseInternal(m_activeInstallation.baseDirectory());
    }
    return {VpkSignatureLeaseStatus::Inactive, QString(), QString()};
}

VpkSignatureLeaseResult VpkSignatureLeaseService::acquireLeaseInternal(const Core::Path::FilesystemPath& cs2BasePath)
{
    VpkSignatureLeaseResult result;

    if (cs2BasePath.isEmpty() || !cs2BasePath.exists() || !cs2BasePath.isDirectory()) {
        result.status = VpkSignatureLeaseStatus::NotFound;
        result.systemMessage = QStringLiteral("CS2 base directory is invalid or does not exist: %1").arg(cs2BasePath.toString());
        m_lastStatus = result.status;
        Core::Logging::Logger::warning(result.systemMessage);
        emit leaseStatusChanged(result.status, QString(), result.systemMessage);
        return result;
    }

    // Target is strictly: <cs2BasePath>/game/bin/win64/vpk.signatures
    const QString targetPath = QDir(cs2BasePath.toString()).filePath(QStringLiteral("game/bin/win64/vpk.signatures"));
    result.targetPath = targetPath;
    const QFileInfo targetInfo(targetPath);

    if (!targetInfo.exists()) {
        result.status = VpkSignatureLeaseStatus::NotFound;
        result.systemMessage = QStringLiteral("vpk.signatures does not exist at expected path: %1").arg(targetPath);
        m_lastStatus = result.status;
        Core::Logging::Logger::warning(result.systemMessage);
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return result;
    }

    if (!targetInfo.isFile()) {
        result.status = VpkSignatureLeaseStatus::Failed;
        result.systemMessage = QStringLiteral("Target vpk.signatures is not a regular file: %1").arg(targetPath);
        m_lastStatus = result.status;
        Core::Logging::Logger::warning(result.systemMessage);
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return result;
    }

    // If we already hold a lease on the exact same path, keep it active
    if (m_lease.isHeld() && m_lease.filePath() == QDir::toNativeSeparators(targetInfo.absoluteFilePath())) {
        result.status = VpkSignatureLeaseStatus::Acquired;
        m_lastStatus = result.status;
        return result;
    }

    // Release previous lease before acquiring new one
    releaseLease();

    const Core::FileSystem::FileLeaseResult leaseRes = m_lease.acquireExclusive(targetPath);
    if (!leaseRes.isSuccess()) {
        switch (leaseRes.error) {
        case Core::FileSystem::FileLeaseError::AlreadyInUse:
            result.status = VpkSignatureLeaseStatus::AlreadyInUse;
            break;
        case Core::FileSystem::FileLeaseError::AccessDenied:
            result.status = VpkSignatureLeaseStatus::AccessDenied;
            break;
        case Core::FileSystem::FileLeaseError::NotFound:
            result.status = VpkSignatureLeaseStatus::NotFound;
            break;
        default:
            result.status = VpkSignatureLeaseStatus::Failed;
            break;
        }

        result.systemMessage = leaseRes.message;
        m_lastStatus = result.status;
        Core::Logging::Logger::error(QStringLiteral("Failed to acquire exclusive lease for vpk.signatures at '%1': %2")
            .arg(targetPath)
            .arg(leaseRes.message));
        emit leaseStateChanged(false, QString());
        emit leaseStatusChanged(result.status, targetPath, result.systemMessage);
        return result;
    }

    result.status = VpkSignatureLeaseStatus::Acquired;
    m_lastStatus = result.status;
    Core::Logging::Logger::info(QStringLiteral("Acquired exclusive file lease on %1").arg(m_lease.filePath()));

    emit leaseStateChanged(true, m_lease.filePath());
    emit leaseStatusChanged(result.status, m_lease.filePath(), QString());
    return result;
}

void VpkSignatureLeaseService::releaseLease() noexcept
{
    if (m_lease.isHeld()) {
        const QString prevPath = m_lease.filePath();
        m_lease.release();
        m_lastStatus = VpkSignatureLeaseStatus::Inactive;
        Core::Logging::Logger::info(QStringLiteral("Released exclusive file lease on %1").arg(prevPath));
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
