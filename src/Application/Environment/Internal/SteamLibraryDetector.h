#pragma once

#include "Application/Async/SystemTaskLog.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Game/SteamGameLocator.h"
#include <vector>

namespace Application::Environment::Internal {

/**
 * @brief Internal Application infrastructure adapter responsible for host-OS Steam
 * installation path discovery (Windows Registry and standard fallback locations),
 * delegating VDF/ACF library parsing to Domain::Game::SteamGameLocator.
 */
class SteamLibraryDetector {
public:
    // Detect Steam installation path on the host OS (Windows Registry, or standard locations)
    static Core::Result<Core::Path::FilesystemPath> detectSteamInstallPath(
        const Application::Async::SystemTaskLog* logCtx = nullptr);

    // Detect all Steam libraries (within steamPath, or auto-detected Steam path if empty)
    static Core::Result<std::vector<Domain::Game::SteamLibrary>> detectLibraries(
        const Core::Path::FilesystemPath& steamPath = {},
        const Application::Async::SystemTaskLog* logCtx = nullptr);
};

} // namespace Application::Environment::Internal
