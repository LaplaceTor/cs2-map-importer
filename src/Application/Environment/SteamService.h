#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include "Domain/Game/GameType.h"
#include <QString>
#include <memory>
#include <vector>

namespace Application::Environment {

struct SteamLibrary {
    Core::Path::FilesystemPath path;
    std::vector<int> installedAppIds;
};

class SteamService {
public:
    // Detect Steam installation path on the host OS (Windows Registry, or standard locations)
    static Core::Path::FilesystemPath detectSteamInstallPath(
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Detect all Steam libraries from libraryfolders.vdf (within steamPath, or auto-detected Steam path if empty)
    static std::vector<SteamLibrary> detectLibraries(
        const Core::Path::FilesystemPath& steamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Parse a libraryfolders.vdf file directly
    static std::vector<SteamLibrary> parseLibraryFolders(
        const Core::Path::FilesystemPath& libraryFoldersVdfPath,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Read installdir from appmanifest_<appId>.acf in a steamapps folder
    static QString readAppInstallDir(const Core::Path::FilesystemPath& libraryPath, int appId);

    // Read full app name from appmanifest_<appId>.acf in a steamapps folder
    static QString readAppName(const Core::Path::FilesystemPath& libraryPath, int appId);

    // Launch Steam game files validation for a specific Steam AppID
    static Core::Result<void> validateGameFiles(
        int appId,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Launch Steam game files validation for a registered GameType
    static Core::Result<void> validateGameFiles(
        Domain::Game::GameType type,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);
};

} // namespace Application::Environment
