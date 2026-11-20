#include <QCoreApplication>
#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/Internal/SteamLibraryDetector.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/Path/PathUtils.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/SteamGameLocator.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace Application::Environment {

GameInstallationInfo toInstallationInfo(const Domain::Game::ResolvedGameInstallation& resolved)
{
    GameInstallationInfo info;
    if (!resolved.isValid) {
        return info;
    }

    info.gameId = Domain::Game::GameRegistry::gameTypeToString(resolved.type);
    const QString title = resolved.gameInfo.game().isEmpty() ? resolved.gameInfo.title() : resolved.gameInfo.game();

    const auto* def = Domain::Game::GameRegistry::findByType(resolved.type);
    if (def && resolved.type != Domain::Game::GameType::Custom && resolved.type != Domain::Game::GameType::Unknown) {
        info.displayName = def->displayName;
        info.isSource2 = def->isSource2();
    } else {
        info.displayName = title;
        info.isSource2 = resolved.isSource2;
    }

    info.gameTitle = title;
    info.basePath = resolved.baseDirectory.toString();
    info.gameInfoPath = resolved.gameInfoPath.toString();
    info.gameInfoDir = resolved.gameInfoPath.parentPath().toString();
    info.isValid = true;
    return info;
}

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const Core::Result<DetectionResult>&)> callback,
    const Core::Path::FilesystemPath& customSteamPath)
{
    (void)Application::Async::AsyncTaskRunner::runSystemTask<DetectionResult>(
        QCoreApplication::translate("GameDetectService", "Detect Environment"),
        context,
        [customSteamPath](const Application::Async::SystemTaskLog& sysLog) -> Core::Result<DetectionResult> {
            sysLog.info(QStringLiteral("Scanning Steam libraries..."));
            auto result = detectEnvironment(customSteamPath, &sysLog);
            if (result.isSuccess()) {
                sysLog.info(QStringLiteral("Found %1 installation(s), %2 warning(s)")
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
    const Application::Async::SystemTaskLog* logCtx)
{
    return Core::Error::ExecutionGuard::guard<DetectionResult>([&]() -> Core::Result<DetectionResult> {
        DetectionResult result;
        auto libRes = Internal::SteamLibraryDetector::detectLibraries(customSteamPath, logCtx);
        if (!libRes.isSuccess()) {
            // 1. Explicit customSteamPath was provided:
            if (customSteamPath.isValid()) {
                if (libRes.errorCode() == Core::Error::ErrorCode::DirectoryNotFound) {
                    if (logCtx) {
                        logCtx->error(QStringLiteral("Custom Steam path not found: %1").arg(customSteamPath.toString()));
                    }
                    return Core::Result<DetectionResult>::failure(
                        libRes.error(),
                        QCoreApplication::translate("GameDetectService", "Invalid custom Steam path"));
                }
                // Custom path exists, but internal error (e.g. corrupted libraryfolders.vdf, unreadable config)
                if (logCtx) {
                    logCtx->error(QStringLiteral("Failed to detect Steam libraries with custom path: %1 (%2)")
                        .arg(customSteamPath.toString(), libRes.message()));
                }
                return Core::Result<DetectionResult>::failure(
                    libRes.error(),
                    libRes.message());
            }

            // 2. Steam cleanly not found on host during auto-detection -> Success with warning (benign empty result)
            if (libRes.errorCode() == Core::Error::ErrorCode::DirectoryNotFound) {
                QString warnMsg = QCoreApplication::translate("GameDetectService", "No Steam installation detected on this system.");
                if (logCtx) {
                    logCtx->warning(warnMsg);
                }
                result.warnings.append(warnMsg);
                return Core::Result<DetectionResult>::success(std::move(result));
            }

            // 3. Fatal Steam configuration or filesystem failure -> Return structured failure preserving original message & details
            if (logCtx) {
                logCtx->error(QStringLiteral("Fatal Steam library discovery error: %1 (%2)")
                    .arg(libRes.message(), libRes.details()));
            }
            return Core::Result<DetectionResult>::failure(
                libRes.error(),
                libRes.message());
        }

        const auto& libraries = libRes.value();
        if (libraries.empty()) {
            QString warnMsg = QCoreApplication::translate("GameDetectService", "No Steam libraries found.");
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
                QString warnMsg = QCoreApplication::translate("GameDetectService", "Invalid Steam library path: %1").arg(lib.path.toString());
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
                [&](const QString& warn) {
                    if (logCtx) {
                        logCtx->warning(warn);
                    }
                    result.warnings.append(warn);
                });

            for (const auto& resolved : resolvedList) {
                auto info = toInstallationInfo(resolved);
                if (!info.isValid || isAlreadyAdded(info.gameId)) {
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
    }, QCoreApplication::translate("GameDetectService", "Environment detection failed"));
}

Core::Result<DetectionResult> GameDetectService::detectEnvironment(
    const QString& customSteamPath,
    const Application::Async::SystemTaskLog* logCtx)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    return detectEnvironment(fsPath, logCtx);
}

} // namespace Application::Environment
