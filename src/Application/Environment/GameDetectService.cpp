#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Domain/Game/SteamGameLocator.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameErrors.h"
#include "Core/Path/PathUtils.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace Application::Environment {

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const Core::Result<DetectionResult>&)> callback,
    const Core::Path::FilesystemPath& customSteamPath)
{
    Application::Async::AsyncTaskRunner::runTask<DetectionResult>(
        QStringLiteral("Detect Environment"),
        context,
        [customSteamPath](std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) -> Core::Result<DetectionResult> {
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
    std::function<void(const Core::Result<DetectionResult>&)> callback,
    const QString& customSteamPath)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    detectEnvironmentAsync(context, std::move(callback), fsPath);
}

Core::Result<DetectionResult> GameDetectService::detectEnvironment(
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<DetectionResult>([&]() -> Core::Result<DetectionResult> {
        DetectionResult result;
        auto libRes = SteamService::detectLibraries(customSteamPath, logCtx);
        if (!libRes.isSuccess()) {
            // 1. Explicit customSteamPath was provided but failed -> Fatal Failure
            if (customSteamPath.isValid()) {
                if (logCtx) {
                    logCtx->error(QStringLiteral("Failed to detect Steam libraries with custom path: %1 (%2)")
                        .arg(customSteamPath.toString(), libRes.message()));
                }
                return Core::Result<DetectionResult>::failure(
                    libRes.error(),
                    QStringLiteral("Invalid custom Steam path"));
            }

            // 2. Corrupted Steam configuration on host -> Fatal Failure
            if (libRes.errorCode() != Core::Error::ErrorCode::DirectoryNotFound) {
                if (logCtx) {
                    logCtx->error(QStringLiteral("Fatal Steam configuration error: %1").arg(libRes.message()));
                }
                return Core::Result<DetectionResult>::failure(
                    libRes.error(),
                    QStringLiteral("Steam configuration corrupted"));
            }

            // 3. Steam cleanly not found on host during auto-detection -> Success with warning (benign empty result)
            QString warnMsg = QStringLiteral("No Steam installation detected on this system.");
            if (logCtx) {
                logCtx->warning(warnMsg);
            }
            result.warnings.append(warnMsg);
            return Core::Result<DetectionResult>::success(std::move(result));
        }

        const auto& libraries = libRes.value();
        if (libraries.empty()) {
            QString warnMsg = QStringLiteral("No Steam libraries found.");
            if (logCtx) {
                logCtx->warning(warnMsg);
            }
            result.warnings.append(warnMsg);
            return Core::Result<DetectionResult>::success(std::move(result));
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
                    auto res = SteamService::readAppInstallDir(lPath, appId);
                    return res.isSuccess() ? res.value() : QString();
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

        return Core::Result<DetectionResult>::success(std::move(result));
    }, QStringLiteral("Environment detection failed"));
}

Core::Result<DetectionResult> GameDetectService::detectEnvironment(
    const QString& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    return detectEnvironment(fsPath, logCtx);
}

Core::Result<GameInstallation> GameDetectService::detectGame(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& customSteamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<GameInstallation>([&]() -> Core::Result<GameInstallation> {
        if (type == Domain::Game::GameType::Unknown || type == Domain::Game::GameType::Custom) {
            return Core::Result<GameInstallation>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("Cannot detect games with Unknown or Custom type in Steam libraries"));
        }

        auto libRes = SteamService::detectLibraries(customSteamPath, logCtx);
        if (!libRes.isSuccess()) {
            return Core::Result<GameInstallation>::failure(
                libRes.error(),
                QStringLiteral("Steam game detection failed"));
        }

        const auto& libraries = libRes.value();
        if (libraries.empty()) {
            return Core::Result<GameInstallation>::failure(
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
                    auto res = SteamService::readAppInstallDir(lPath, appId);
                    return res.isSuccess() ? res.value() : QString();
                });

            if (optResolved.has_value()) {
                auto optInst = GameInstallationValidator::createInstallationFromResolved(*optResolved);
                if (optInst.has_value()) {
                    return Core::Result<GameInstallation>::success(std::move(*optInst));
                }
            }
        }

        return Core::Result<GameInstallation>::failure(
            Domain::Game::GameErrors::gameInfoNotFound(
                QStringLiteral("Game not found in detected Steam libraries"),
                Domain::Game::GameRegistry::gameTypeToString(type)),
            QStringLiteral("Steam game detection failed"));
    }, QStringLiteral("Steam game detection failed"));
}

} // namespace Application::Environment
