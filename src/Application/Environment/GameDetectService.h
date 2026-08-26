#pragma once

#include "Application/Environment/GameInstallationInfo.h"
#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/SteamService.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Async/TaskResult.h"
#include <functional>
#include <memory>
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
        std::function<void(const Core::Async::TaskResult<DetectionResult>&)> callback,
        const Core::Path::FilesystemPath& customSteamPath = {});

    static void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const Core::Async::TaskResult<DetectionResult>&)> callback,
        const QString& customSteamPath);

    // Synchronous environment detection returning both installations (as UI DTOs) and any non-fatal scan warnings
    static Core::Async::TaskResult<DetectionResult> detectEnvironment(
        const Core::Path::FilesystemPath& customSteamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    static Core::Async::TaskResult<DetectionResult> detectEnvironment(
        const QString& customSteamPath,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Detect all supported Source and Source 2 games across all detected Steam libraries (Application internal model)
    static std::vector<GameInstallation> detectAllGames(
        const Core::Path::FilesystemPath& customSteamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Detect a specific game in Steam libraries (Application internal model)
    static std::optional<GameInstallation> detectGame(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& customSteamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);
};

} // namespace Application::Environment
