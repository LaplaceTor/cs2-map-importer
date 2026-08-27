#include "Application/Environment/SteamService.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameErrors.h"
#include <QDesktopServices>
#include <QUrl>

namespace Application::Environment {

Core::Result<void> SteamService::validateGameFiles(
    int appId,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<void>([&]() -> Core::Result<void> {
        return validateGameFilesInternal(appId, logCtx);
    }, QStringLiteral("Steam validation failed"));
}

Core::Result<void> SteamService::validateGameFiles(
    Domain::Game::GameType type,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<void>([&]() -> Core::Result<void> {
        const auto* def = Domain::Game::GameRegistry::findByType(type);
        if (!def || def->primaryAppId <= 0) {
            QString typeStr = Domain::Game::GameRegistry::gameTypeToString(type);
            if (logCtx) {
                logCtx->error(QStringLiteral("No primary AppID registered for game type: %1").arg(typeStr));
            }
            return Core::Result<void>::failure(
                Domain::Game::GameErrors::unsupportedGame(
                    QStringLiteral("No primary AppID registered for game type"),
                    typeStr));
        }
        return validateGameFilesInternal(def->primaryAppId, logCtx);
    }, QStringLiteral("Steam validation failed"));
}

Core::Result<void> SteamService::validateGameFilesInternal(
    int appId,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (appId <= 0) {
        if (logCtx) {
            logCtx->error(QStringLiteral("Invalid Steam AppID: %1").arg(appId));
        }
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QStringLiteral("Invalid Steam AppID"),
            QString::number(appId));
    }
    if (logCtx) {
        logCtx->info(QStringLiteral("Requesting Steam game files validation for AppID: %1").arg(appId));
    }
    QUrl validateUrl(QStringLiteral("steam://validate/") + QString::number(appId));
    bool ok = QDesktopServices::openUrl(validateUrl);
    if (!ok) {
        if (logCtx) {
            logCtx->error(QStringLiteral("Failed to open Steam validation URL for AppID: %1").arg(appId));
        }
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Failed to open Steam validation URL"),
            validateUrl.toString());
    }
    return Core::Result<void>::success();
}

} // namespace Application::Environment
