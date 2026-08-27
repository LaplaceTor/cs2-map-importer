#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include <QString>
#include <memory>
#include <vector>

namespace Application::Environment::Internal {

struct SteamLibrary {
    Core::Path::FilesystemPath path;
    std::vector<int> installedAppIds;
};

/**
 * @brief Internal Application infrastructure adapter responsible for Steam installation discovery,
 * library folder resolution, and ACF manifest reading.
 */
class SteamLibraryDetector {
public:
    // Detect Steam installation path on the host OS (Windows Registry, or standard locations)
    static Core::Result<Core::Path::FilesystemPath> detectSteamInstallPath(
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Detect all Steam libraries from libraryfolders.vdf (within steamPath, or auto-detected Steam path if empty)
    static Core::Result<std::vector<SteamLibrary>> detectLibraries(
        const Core::Path::FilesystemPath& steamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Parse a libraryfolders.vdf file directly
    static Core::Result<std::vector<SteamLibrary>> parseLibraryFolders(
        const Core::Path::FilesystemPath& libraryFoldersVdfPath,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Read installdir from appmanifest_<appId>.acf in a steamapps folder
    static Core::Result<QString> readAppInstallDir(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);

    // Read full app name from appmanifest_<appId>.acf in a steamapps folder
    static Core::Result<QString> readAppName(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);
};

} // namespace Application::Environment::Internal

