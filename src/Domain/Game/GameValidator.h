#pragma once

#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <optional>

namespace Domain::Game {

class GameValidator {
public:
    // Checks if parsed GameInfo matches the expected GameType
    static Core::Result<void> validateGameInfo(const GameInfo& info, GameType expectedType);

    // Determines GameType from a parsed GameInfo object if recognized, or std::nullopt if no known pattern matches
    static std::optional<GameType> tryIdentifyGameType(const GameInfo& info);

    // Computes the expected path to gameinfo.txt / gameinfo.gi for a game directory and GameType
    static Core::Path::FilesystemPath getExpectedGameInfoPath(
        const Core::Path::FilesystemPath& gameDir,
        GameType type);

    // Validates a game installation directory against a GameType and returns the parsed GameInfo on success
    static Core::Result<GameInfo> validateDirectory(
        const Core::Path::FilesystemPath& gameDir,
        GameType type);
};

} // namespace Domain::Game

