#pragma once

#include "Application/Environment/GameInstallationInfo.h"
#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/SteamService.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include <functional>
#include <optional>
#include <vector>
#include <QObject>
#include <QString>
#include <QStringList>

namespace Application::Environment {

/**
 * @brief Application service responsible for discovering game installations across Steam libraries.
 */
class GameDetectService {
public:
    // Asynchronous environment detection dispatched on a worker thread and marshaled safely to caller's QObject context
    static void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const DetectionResult&)> callback,
        const Core::Path::FilesystemPath& customSteamPath = {});

    static void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const DetectionResult&)> callback,
        const QString& customSteamPath);

    // Synchronous environment detection returning both installations (as UI DTOs) and any non-fatal scan warnings
    static DetectionResult detectEnvironment(
        const Core::Path::FilesystemPath& customSteamPath = {});

    static DetectionResult detectEnvironment(
        const QString& customSteamPath);

    // Detect all supported Source and Source 2 games across all detected Steam libraries (Application internal model)
    static std::vector<GameInstallation> detectAllGames(
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Detect a specific game in Steam libraries (Application internal model)
    static std::optional<GameInstallation> detectGame(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& customSteamPath = {});
};

} // namespace Application::Environment
