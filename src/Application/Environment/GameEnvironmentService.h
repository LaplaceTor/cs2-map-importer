#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include "Application/Environment/GameInstallationInfo.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Async/TaskResult.h"

namespace Application::Environment {

/**
 * @brief Primary Application Service and Facade for Game and Steam environment operations.
 *
 * Orchestrates detection, validation, Steam interactions, and VPK leasing while
 * providing asynchronous, thread-safe APIs using UI-friendly DTOs.
 */
class GameEnvironmentService : public QObject {
    Q_OBJECT

public:
    explicit GameEnvironmentService(
        VpkSignatureLeaseService* leaseService = nullptr,
        QObject* parent = nullptr);
    ~GameEnvironmentService() override = default;

    GameEnvironmentService(const GameEnvironmentService&) = delete;
    GameEnvironmentService& operator=(const GameEnvironmentService&) = delete;

    // Supported Source 1 game presentation names (Query)
    QStringList s1GameTypes() const;

    // Supported Source 2 game presentation names (Query)
    QStringList s2GameTypes() const;

    // Asynchronously detect all games across Steam libraries (off-UI-thread worker)
    void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const Core::Async::TaskResult<DetectionResult>&)> callback,
        const QString& customSteamPath = QString());

    // Synchronous environment detection
    Core::Async::TaskResult<DetectionResult> detectEnvironment(
        const QString& customSteamPath = QString());

    // Asynchronously validate a Source 1 game directory or gameinfo.txt
    void validateSource1FolderAsync(
        const QString& typeName,
        const QString& pathOrUrl,
        QObject* context,
        std::function<void(const Core::Async::TaskResult<GameInstallationInfo>&)> callback);

    // Synchronous Source 1 validation
    Core::Async::TaskResult<GameInstallationInfo> validateSource1Folder(
        const QString& typeName,
        const QString& pathOrUrl);

    // Asynchronously validate a Source 2 game directory or gameinfo.gi
    void validateSource2FolderAsync(
        const QString& pathOrUrl,
        QObject* context,
        std::function<void(const Core::Async::TaskResult<GameInstallationInfo>&)> callback);

    // Synchronous Source 2 validation
    Core::Async::TaskResult<GameInstallationInfo> validateSource2Folder(
        const QString& pathOrUrl);

    // Validates game files via Steam client
    Core::Async::TaskResult<void> validateGameInSteam(const QString& typeName);

    // Lists addons found in Source 2 installation (Query)
    QStringList listSource2Addons(const QString& s2BasePath) const;
    QStringList listSource2Addons(const GameInstallationInfo& s2Installation) const;

    // VPK signature lease operations encapsulated in facade
    void setVpkSignatureLeaseService(VpkSignatureLeaseService* service) noexcept;
    bool isVpkLeaseHeld() const noexcept;
    VpkSignatureLeaseStatus vpkLeaseStatus() const noexcept;
    QString leasedVpkFilePath() const;
    Core::Async::TaskResult<VpkSignatureLeaseResult> updateVpkLease(const QString& s2BasePath);
    Core::Async::TaskResult<VpkSignatureLeaseResult> updateVpkLease(const GameInstallationInfo& s2Installation);
    Core::Async::TaskResult<VpkSignatureLeaseResult> retryVpkLease();

signals:
    void vpkLeaseStateChanged(bool isHeld, const QString& filePath);
    void vpkLeaseStatusChanged(Application::Environment::VpkSignatureLeaseStatus status, const QString& filePath, const QString& systemMessage);

private:
    void hookLeaseSignals();
    QString cleanPath(const QString& pathOrUrl) const;

    std::unique_ptr<VpkSignatureLeaseService> m_ownedLeaseService;
    VpkSignatureLeaseService* m_leaseService = nullptr;
};

} // namespace Application::Environment
