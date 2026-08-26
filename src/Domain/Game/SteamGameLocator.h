#pragma once

#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include <functional>
#include <optional>
#include <vector>

namespace Domain::Game {

/**
 * @brief Domain service for locating and resolving Valve game installations
 * within Steam library directory structures.
 */
class SteamGameLocator {
public:
    using InstallDirReaderFn = std::function<QString(const Core::Path::FilesystemPath& libraryPath, int appId)>;

    /**
     * @brief Resolves candidate game root paths in a Steam library for an AppId.
     */
    static std::vector<Core::Path::FilesystemPath> locateCandidateDirectories(
        const Core::Path::FilesystemPath& libraryPath,
        int appId,
        const QString& appInstallDirName = QString());

    /**
     * @brief Resolves all games matching installed AppIds in a Steam library.
     */
    static std::vector<ResolvedGameInstallation> resolveGamesInLibrary(
        const Core::Path::FilesystemPath& libraryPath,
        const std::vector<int>& installedAppIds,
        const InstallDirReaderFn& installDirReader = nullptr);

    /**
     * @brief Resolves a specific game type within a Steam library.
     */
    static std::optional<ResolvedGameInstallation> resolveGameInLibrary(
        const Core::Path::FilesystemPath& libraryPath,
        GameType type,
        const InstallDirReaderFn& installDirReader = nullptr);
};

} // namespace Domain::Game

