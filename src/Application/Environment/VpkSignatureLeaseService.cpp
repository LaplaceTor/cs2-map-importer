#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Logging/Logger.h"
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

bool VpkSignatureLeaseService::acquireLease(const Core::Path::FilesystemPath& cs2BasePath, QString* errorMessage)
{
    if (cs2BasePath.isEmpty() || !cs2BasePath.exists() || !cs2BasePath.isDirectory()) {
        const QString err = QStringLiteral("CS2 base directory is invalid or does not exist: %1").arg(cs2BasePath.toString());
        if (errorMessage) {
            *errorMessage = err;
        }
        Core::Logging::Logger::warning(err);
        return false;
    }

    // Target is strictly: <cs2BasePath>/game/bin/win64/vpk.signatures
    const QString targetPath = QDir(cs2BasePath.toString()).filePath(QStringLiteral("game/bin/win64/vpk.signatures"));
    const QFileInfo targetInfo(targetPath);

    if (!targetInfo.exists()) {
        const QString err = QStringLiteral("vpk.signatures does not exist at expected path: %1").arg(targetPath);
        if (errorMessage) {
            *errorMessage = err;
        }
        Core::Logging::Logger::warning(err);
        return false;
    }

    if (!targetInfo.isFile()) {
        const QString err = QStringLiteral("Target vpk.signatures is not a regular file: %1").arg(targetPath);
        if (errorMessage) {
            *errorMessage = err;
        }
        Core::Logging::Logger::warning(err);
        return false;
    }

    // If we already hold a lease on the exact same path, keep it active
    if (m_lease.isHeld() && m_lease.filePath() == QDir::toNativeSeparators(targetInfo.absoluteFilePath())) {
        return true;
    }

    // Release previous lease before acquiring new one
    releaseLease();

    QString leaseErr;
    if (!m_lease.acquireExclusive(targetPath, &leaseErr)) {
        const QString diagnosticMsg = QStringLiteral("Failed to acquire exclusive lease for vpk.signatures at '%1': %2")
            .arg(targetPath)
            .arg(leaseErr);
        if (errorMessage) {
            *errorMessage = diagnosticMsg;
        }
        Core::Logging::Logger::error(diagnosticMsg);
        emit leaseStateChanged(false, QString());
        return false;
    }

    Core::Logging::Logger::info(QStringLiteral("Acquired exclusive file lease on %1").arg(m_lease.filePath()));

    emit leaseStateChanged(true, m_lease.filePath());
    return true;
}

void VpkSignatureLeaseService::releaseLease() noexcept
{
    if (m_lease.isHeld()) {
        const QString prevPath = m_lease.filePath();
        m_lease.release();
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

