#include "Domain/Game/GameValidator.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInfoParser.h"
#include <QDir>
#include <QFileInfo>

namespace Domain::Game {

bool GameValidator::validateGameInfo(const GameInfo& info, GameType expectedType) {
    if (expectedType == GameType::Unknown) {
        return false;
    }

    if (expectedType == GameType::Custom) {
        return !info.game().isEmpty() || !info.title().isEmpty() || info.gameInfoPath().exists();
    }

    const auto* def = GameRegistry::findByType(expectedType);
    if (!def) {
        return false;
    }

    const QString& actualGame = info.game().trimmed();
    const QString& actualTitle = info.title().trimmed();
    const QString& expectedTitle = def->expectedGameTitle;

    if (!expectedTitle.isEmpty()) {
        if (actualGame.compare(expectedTitle, Qt::CaseInsensitive) == 0 ||
            actualTitle.compare(expectedTitle, Qt::CaseInsensitive) == 0) {
            return true;
        }

        // Generic substring match: any declared expected title in GameDefinition automatically matches
        if (actualGame.contains(expectedTitle, Qt::CaseInsensitive) ||
            actualTitle.contains(expectedTitle, Qt::CaseInsensitive)) {
            return true;
        }

        // Special lenient matchers for known titles with historical differences
        if (expectedType == GameType::CSGO && actualGame.contains(QStringLiteral("Global Offensive"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::CSS && actualGame.contains(QStringLiteral("Counter-Strike"), Qt::CaseInsensitive) &&
            actualGame.contains(QStringLiteral("Source"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::HL2 && actualGame.contains(QStringLiteral("Half-Life 2"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::L4D && actualGame.compare(QStringLiteral("Left 4 Dead"), Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (expectedType == GameType::L4D2 && actualGame.contains(QStringLiteral("Left 4 Dead 2"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::Portal && actualGame.compare(QStringLiteral("Portal"), Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (expectedType == GameType::Portal2 && actualGame.contains(QStringLiteral("Portal 2"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::TF2 && actualGame.contains(QStringLiteral("Team Fortress 2"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::GMod && actualGame.contains(QStringLiteral("Garry"), Qt::CaseInsensitive)) {
            return true;
        }
        if (expectedType == GameType::BlackMesa && actualGame.contains(QStringLiteral("Black Mesa"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Secondary check: Steam AppID
    if (def->primaryAppId > 0 && info.steamAppId() > 0) {
        for (int id : def->allAppIds) {
            if (id == info.steamAppId()) {
                return true;
            }
        }
    }

    return false;
}

std::optional<GameType> GameValidator::identifyGameType(const GameInfo& info) {
    const auto& defs = GameRegistry::allDefinitions();
    for (const auto& def : defs) {
        if (def.type == GameType::Unknown || def.type == GameType::Custom) {
            continue;
        }
        if (validateGameInfo(info, def.type)) {
            return def.type;
        }
    }

    if (!info.game().isEmpty() || !info.title().isEmpty()) {
        return GameType::Custom;
    }

    return std::nullopt;
}

Core::Path::FilesystemPath GameValidator::getExpectedGameInfoPath(
    const Core::Path::FilesystemPath& gameDir,
    GameType type)
{
    const QString baseStr = gameDir.toString();
    if (baseStr.isEmpty()) {
        return Core::Path::FilesystemPath();
    }

    if (type == GameType::Custom) {
        if (gameDir.isFile()) {
            return gameDir;
        }
        Core::Path::FilesystemPath giPath(QDir(baseStr).filePath(QStringLiteral("gameinfo.gi")));
        if (giPath.exists() && giPath.isFile()) {
            return giPath;
        }
        return Core::Path::FilesystemPath(QDir(baseStr).filePath(QStringLiteral("gameinfo.txt")));
    }

    const auto* def = GameRegistry::findByType(type);
    if (!def || def->modSubdirectory.isEmpty()) {
        return Core::Path::FilesystemPath(QDir(baseStr).filePath(QStringLiteral("gameinfo.txt")));
    }

    QString relativePath = def->modSubdirectory + QLatin1Char('/') + def->gameInfoFileName;
    return Core::Path::FilesystemPath(QDir(baseStr).filePath(relativePath));
}

std::optional<GameInfo> GameValidator::validateDirectory(
    const Core::Path::FilesystemPath& gameDir,
    GameType type)
{
    if (!gameDir.isValid()) {
        return std::nullopt;
    }

    Core::Path::FilesystemPath targetGameInfoPath;

    if (type == GameType::Custom && gameDir.isFile()) {
        targetGameInfoPath = gameDir;
    } else if (gameDir.isDirectory()) {
        targetGameInfoPath = getExpectedGameInfoPath(gameDir, type);
    } else {
        return std::nullopt;
    }

    if (!targetGameInfoPath.exists() || !targetGameInfoPath.isFile()) {
        return std::nullopt;
    }

    const auto* def = GameRegistry::findByType(type);
    EngineType engine = def ? def->engine
                            : (targetGameInfoPath.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0
                                   ? EngineType::Source2
                                   : EngineType::Source1);

    QString error;
    auto optInfo = GameInfoParser::parse(targetGameInfoPath, engine, &error);
    if (!optInfo.has_value()) {
        return std::nullopt;
    }

    if (validateGameInfo(*optInfo, type)) {
        return optInfo;
    }

    return std::nullopt;
}

} // namespace Domain::Game

