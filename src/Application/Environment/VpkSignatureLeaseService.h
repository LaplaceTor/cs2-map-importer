#pragma once

#include <QString>
#include <QObject>
#include "Application/Environment/GameInstallation.h"
#include "Core/FileSystem/FileLease.h"
#include "Core/Path/FilesystemPath.h"

namespace Application::Environment {

enum class VpkSignatureLeaseStatus {
    Inactive,      // Not a CS2 installation or no lease requested
    Acquired,      // Exclusive lease actively held
    AlreadyInUse,  // File is locked by another process (e.g. CS2)
    NotFound,      // vpk.signatures not found
    AccessDenied,  // Permission denied
    Failed         // Other OS failure
};

struct VpkSignatureLeaseResult {
    VpkSignatureLeaseStatus status = VpkSignatureLeaseStatus::Inactive;
    QString systemMessage;
    QString targetPath;

    bool isSuccess() const noexcept {
        return status == VpkSignatureLeaseStatus::Acquired || status == VpkSignatureLeaseStatus::Inactive;
    }
};

/**
 * @brief Application service responsible for leasing vpk.signatures exclusively.
 *
 * Encapsulates the application policy for CS2 signature file locking:
 * Automatically locates '<cs2BasePath>/game/bin/win64/vpk.signatures' when a CS2
 * installation is active, and maintains an exclusive OS file lease.
 */
class VpkSignatureLeaseService : public QObject
{
    Q_OBJECT

public:
    explicit VpkSignatureLeaseService(QObject* parent = nullptr);
    ~VpkSignatureLeaseService() override;

    VpkSignatureLeaseService(const VpkSignatureLeaseService&) = delete;
    VpkSignatureLeaseService& operator=(const VpkSignatureLeaseService&) = delete;

    /**
     * @brief Updates the active Source 2 installation policy.
     *
     * If the installation is a valid Counter-Strike 2 installation, automatically acquires
     * the exclusive lease. Otherwise, cleanly releases any existing lease.
     *
     * @param s2Installation The active Source 2 game installation.
     * @return VpkSignatureLeaseResult containing status and diagnostics.
     */
    VpkSignatureLeaseResult updateInstallation(const GameInstallation& s2Installation);

    /**
     * @brief Directly acquires an exclusive file lease for the specified CS2 base directory.
     * @param cs2BasePath Base directory of Counter-Strike 2.
     * @return VpkSignatureLeaseResult containing status and diagnostics.
     */
    VpkSignatureLeaseResult acquireLease(const Core::Path::FilesystemPath& cs2BasePath);

    /**
     * @brief Retries acquiring the lease for the currently active installation.
     */
    VpkSignatureLeaseResult retryLease();

    /**
     * @brief Releases the active file lease, making vpk.signatures accessible again.
     */
    void releaseLease() noexcept;

    /**
     * @brief Returns true if an exclusive lease is currently held on vpk.signatures.
     */
    bool isLeaseHeld() const noexcept;

    /**
     * @brief Returns the absolute path to the leased vpk.signatures file, or empty if none held.
     */
    QString leasedFilePath() const;

    /**
     * @brief Returns the last recorded lease status.
     */
    VpkSignatureLeaseStatus currentStatus() const noexcept { return m_lastStatus; }

signals:
    void leaseStateChanged(bool isHeld, const QString& filePath);
    void leaseStatusChanged(VpkSignatureLeaseStatus status, const QString& filePath, const QString& systemMessage);

private:
    VpkSignatureLeaseResult acquireLeaseInternal(const Core::Path::FilesystemPath& cs2BasePath);

    Core::FileSystem::FileLease m_lease;
    GameInstallation m_activeInstallation;
    VpkSignatureLeaseStatus m_lastStatus = VpkSignatureLeaseStatus::Inactive;
};

} // namespace Application::Environment
