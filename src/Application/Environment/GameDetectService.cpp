#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Domain/Game/SteamGameLocator.h"
#include "Domain/Game/GameRegistry.h"
#include "Core/Path/PathUtils.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace Application::Environment {

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const Core::Async::TaskResult<DetectionResult>&)> callback,
    const Core::Path::FilesystemPath& customSteamPath)
{
    Application::Async::AsyncTaskRunner::runTask<DetectionResult>(
        QStringLiteral("Detect Environment"),
        context,
        [customSteamPath](std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) -> Core::Async::TaskResult<DetectionResult> {
            if (logCtx) {
                logCtx->info(QStringLiteral("Starting environment detection across Steam libraries..."));
            }
            auto result = detectEnvironment(customSteamPath, logCtx);
            if (logCtx && result.isSuccess()) {
                logCtx->info(QStringLiteral("Environment detection completed: %1 installation(s) found, %2 warning(s)")
                    .arg(result.value().installations.size())
                    .arg(result.value().warnings.size()));
            }
            return result;
        },
        std::move(callback));
}

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const Core::Async::TaskResult<DetectionResult>&)> callback,
    const QString& customSteamPath)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    detectEnvironmentAsync(context, std::move(callback), fsPath);
}

Core::Async::TaskResult<DetectionResult> GameDetectService::detectEnvironment(
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    DetectionResult result;
    auto libraries = SteamService::detectLibraries(customSteamPath, logCtx);
    if (libraries.empty()) {
        QString warnMsg = QStringLiteral("No Steam libraries or installations detected.");
        if (logCtx) {
            logCtx->warning(warnMsg);
        }
        result.warnings.append(warnMsg);
        return Core::Async::TaskResult<DetectionResult>::success(std::move(result));
    }

    auto isAlreadyAdded = [&](const QString& gameId) {
        return std::any_of(result.installations.begin(), result.installations.end(), [&](const GameInstallationInfo& inst) {
            return inst.gameId == gameId;
        });
    };

    for (const auto& lib : libraries) {
        if (!lib.path.isValid() || !lib.path.isDirectory()) {
            QString warnMsg = QStringLiteral("Invalid Steam library path: %1").arg(lib.path.toString());
            if (logCtx) {
                logCtx->warning(warnMsg);
            }
            result.warnings.append(warnMsg);
            continue;
        }

        if (logCtx) {
            logCtx->debug(QStringLiteral("Scanning Steam library at: %1 (%2 apps registered)")
                .arg(lib.path.toString()).arg(lib.installedAppIds.size()));
        }

        auto resolvedList = Domain::Game::SteamGameLocator::resolveGamesInLibrary(
            lib.path,
            lib.installedAppIds,
            [](const Core::Path::FilesystemPath& lPath, int appId) {
                return SteamService::readAppInstallDir(lPath, appId);
            });

        for (const auto& resolved : resolvedList) {
            auto optInst = GameInstallationValidator::createInstallationFromResolved(resolved);
            if (!optInst.has_value()) {
                continue;
            }

            auto info = optInst->toInfo();
            if (isAlreadyAdded(info.gameId)) {
                continue;
            }

            if (logCtx) {
                logCtx->info(QStringLiteral("Discovered %1 at: %2")
                    .arg(info.displayName, info.basePath));
            }
            result.installations.push_back(std::move(info));
        }
    }

    return Core::Async::TaskResult<DetectionResult>::success(std::move(result));
}

Core::Async::TaskResult<DetectionResult> GameDetectService::detectEnvironment(
    const QString& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    return detectEnvironment(fsPath, logCtx);
}

Core::Async::TaskResult<GameInstallation> GameDetectService::detectGame(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (type == Domain::Game::GameType::Unknown || type == Domain::Game::GameType::Custom) {
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QStringLiteral("Cannot detect games with Unknown or Custom type in Steam libraries"));
    }

    auto libraries = SteamService::detectLibraries(customSteamPath, logCtx);
    if (libraries.empty()) {
        return Core::Async::TaskResult<GameInstallation>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QStringLiteral("No Steam libraries detected on this host"));
    }

    for (const auto& lib : libraries) {
        if (!lib.path.isValid() || !lib.path.isDirectory()) {
            continue;
        }

        auto optResolved = Domain::Game::SteamGameLocator::resolveGameInLibrary(
            lib.path,
            type,
            [](const Core::Path::FilesystemPath& lPath, int appId) {
                return SteamService::readAppInstallDir(lPath, appId);
            });

        if (optResolved.has_value()) {
            auto optInst = GameInstallationValidator::createInstallationFromResolved(*optResolved);
            if (optInst.has_value()) {
                return Core::Async::TaskResult<GameInstallation>::success(std::move(*optInst));
            }
        }
    }

    return Core::Async::TaskResult<GameInstallation>::failure(
        Core::Error::ErrorCode::FileNotFound,
        QStringLiteral("Game %1 not found in detected Steam libraries")
            .arg(Domain::Game::GameRegistry::gameTypeToString(type)));
}

} // namespace Application::Environment
