#pragma once

#include <QString>
#include <QObject>
#include "Core/FileSystem/FileLease.h"
#include "Core/Path/FilesystemPath.h"

namespace Application::Environment {

/**
 * @brief Application service responsible for leasing vpk.signatures exclusively.
 *
 * Automatically locates '<cs2BasePath>/game/bin/win64/vpk.signatures' and maintains
 * an exclusive OS file lease to prevent external tools from accessing or modifying it.
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
     * @brief Acquires an exclusive file lease on vpk.signatures inside the specified CS2 base folder.
     *
     * Resolves the path to 'game/bin/win64/vpk.signatures', validates that it exists as a regular file,
     * and acquires an exclusive OS lease.
     *
     * @param cs2BasePath Base directory of Counter-Strike 2.
     * @param errorMessage Optional pointer to store error message if acquisition fails.
     * @return true if the lease was acquired successfully, false otherwise.
     */
    bool acquireLease(const Core::Path::FilesystemPath& cs2BasePath, QString* errorMessage = nullptr);

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

signals:
    void leaseStateChanged(bool isHeld, const QString& filePath);
    void leaseConflictOccurred(const QString& title, const QString& message);

private:
    Core::FileSystem::FileLease m_lease;
};

} // namespace Application::Environment

