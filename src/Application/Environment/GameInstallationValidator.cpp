#include "Application/Environment/GameInstallationValidator.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace Application::Environment {

std::optional<GameInstallation> GameInstallationValidator::createInstallationFromGameInfo(
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

std::optional<GameInstallation> GameInstallationValidator::validateSource1(
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

std::optional<GameInstallation> GameInstallationValidator::validateSource2(
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

std::optional<GameInstallation> GameInstallationValidator::inspectGameInfo(
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

std::optional<GameInstallation> GameInstallationValidator::validateGameDirectory(
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

} // namespace Application::Environment

