#pragma once

#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Async/TaskResult.h"
#include <QStringList>

namespace Domain::Game {

struct ResolvedGameInstallation {
    GameType type = GameType::Unknown;
    Core::Path::FilesystemPath baseDirectory;
    Core::Path::FilesystemPath gameInfoPath;
    GameInfo gameInfo;
    bool isValid = false;
    bool isSource2 = false;

    // Source 1 / Source 2 layout helpers
    QString modName() const;
    Core::Path::FilesystemPath contentDirectory() const;
    Core::Path::FilesystemPath modDirectory() const;
    Core::Path::FilesystemPath addonGameDirectory(const QString& addonName = QString()) const;
    Core::Path::FilesystemPath addonContentDirectory(const QString& addonName = QString()) const;
};

/**
 * @brief Domain service encapsulating Valve / Source 1 & 2 filesystem layout rules,
 * heuristics for locating gameinfo files, directory structure resolution, and addon discovery.
 */
class GameInstallationResolver {
public:
    // Resolves a Source 1 game directory against an expected GameType
    static Core::Async::TaskResult<ResolvedGameInstallation> resolveSource1(
        GameType type,
        const Core::Path::FilesystemPath& directory);

    // Resolves a Source 2 game directory or gameinfo.gi file using Source 2 layout heuristics
    static Core::Async::TaskResult<ResolvedGameInstallation> resolveSource2(
        const Core::Path::FilesystemPath& directory,
        GameType type = GameType::Unknown);

    // Inspects an arbitrary gameinfo.txt or gameinfo.gi file/directory and resolves the installation
    static Core::Async::TaskResult<ResolvedGameInstallation> inspectGameInfo(
        const Core::Path::FilesystemPath& path);

    // Generic entry point resolving a directory according to GameType
    static Core::Async::TaskResult<ResolvedGameInstallation> resolveGameDirectory(
        GameType type,
        const Core::Path::FilesystemPath& directory);

    // Helper constructing a ResolvedGameInstallation from parsed GameInfo and base directory
    static Core::Async::TaskResult<ResolvedGameInstallation> createResolved(
        GameType type,
        const Core::Path::FilesystemPath& baseDir,
        const GameInfo& info);

    // Lists all addons in a Source 2 installation directory
    static QStringList listSource2Addons(const Core::Path::FilesystemPath& s2BasePath);
};

} // namespace Domain::Game
