#pragma once

#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Async/TaskResult.h"
#include <optional>

namespace Domain::Game {

class GameValidator {
public:
    // Checks if parsed GameInfo matches the expected GameType
    static bool validateGameInfo(const GameInfo& info, GameType expectedType);

    // Determines GameType from a parsed GameInfo object
    static std::optional<GameType> identifyGameType(const GameInfo& info);

    // Computes the expected path to gameinfo.txt / gameinfo.gi for a game directory and GameType
    static Core::Path::FilesystemPath getExpectedGameInfoPath(
        const Core::Path::FilesystemPath& gameDir,
        GameType type);

    // Validates a game installation directory against a GameType and returns the parsed GameInfo on success
    static Core::Async::TaskResult<GameInfo> validateDirectory(
        const Core::Path::FilesystemPath& gameDir,
        GameType type);
};

} // namespace Domain::Game

