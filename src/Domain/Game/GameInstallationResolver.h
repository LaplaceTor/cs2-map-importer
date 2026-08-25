#pragma once

#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include <optional>

namespace Domain::Game {

struct ResolvedGameInstallation {
    GameType type = GameType::Unknown;
    Core::Path::FilesystemPath baseDirectory;
    Core::Path::FilesystemPath gameInfoPath;
    GameInfo gameInfo;
    bool isValid = false;
    bool isSource2 = false;
};

/**
 * @brief Domain service encapsulating Valve / Source 1 & 2 filesystem layout rules,
 * heuristics for locating gameinfo files, and directory structure resolution.
 */
class GameInstallationResolver {
public:
    // Resolves a Source 1 game directory against an expected GameType
    static std::optional<ResolvedGameInstallation> resolveSource1(
        GameType type,
        const Core::Path::FilesystemPath& directory);

    // Resolves a Source 2 game directory or gameinfo.gi file using Source 2 layout heuristics
    static std::optional<ResolvedGameInstallation> resolveSource2(
        const Core::Path::FilesystemPath& directory,
        GameType type = GameType::Unknown);

    // Inspects an arbitrary gameinfo.txt or gameinfo.gi file/directory and resolves the installation
    static std::optional<ResolvedGameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& path);

    // Generic entry point resolving a directory according to GameType
    static std::optional<ResolvedGameInstallation> resolveGameDirectory(
        GameType type,
        const Core::Path::FilesystemPath& directory);

    // Helper constructing a ResolvedGameInstallation from parsed GameInfo and base directory
    static std::optional<ResolvedGameInstallation> createResolved(
        GameType type,
        const Core::Path::FilesystemPath& baseDir,
        const GameInfo& info);
};

} // namespace Domain::Game

