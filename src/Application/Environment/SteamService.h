#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include "Domain/Game/GameType.h"
#include <QString>
#include <memory>
#include <vector>

class TestEnvironment;

namespace Application::Environment {

struct SteamLibrary {
    Core::Path::FilesystemPath path;
    std::vector<int> installedAppIds;
};

class GameDetectService;

class SteamService {
public:
    // Launch Steam game files validation for a specific Steam AppID
    static Core::Result<void> validateGameFiles(
        int appId,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    // Launch Steam game files validation for a registered GameType
    static Core::Result<void> validateGameFiles(
        Domain::Game::GameType type,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

private:
    friend class GameDetectService;
    friend class ::TestEnvironment;

    // Steam integration helpers (manifest reading, library parsing, path detection)
    static Core::Result<Core::Path::FilesystemPath> detectSteamInstallPath(
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    static Core::Result<std::vector<SteamLibrary>> detectLibraries(
        const Core::Path::FilesystemPath& steamPath = {},
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    static Core::Result<std::vector<SteamLibrary>> parseLibraryFolders(
        const Core::Path::FilesystemPath& libraryFoldersVdfPath,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);

    static Core::Result<QString> readAppInstallDir(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);

    static Core::Result<QString> readAppName(
        const Core::Path::FilesystemPath& libraryPath,
        int appId);
};

} // namespace Application::Environment
