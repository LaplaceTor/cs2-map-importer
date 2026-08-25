#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Environment/SteamService.h"
#include "Application/Environment/VpkSignatureLeaseService.h"
#include "Core/Path/FilesystemPath.h"

namespace Application::Environment {

/**
 * @brief Primary Application Service and Facade for Game and Steam environment operations.
 *
 * Orchestrates detection, validation, Steam interactions, and VPK leasing while
 * providing asynchronous, thread-safe APIs suitable for Presentation ViewModels.
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

    // Supported Source 1 game presentation names
    QStringList s1GameTypes() const;

    // Supported Source 2 game presentation names
    QStringList s2GameTypes() const;

    // Asynchronously detect all games across Steam libraries (off-UI-thread worker)
    void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const DetectionResult&)> callback,
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Synchronous environment detection
    DetectionResult detectEnvironment(
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Asynchronously validate a Source 1 game directory or gameinfo.txt
    void validateSource1FolderAsync(
        const QString& typeName,
        const QString& pathOrUrl,
        QObject* context,
        std::function<void(const std::optional<GameInstallation>&)> callback);

    // Synchronous Source 1 validation
    std::optional<GameInstallation> validateSource1Folder(
        const QString& typeName,
        const QString& pathOrUrl);

    // Asynchronously validate a Source 2 game directory or gameinfo.gi
    void validateSource2FolderAsync(
        const QString& pathOrUrl,
        QObject* context,
        std::function<void(const std::optional<GameInstallation>&)> callback);

    // Synchronous Source 2 validation
    std::optional<GameInstallation> validateSource2Folder(
        const QString& pathOrUrl);

    // Validates game files via Steam client
    bool validateGameInSteam(const QString& typeName);

    // Lists addons found in Source 2 installation
    QStringList listSource2Addons(const GameInstallation& s2Installation) const;

    // Cleans and normalizes user input path (handles file:// URLs, native separators)
    QString cleanPath(const QString& pathOrUrl) const;

    // VPK signature lease operations
    VpkSignatureLeaseService* vpkSignatureLeaseService() const noexcept { return m_leaseService; }
    void setVpkSignatureLeaseService(VpkSignatureLeaseService* service) noexcept;
    VpkSignatureLeaseResult updateVpkLease(const GameInstallation& s2Installation);
    VpkSignatureLeaseResult retryVpkLease();

private:
    Domain::Game::GameType resolveGameType(const QString& typeName) const;

    std::unique_ptr<VpkSignatureLeaseService> m_ownedLeaseService;
    VpkSignatureLeaseService* m_leaseService = nullptr;
};

} // namespace Application::Environment

