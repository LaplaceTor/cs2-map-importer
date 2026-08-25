#pragma once

#include "Application/Environment/GameInstallation.h"
#include "Application/Environment/SteamService.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameValidator.h"
#include "Domain/Game/GameInfoParser.h"
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

    // Detect all supported Source and Source 2 games across all detected Steam libraries (convenience wrapper)
    static std::vector<GameInstallation> detectAllGames(
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Detect a specific game in Steam libraries
    static std::optional<GameInstallation> detectGame(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& customSteamPath = {});

    // Validates if a user-supplied directory contains a valid installation of `type`
    static std::optional<GameInstallation> validateGameDirectory(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory);

    // Validates a Source 2 game installation directory (either a specific game type or any Source 2 game layout)
    static std::optional<GameInstallation> validateSource2(
        const Core::Path::FilesystemPath& directory,
        Domain::Game::GameType type = Domain::Game::GameType::Unknown);

    // Validates a Source 1 game installation directory
    static std::optional<GameInstallation> validateSource1(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory);

    // Inspects an arbitrary gameinfo.txt or gameinfo.gi file and creates a GameInstallation object
    static std::optional<GameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& gameInfoPath);

private:
    static std::optional<GameInstallation> createInstallationFromGameInfo(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& baseDir,
        const Domain::Game::GameInfo& info);
};

} // namespace Application::Environment

