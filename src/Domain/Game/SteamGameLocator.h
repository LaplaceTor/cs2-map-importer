#pragma once

#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <functional>
#include <optional>
#include <vector>

namespace Domain::Game {

struct SteamLibrary {
    Core::Path::FilesystemPath path;
    std::vector<int> installedAppIds;
};

/**
 * @brief Domain service for locating and resolving Valve game installations
 * within Steam library directory structures, including VDF/ACF manifest parsing.
 */
class SteamGameLocator {
public:
    using DiagnosticFn = std::function<void(const QString& warning)>;

    /**
     * @brief Discovers all Steam library folders from a Steam root directory.
     * Parses steamapps/libraryfolders.vdf when present, falling back to scanning
     * steamapps/appmanifest_*.acf under the Steam root if the VDF file does not exist.
     */
    static Core::Result<std::vector<SteamLibrary>> discoverLibraries(
        const Core::Path::FilesystemPath& steamRootPath);

    /**
     * @brief Parses a Steam libraryfolders.vdf file and discovers installed AppIds.
     */
    static Core::Result<std::vector<SteamLibrary>> parseLibraryFolders(
        const Core::Path::FilesystemPath& libraryFoldersVdfPath);

    /**
     * @brief Reads the 'installdir' property from steamapps/appmanifest_<appId>.acf.
     */
    static Core::Result<QString> readAppInstallDir(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);

    /**
     * @brief Reads the 'name' property from steamapps/appmanifest_<appId>.acf.
     */
    static Core::Result<QString> readAppName(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);

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
        const DiagnosticFn& diagnosticHandler = nullptr);

    /**
     * @brief Resolves a specific game type within a Steam library.
     */
    static std::optional<ResolvedGameInstallation> resolveGameInLibrary(
        const Core::Path::FilesystemPath& libraryPath,
        GameType type,
        const DiagnosticFn& diagnosticHandler = nullptr);
};

} // namespace Domain::Game
