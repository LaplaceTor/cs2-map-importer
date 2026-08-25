#pragma once

#include "Application/Environment/GameInstallation.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/FilesystemPath.h"
#include <optional>

namespace Application::Environment {

/**
 * @brief Application service responsible for coordinating domain game validation
 * and constructing Application GameInstallation models.
 */
class GameInstallationValidator {
public:
    // Validates a Source 1 game installation directory against expected GameType
    static std::optional<GameInstallation> validateSource1(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory);

    // Validates a Source 2 game installation directory or gameinfo.gi file
    static std::optional<GameInstallation> validateSource2(
        const Core::Path::FilesystemPath& directory,
        Domain::Game::GameType type = Domain::Game::GameType::Unknown);

    // Inspects an arbitrary gameinfo.txt or gameinfo.gi file/directory and creates a GameInstallation
    static std::optional<GameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& gameInfoPath);

    // Generic entry point that validates a directory according to GameType
    static std::optional<GameInstallation> validateGameDirectory(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory);

    // Constructs a GameInstallation value object from a Domain ResolvedGameInstallation
    static std::optional<GameInstallation> createInstallationFromResolved(
        const Domain::Game::ResolvedGameInstallation& resolved);

    // Constructs a GameInstallation value object from parsed GameInfo and baseDir
    static std::optional<GameInstallation> createInstallationFromGameInfo(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& baseDir,
        const Domain::Game::GameInfo& info);
};

} // namespace Application::Environment
