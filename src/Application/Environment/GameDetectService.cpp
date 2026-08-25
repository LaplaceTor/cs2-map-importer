#include "Application/Environment/GameDetectService.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>
#include <QMetaObject>
#include <algorithm>

namespace Application::Environment {

std::optional<GameInstallation> GameDetectService::createInstallationFromGameInfo(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& baseDir,
    const Domain::Game::GameInfo& info)
{
    GameInstallation inst;
    inst.setType(type);
    inst.setGameId(Domain::Game::GameRegistry::gameTypeToString(type));

    const auto* def = Domain::Game::GameRegistry::findByType(type);
    if (def && type != Domain::Game::GameType::Custom && type != Domain::Game::GameType::Unknown) {
        inst.setDisplayName(def->displayName);
        inst.setSource2(def->isSource2());
        inst.setAppId(info.steamAppId() > 0 ? info.steamAppId() : def->primaryAppId);
    } else {
        inst.setDisplayName(info.game().isEmpty() ? info.title() : info.game());
        inst.setSource2(info.gameInfoPath().extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
        inst.setAppId(info.steamAppId());
    }

    inst.setGameTitle(info.game().isEmpty() ? info.title() : info.game());
    inst.setBaseDirectory(baseDir.isValid() ? baseDir : info.baseDirectory());
    inst.setGameInfoPath(info.gameInfoPath());
    inst.setValid(true);
    inst.setGameInfo(info);

    return inst;
}

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
                    auto validated = def->isSource2() ? validateSource2(candidateDir, def->type)
                                                      : validateSource1(def->type, candidateDir);
                    if (validated.has_value()) {
                        result.installations.push_back(std::move(*validated));
                        continue;
                    }
                }

                // Try default folder name if different
                if (!def->defaultFolderName.isEmpty() && def->defaultFolderName != installDirName) {
                    Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
                    auto validated = def->isSource2() ? validateSource2(candidateDir, def->type)
                                                      : validateSource1(def->type, candidateDir);
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
                auto validated = def->isSource2() ? validateSource2(candidateDir, type)
                                                  : validateSource1(type, candidateDir);
                if (validated.has_value()) {
                    return validated;
                }
            }
        }

        // 2. Fallback to default folder name
        if (!def->defaultFolderName.isEmpty()) {
            Core::Path::FilesystemPath candidateDir(QDir(commonDir).filePath(def->defaultFolderName));
            auto validated = def->isSource2() ? validateSource2(candidateDir, type)
                                              : validateSource1(type, candidateDir);
            if (validated.has_value()) {
                return validated;
            }
        }
    }

    return std::nullopt;
}

std::optional<GameInstallation> GameDetectService::validateGameDirectory(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid()) {
        return std::nullopt;
    }

    const auto* def = Domain::Game::GameRegistry::findByType(type);
    if (def && def->isSource2()) {
        return validateSource2(directory, type);
    }

    if (type == Domain::Game::GameType::Custom) {
        return inspectGameInfo(directory);
    }

    return validateSource1(type, directory);
}

