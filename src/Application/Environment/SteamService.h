#pragma once

#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Result/Result.h"
#include "Domain/Game/GameType.h"
#include <memory>

namespace Application::Environment {

/**
 * @brief Application service responsible for user-facing Steam client operations.
 */
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
    static Core::Result<void> validateGameFilesInternal(
        int appId,
        std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx = nullptr);
};

} // namespace Application::Environment
