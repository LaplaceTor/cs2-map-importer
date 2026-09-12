#pragma once

#include "Application/Environment/GameInstallationInfo.h"
#include "Application/Environment/GameInstallation.h"
#include "Application/Async/SystemTaskLog.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <functional>
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
        std::function<void(const Core::Result<DetectionResult>&)> callback,
        const Core::Path::FilesystemPath& customSteamPath = {});

    static void detectEnvironmentAsync(
        QObject* context,
        std::function<void(const Core::Result<DetectionResult>&)> callback,
        const QString& customSteamPath);

    // Synchronous environment detection returning both installations (as UI DTOs) and any non-fatal scan warnings
    static Core::Result<DetectionResult> detectEnvironment(
        const Core::Path::FilesystemPath& customSteamPath = {},
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    static Core::Result<DetectionResult> detectEnvironment(
        const QString& customSteamPath,
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    // Synchronous single game detection in Steam libraries (Application internal model)
    static Core::Result<GameInstallation> detectGame(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& customSteamPath = {},
        const Application::Async::SystemTaskLog* logCtx = nullptr);
};

} // namespace Application::Environment
