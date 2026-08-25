#pragma once

#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Environment/SteamService.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include <functional>
#include <optional>
#include <vector>
#include <QObject>
#include <QStringList>

namespace Application::Environment {

struct DetectionResult {
    std::vector<GameInstallation> installations;
    QStringList warnings;
};

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

    // Synchronous environment detection returning both installations and any non-fatal scan warnings
    static DetectionResult detectEnvironment(
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Detect all supported Source and Source 2 games across all detected Steam libraries
    static std::vector<GameInstallation> detectAllGames(
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Detect a specific game in Steam libraries
    static std::optional<GameInstallation> detectGame(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Convenience delegates to GameInstallationValidator
    static std::optional<GameInstallation> validateGameDirectory(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory)
    {
        return GameInstallationValidator::validateGameDirectory(type, directory);
    }

    static std::optional<GameInstallation> validateSource2(
        const Core::Path::FilesystemPath& directory,
        Domain::Game::GameType type = Domain::Game::GameType::Unknown)
    {
        return GameInstallationValidator::validateSource2(directory, type);
    }

    static std::optional<GameInstallation> validateSource1(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory)
    {
        return GameInstallationValidator::validateSource1(type, directory);
    }

    static std::optional<GameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& gameInfoPath)
    {
        return GameInstallationValidator::inspectGameInfo(gameInfoPath);
    }
};

} // namespace Application::Environment
