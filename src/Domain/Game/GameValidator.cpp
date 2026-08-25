#include "Domain/Game/GameValidator.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInfoParser.h"
#include <algorithm>
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

    const QString actualGame = info.game().trimmed();
    const QString actualTitle = info.title().trimmed();
    const QString& expectedTitle = def->expectedGameTitle;

    // 1. Exact game / title match
    if (!expectedTitle.isEmpty()) {
        if (actualGame.compare(expectedTitle, Qt::CaseInsensitive) == 0 ||
            actualTitle.compare(expectedTitle, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    // 2. Exact aliases match
    for (const auto& alias : def->titleAliases) {
        if (!alias.isEmpty()) {
            if (actualGame.compare(alias, Qt::CaseInsensitive) == 0 ||
                actualTitle.compare(alias, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    // 3. Steam AppID match
    if (def->primaryAppId > 0 && info.steamAppId() > 0) {
        bool appIdMatched = (std::find(def->allAppIds.begin(), def->allAppIds.end(), info.steamAppId()) != def->allAppIds.end());
        if (appIdMatched) {
            if (def->primaryAppId == 730) {
                bool isGi = (info.gameInfoPath().extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
                if (def->isSource2() == isGi) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }

    // Guard: If AppID is present and belongs to another known game, reject loose matching
    if (info.steamAppId() > 0) {
        const auto* otherDef = GameRegistry::findByAppId(info.steamAppId());
        if (otherDef && otherDef->type != expectedType && !(otherDef->primaryAppId == 730 && def->primaryAppId == 730)) {
            return false;
        }
    }

    // 4. Loose substring match fallback
    auto matchesLooseString = [&](const QString& pattern) -> bool {
        if (pattern.isEmpty()) {
            return false;
        }
        if (!actualGame.contains(pattern, Qt::CaseInsensitive) &&
            !actualTitle.contains(pattern, Qt::CaseInsensitive)) {
            return false;
        }

        // Guard against prefix collision: if another game has a strictly longer matching title or alias,
        // this shorter match is shadowed.
        for (const auto& other : GameRegistry::allDefinitions()) {
            if (other.type == expectedType || other.type == GameType::Custom || other.type == GameType::Unknown) {
                continue;
            }
            if (!other.expectedGameTitle.isEmpty() &&
                other.expectedGameTitle.length() > pattern.length() &&
                (actualGame.contains(other.expectedGameTitle, Qt::CaseInsensitive) ||
                 actualTitle.contains(other.expectedGameTitle, Qt::CaseInsensitive))) {
                return false;
            }
            for (const auto& otherAlias : other.titleAliases) {
                if (!otherAlias.isEmpty() &&
                    otherAlias.length() > pattern.length() &&
                    (actualGame.contains(otherAlias, Qt::CaseInsensitive) ||
                     actualTitle.contains(otherAlias, Qt::CaseInsensitive))) {
                    return false;
                }
            }
        }
        return true;
    };

    if (matchesLooseString(expectedTitle)) {
        return true;
    }
    for (const auto& alias : def->titleAliases) {
        if (matchesLooseString(alias)) {
            return true;
        }
    }

    return false;
}

std::optional<GameType> GameValidator::identifyGameType(const GameInfo& info) {
    const auto& defs = GameRegistry::allDefinitions();
    const QString actualGame = info.game().trimmed();
    const QString actualTitle = info.title().trimmed();

    // 1. Exact game / title match across all definitions
    for (const auto& def : defs) {
        if (def.type == GameType::Unknown || def.type == GameType::Custom) {
            continue;
        }
        if (!def.expectedGameTitle.isEmpty()) {
            if (actualGame.compare(def.expectedGameTitle, Qt::CaseInsensitive) == 0 ||
                actualTitle.compare(def.expectedGameTitle, Qt::CaseInsensitive) == 0) {
                return def.type;
            }
        }
    }

    // 2. Exact aliases match across all definitions
    for (const auto& def : defs) {
        if (def.type == GameType::Unknown || def.type == GameType::Custom) {
            continue;
        }
        for (const auto& alias : def.titleAliases) {
            if (!alias.isEmpty()) {
                if (actualGame.compare(alias, Qt::CaseInsensitive) == 0 ||
                    actualTitle.compare(alias, Qt::CaseInsensitive) == 0) {
                    return def.type;
                }
            }
        }
    }

    // 3. Steam AppID match across all definitions
    if (info.steamAppId() > 0) {
        auto matchingDefs = GameRegistry::findAllByAppId(info.steamAppId());
        if (matchingDefs.size() == 1) {
            return matchingDefs.front()->type;
        }
        if (matchingDefs.size() > 1) {
            bool isGi = (info.gameInfoPath().extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
            for (const auto* def : matchingDefs) {
                if (def->isSource2() == isGi) {
                    return def->type;
                }
            }
            return matchingDefs.front()->type;
        }
    }

    // 4. Loose substring match (longest matching pattern wins)
    struct SubstringCandidate {
        GameType type;
        int matchLength;
    };
    std::vector<SubstringCandidate> candidates;

    for (const auto& def : defs) {
        if (def.type == GameType::Unknown || def.type == GameType::Custom) {
            continue;
        }
        int maxLen = 0;
        if (!def.expectedGameTitle.isEmpty() &&
            (actualGame.contains(def.expectedGameTitle, Qt::CaseInsensitive) ||
             actualTitle.contains(def.expectedGameTitle, Qt::CaseInsensitive))) {
            maxLen = std::max(maxLen, static_cast<int>(def.expectedGameTitle.length()));
        }
        for (const auto& alias : def.titleAliases) {
            if (!alias.isEmpty() &&
                (actualGame.contains(alias, Qt::CaseInsensitive) ||
                 actualTitle.contains(alias, Qt::CaseInsensitive))) {
                maxLen = std::max(maxLen, static_cast<int>(alias.length()));
            }
        }
        if (maxLen > 0) {
            candidates.push_back({def.type, maxLen});
        }
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const SubstringCandidate& a, const SubstringCandidate& b) {
            return a.matchLength > b.matchLength;
        });
        return candidates.front().type;
    }

    // 5. Custom fallback
    if (!info.game().isEmpty() || !info.title().isEmpty() || info.gameInfoPath().exists()) {
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

