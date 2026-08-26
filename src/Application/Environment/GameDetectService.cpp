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
    Application::Async::AsyncTaskRunner::run<Core::Async::TaskResult<DetectionResult>>(
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

std::vector<GameInstallation> GameDetectService::detectAllGames(
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    std::vector<GameInstallation> result;
    auto libraries = SteamService::detectLibraries(customSteamPath, logCtx);
    if (libraries.empty()) {
        return result;
    }

    auto isAlreadyAdded = [&](Domain::Game::GameType type) {
        return std::any_of(result.begin(), result.end(), [type](const GameInstallation& inst) {
            return inst.type() == type;
        });
    };

    for (const auto& lib : libraries) {
        if (!lib.path.isValid() || !lib.path.isDirectory()) {
            continue;
        }

        auto resolvedList = Domain::Game::SteamGameLocator::resolveGamesInLibrary(
            lib.path,
            lib.installedAppIds,
            [](const Core::Path::FilesystemPath& lPath, int appId) {
                return SteamService::readAppInstallDir(lPath, appId);
            });

        for (const auto& resolved : resolvedList) {
            if (isAlreadyAdded(resolved.type)) {
                continue;
            }
            auto optInst = GameInstallationValidator::createInstallationFromResolved(resolved);
            if (optInst.has_value()) {
                result.push_back(std::move(*optInst));
            }
        }
    }

    return result;
}

std::optional<GameInstallation> GameDetectService::detectGame(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    if (type == Domain::Game::GameType::Unknown || type == Domain::Game::GameType::Custom) {
        return std::nullopt;
    }

    auto libraries = SteamService::detectLibraries(customSteamPath, logCtx);
    if (libraries.empty()) {
        return std::nullopt;
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
            return GameInstallationValidator::createInstallationFromResolved(*optResolved);
        }
    }

    return std::nullopt;
}

} // namespace Application::Environment
