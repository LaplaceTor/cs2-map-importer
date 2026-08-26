#include "Application/Environment/GameDetectService.h"
#include "Application/Environment/GameInstallationValidator.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Domain/Game/GameRegistry.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <functional>
#include <utility>

namespace Application::Environment {

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const DetectionResult&)> callback,
    const Core::Path::FilesystemPath& customSteamPath)
{
    Application::Async::AsyncTaskRunner::run<DetectionResult>(
        QStringLiteral("Detect Environment"),
        context,
        [customSteamPath](std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) -> DetectionResult {
            if (logCtx) {
                logCtx->info(QStringLiteral("Starting environment detection across Steam libraries..."));
            }
            DetectionResult result = detectEnvironment(customSteamPath, logCtx);
            if (logCtx) {
                logCtx->info(QStringLiteral("Environment detection completed: %1 installation(s) found, %2 warning(s)")
                    .arg(result.installations.size())
                    .arg(result.warnings.size()));
            }
            return result;
        },
        std::move(callback));
}

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const DetectionResult&)> callback,
    const QString& customSteamPath)
{
    Core::Path::FilesystemPath fsPath(customSteamPath.isEmpty() ? QString() : Core::Path::PathUtils::normalize(customSteamPath));
    detectEnvironmentAsync(context, std::move(callback), fsPath);
}

DetectionResult GameDetectService::detectEnvironment(
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
        return result;
    }

    auto isAlreadyAdded = [&](Domain::Game::GameType type) {
        QString typeStr = Domain::Game::GameRegistry::gameTypeToString(type);
        return std::any_of(result.installations.begin(), result.installations.end(), [&](const GameInstallationInfo& inst) {
            return inst.gameId == typeStr;
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

        const QString commonDir = QDir(lib.path.toString()).filePath(QStringLiteral("steamapps/common"));

        for (int appId : lib.installedAppIds) {
            auto matchingDefs = Domain::Game::GameRegistry::findAllByAppId(appId);
            if (matchingDefs.empty()) {
                continue;
            }

            QString installDirName = SteamService::readAppInstallDir(lib.path, appId);

            for (const auto* def : matchingDefs) {
                if (!def || isAlreadyAdded(def->type)) {
                    continue;
                }

                // Try directory from appmanifest
                if (!installDirName.isEmpty()) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(installDirName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type, logCtx)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir, logCtx);
                    if (validated.has_value()) {
                        if (logCtx) {
                            logCtx->info(QStringLiteral("Discovered %1 at: %2")
                                .arg(validated->displayName(), validated->baseDirectory().toString()));
                        }
                        result.installations.push_back(validated->toInfo());
                        continue;
                    }
                }

                // Try default folder name if different
                if (!def->defaultFolderName.isEmpty() && def->defaultFolderName != installDirName) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type, logCtx)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir, logCtx);
                    if (validated.has_value()) {
                        if (logCtx) {
                            logCtx->info(QStringLiteral("Discovered %1 at default folder: %2")
                                .arg(validated->displayName(), validated->baseDirectory().toString()));
                        }
                        result.installations.push_back(validated->toInfo());
                    }
                }
            }
        }
    }

    return result;
}

DetectionResult GameDetectService::detectEnvironment(
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

        const QString commonDir = QDir(lib.path.toString()).filePath(QStringLiteral("steamapps/common"));

        for (int appId : lib.installedAppIds) {
            auto matchingDefs = Domain::Game::GameRegistry::findAllByAppId(appId);
            if (matchingDefs.empty()) {
                continue;
            }

            QString installDirName = SteamService::readAppInstallDir(lib.path, appId);

            for (const auto* def : matchingDefs) {
                if (!def || isAlreadyAdded(def->type)) {
                    continue;
                }

                if (!installDirName.isEmpty()) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(installDirName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type, logCtx)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir, logCtx);
                    if (validated.has_value()) {
                        result.push_back(std::move(*validated));
                        continue;
                    }
                }

                if (!def->defaultFolderName.isEmpty() && def->defaultFolderName != installDirName) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type, logCtx)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir, logCtx);
                    if (validated.has_value()) {
                        result.push_back(std::move(*validated));
                    }
                }
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

    const auto* def = Domain::Game::GameRegistry::findByType(type);
    if (!def) {
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

        const QString commonDir = QDir(lib.path.toString()).filePath(QStringLiteral("steamapps/common"));

        for (int appId : def->allAppIds) {
            QString installDirName = SteamService::readAppInstallDir(lib.path, appId);
            if (!installDirName.isEmpty()) {
                Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(installDirName));
                auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, type, logCtx)
                                                  : GameInstallationValidator::validateSource1(type, candidateDir, logCtx);
                if (validated.has_value()) {
                    return validated;
                }
            }
        }

        if (!def->defaultFolderName.isEmpty()) {
            Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
            auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, type, logCtx)
                                              : GameInstallationValidator::validateSource1(type, candidateDir, logCtx);
            if (validated.has_value()) {
                return validated;
            }
        }
    }

    return std::nullopt;
}

} // namespace Application::Environment
