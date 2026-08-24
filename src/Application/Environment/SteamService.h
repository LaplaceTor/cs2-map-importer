#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Domain/Game/GameType.h"
#include <QString>
#include <vector>

namespace Application::Environment {

struct SteamLibrary {
    Core::Path::FilesystemPath path;
    std::vector<int> installedAppIds;
};

class SteamService {
public:
    // Detect Steam installation path on the host OS (Windows Registry, or standard locations)
    static Core::Path::FilesystemPath detectSteamInstallPath();

    // Detect all Steam libraries from libraryfolders.vdf (within steamPath, or auto-detected Steam path if empty)
    static std::vector<SteamLibrary> detectLibraries(const Core::Path::FilesystemPath& steamPath = {});

    // Parse a libraryfolders.vdf file directly
    static std::vector<SteamLibrary> parseLibraryFolders(const Core::Path::FilesystemPath& libraryFoldersVdfPath);

    // Read installdir from appmanifest_<appId>.acf in a steamapps folder
    static QString readAppInstallDir(const Core::Path::FilesystemPath& libraryPath, int appId);

    // Read full app name from appmanifest_<appId>.acf in a steamapps folder
    static QString readAppName(const Core::Path::FilesystemPath& libraryPath, int appId);

    // Launch Steam game files validation for a specific Steam AppID
    static bool validateGameFiles(int appId);

    // Launch Steam game files validation for a registered GameType
    static bool validateGameFiles(Domain::Game::GameType type);
};

} // namespace Application::Environment

