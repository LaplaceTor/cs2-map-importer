#pragma once

#include "Application/Environment/GameInstallation.h"
#include "Application/Async/SystemTaskLog.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <optional>

namespace Application::Environment {

/**
 * @brief Application service responsible for coordinating domain game validation
 * and constructing Application GameInstallation models.
 */
class GameInstallationValidator {
public:
    // Validates a Source 1 game installation directory against expected GameType
    static Core::Result<GameInstallation> validateSource1(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory,
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    // Validates a Source 2 game installation directory or gameinfo.gi file
    static Core::Result<GameInstallation> validateSource2(
        const Core::Path::FilesystemPath& directory,
        Domain::Game::GameType type = Domain::Game::GameType::Unknown,
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    // Inspects an arbitrary gameinfo.txt or gameinfo.gi file/directory and creates a GameInstallation
    static Core::Result<GameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& gameInfoPath,
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    // Generic entry point that validates a directory according to GameType
    static Core::Result<GameInstallation> validateGameDirectory(
        Domain::Game::GameType type,
        const Core::Path::FilesystemPath& directory,
        const Application::Async::SystemTaskLog* logCtx = nullptr);

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
