#include "Application/Environment/GameDetectService.h"
#include "Domain/Game/GameRegistry.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <algorithm>
#include <functional>
#include <utility>

namespace Application::Environment {

void GameDetectService::detectEnvironmentAsync(
    QObject* context,
    std::function<void(const DetectionResult&)> callback,
    const Core::Path::FilesystemPath& customSteamPath)
{
    QPointer<QObject> contextGuard(context);

    QThreadPool::globalInstance()->start([contextGuard, callback = std::move(callback), customSteamPath]() {
        DetectionResult result = detectEnvironment(customSteamPath);

        if (!contextGuard) {
            return;
        }

        QMetaObject::invokeMethod(contextGuard.data(), [contextGuard, callback = std::move(callback), res = std::move(result)]() {
            if (contextGuard && callback) {
                callback(res);
            }
        }, Qt::QueuedConnection);
    });
}

DetectionResult GameDetectService::detectEnvironment(
    const Core::Path::FilesystemPath& customSteamPath)
{
    DetectionResult result;
    auto libraries = SteamService::detectLibraries(customSteamPath);
    if (libraries.empty()) {
        result.warnings.append(QStringLiteral("No Steam libraries or installations detected."));
        return result;
    }

    auto isAlreadyAdded = [&](Domain::Game::GameType type) {
        return std::any_of(result.installations.begin(), result.installations.end(), [type](const GameInstallation& inst) {
            return inst.type() == type;
        });
    };

    for (const auto& lib : libraries) {
        if (!lib.path.isValid() || !lib.path.isDirectory()) {
            result.warnings.append(QStringLiteral("Invalid Steam library path: %1").arg(lib.path.toString()));
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

                // Try directory from appmanifest
                if (!installDirName.isEmpty()) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(installDirName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir);
                    if (validated.has_value()) {
                        result.installations.push_back(std::move(*validated));
                        continue;
                    }
                }

                // Try default folder name if different
                if (!def->defaultFolderName.isEmpty() && def->defaultFolderName != installDirName) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
                    auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, def->type)
                                                      : GameInstallationValidator::validateSource1(def->type, candidateDir);
                    if (validated.has_value()) {
                        result.installations.push_back(std::move(*validated));
                    }
                }
            }
        }
    }

    return result;
}

std::vector<GameInstallation> GameDetectService::detectAllGames(
    const Core::Path::FilesystemPath& customSteamPath)
{
    return detectEnvironment(customSteamPath).installations;
}

std::optional<GameInstallation> GameDetectService::detectGame(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& customSteamPath)
{
    if (type == Domain::Game::GameType::Unknown || type == Domain::Game::GameType::Custom) {
        return std::nullopt;
    }

    const auto* def = Domain::Game::GameRegistry::findByType(type);
    if (!def) {
        return std::nullopt;
    }

    auto libraries = SteamService::detectLibraries(customSteamPath);
    if (libraries.empty()) {
        return std::nullopt;
    }

    for (const auto& lib : libraries) {
        if (!lib.path.isValid() || !lib.path.isDirectory()) {
            continue;
        }

        const QString commonDir = QDir(lib.path.toString()).filePath(QStringLiteral("steamapps/common"));

        // 1. Try reading installdir from appmanifest for each associated AppID
        for (int appId : def->allAppIds) {
            QString installDirName = SteamService::readAppInstallDir(lib.path, appId);
            if (!installDirName.isEmpty()) {
                Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(installDirName));
                auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, type)
                                                  : GameInstallationValidator::validateSource1(type, candidateDir);
                if (validated.has_value()) {
                    return validated;
                }
            }
        }

        // 2. Fallback to default folder name
        if (!def->defaultFolderName.isEmpty()) {
            Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
            auto validated = def->isSource2() ? GameInstallationValidator::validateSource2(candidateDir, type)
                                              : GameInstallationValidator::validateSource1(type, candidateDir);
            if (validated.has_value()) {
                return validated;
            }
        }
    }

    return std::nullopt;
}

} // namespace Application::Environment