std::optional<GameInstallation> GameDetectService::validateSource2(
    const Core::Path::FilesystemPath& directory,
    Domain::Game::GameType type)
{
    if (!directory.isValid()) {
        return std::nullopt;
    }

    const auto* def = Domain::Game::GameRegistry::findByType(type);
    Core::Path::FilesystemPath candidateBaseDir = directory;
    Core::Path::FilesystemPath giPath;

    if (directory.isFile()) {
        if (directory.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0) {
            giPath = directory;
            // Base dir: if <baseDir>/game/<mod>/gameinfo.gi, base is <baseDir>
            if (directory.parentPath().parentPath().fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
                candidateBaseDir = directory.parentPath().parentPath().parentPath();
            } else {
                candidateBaseDir = directory.parentPath();
            }
        } else {
            return std::nullopt;
        }
    } else if (directory.isDirectory()) {
        // 1. If a specific Source 2 definition was given, check its expected modSubdirectory and gameInfoFileName
        if (def && def->isSource2() && !def->modSubdirectory.isEmpty()) {
            giPath = Core::Path::FilesystemPath(QDir(directory.toString()).filePath(
                def->modSubdirectory + QLatin1Char('/') + def->gameInfoFileName));
        }

        // 2. If not found, check if directory itself is <baseDir>/game/<mod>
        if (!giPath.exists() || !giPath.isFile()) {
            if (directory.parentPath().fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
                Core::Path::FilesystemPath directGi(QDir(directory.toString()).filePath(QStringLiteral("gameinfo.gi")));
                if (directGi.exists() && directGi.isFile()) {
                    giPath = directGi;
                    candidateBaseDir = directory.parentPath().parentPath();
                }
            }
        }

        // 3. If not found, scan <directory>/game/*/gameinfo.gi
        if (!giPath.exists() || !giPath.isFile()) {
            QDir gameDir(QDir(directory.toString()).filePath(QStringLiteral("game")));
            if (gameDir.exists()) {
                const auto subdirs = gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateGi(gameDir.filePath(subdir + QStringLiteral("/gameinfo.gi")));
                    if (candidateGi.exists() && candidateGi.isFile()) {
                        giPath = candidateGi;
                        candidateBaseDir = directory;
                        break;
                    }
                }
            }
        }

        // 4. If not found, check root <directory>/gameinfo.gi
        if (!giPath.exists() || !giPath.isFile()) {
            Core::Path::FilesystemPath rootGi(QDir(directory.toString()).filePath(QStringLiteral("gameinfo.gi")));
            if (rootGi.exists() && rootGi.isFile()) {
                giPath = rootGi;
                candidateBaseDir = directory;
            }
        }
    }

    if (!giPath.exists() || !giPath.isFile()) {
        return std::nullopt;
    }

    QString error;
    auto optInfo = Domain::Game::GameInfoParser::parse(giPath, Domain::Game::EngineType::Source2, &error);
    if (!optInfo.has_value()) {
        return std::nullopt;
    }

    auto identifiedType = Domain::Game::GameValidator::identifyGameType(*optInfo);
    Domain::Game::GameType resolvedType = identifiedType.value_or(Domain::Game::GameType::Custom);

    if (type != Domain::Game::GameType::Unknown && type != Domain::Game::GameType::Custom) {
        if (!Domain::Game::GameValidator::validateGameInfo(*optInfo, type)) {
            return std::nullopt;
        }
        resolvedType = type;
    }

    return createInstallationFromGameInfo(resolvedType, candidateBaseDir, *optInfo);
}

std::optional<GameInstallation> GameDetectService::validateSource1(
    Domain::Game::GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid()) {
        return std::nullopt;
    }

    auto optInfo = Domain::Game::GameValidator::validateDirectory(directory, type);
    if (!optInfo.has_value()) {
        return std::nullopt;
    }

    return createInstallationFromGameInfo(type, directory, *optInfo);
}

std::optional<GameInstallation> GameDetectService::inspectGameInfo(
    const Core::Path::FilesystemPath& gameInfoPath)
{
    if (!gameInfoPath.isValid()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath actualPath = gameInfoPath;
    if (actualPath.isDirectory()) {
        // 1. Try direct gameinfo.txt then gameinfo.gi
        Core::Path::FilesystemPath txtPath(QDir(actualPath.toString()).filePath(QStringLiteral("gameinfo.txt")));
        Core::Path::FilesystemPath giPath(QDir(actualPath.toString()).filePath(QStringLiteral("gameinfo.gi")));
        if (txtPath.exists() && txtPath.isFile()) {
            actualPath = txtPath;
        } else if (giPath.exists() && giPath.isFile()) {
            actualPath = giPath;
        } else {
            // 2. Check Source 2 layout: actualPath/game/*/gameinfo.gi
            QDir gameDir(QDir(actualPath.toString()).filePath(QStringLiteral("game")));
            if (gameDir.exists()) {
                const auto subdirs = gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateGi(gameDir.filePath(subdir + QStringLiteral("/gameinfo.gi")));
                    if (candidateGi.exists() && candidateGi.isFile()) {
                        actualPath = candidateGi;
                        break;
                    }
                }
            }
            if (!actualPath.isFile()) {
                // 3. Check Source 1 layout: actualPath/*/gameinfo.txt
                QDir baseDir(actualPath.toString());
                const auto subdirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateTxt(baseDir.filePath(subdir + QStringLiteral("/gameinfo.txt")));
                    if (candidateTxt.exists() && candidateTxt.isFile()) {
                        actualPath = candidateTxt;
                        break;
                    }
                }
            }
            if (!actualPath.isFile()) {
                return std::nullopt;
            }
        }
    }

    if (!actualPath.exists() || !actualPath.isFile()) {
        return std::nullopt;
    }

    bool isGi = actualPath.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0;
    Domain::Game::EngineType engine = isGi ? Domain::Game::EngineType::Source2 : Domain::Game::EngineType::Source1;

    QString error;
    auto optInfo = Domain::Game::GameInfoParser::parse(actualPath, engine, &error);
    if (!optInfo.has_value()) {
        return std::nullopt;
    }

    auto identifiedType = Domain::Game::GameValidator::identifyGameType(*optInfo);
    Domain::Game::GameType type = identifiedType.value_or(Domain::Game::GameType::Custom);

    Core::Path::FilesystemPath baseDir = optInfo->baseDirectory();
    return createInstallationFromGameInfo(type, baseDir, *optInfo);
}

} // namespace Application::Environment

