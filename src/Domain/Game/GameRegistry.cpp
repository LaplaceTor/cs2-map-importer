#include "Domain/Game/GameRegistry.h"
#include <algorithm>

namespace Domain::Game {

namespace {

const std::vector<GameDefinition>& buildDefinitions() {
    static const std::vector<GameDefinition> definitions = {
        {
            GameType::CS2,
            EngineType::Source2,
            QStringLiteral("cs2"),
            QStringLiteral("Counter-Strike 2"),
            730,
            {730},
            QStringLiteral("Counter-Strike Global Offensive"),
            QStringLiteral("game/csgo"),
            QStringLiteral("gameinfo.gi"),
            QStringLiteral("Counter-Strike 2")
        },
        {
            GameType::CSGO,
            EngineType::Source1,
            QStringLiteral("csgo"),
            QStringLiteral("Counter-Strike: Global Offensive"),
            730,
            {730, 4465480},
            QStringLiteral("csgo legacy"),
            QStringLiteral("csgo"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Counter-Strike: Global Offensive")
        },
        {
            GameType::CSS,
            EngineType::Source1,
            QStringLiteral("css"),
            QStringLiteral("Counter-Strike: Source"),
            240,
            {240},
            QStringLiteral("Counter-Strike Source"),
            QStringLiteral("cstrike"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Counter-Strike Source")
        },
        {
            GameType::HL2,
            EngineType::Source1,
            QStringLiteral("hl2"),
            QStringLiteral("Half-Life 2"),
            220,
            {220},
            QStringLiteral("Half-Life 2"),
            QStringLiteral("hl2"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("HALF-LIFE 2")
        },
        {
            GameType::L4D,
            EngineType::Source1,
            QStringLiteral("l4d"),
            QStringLiteral("Left 4 Dead"),
            500,
            {500},
            QStringLiteral("Left 4 Dead"),
            QStringLiteral("left4dead"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Left 4 Dead")
        },
        {
            GameType::L4D2,
            EngineType::Source1,
            QStringLiteral("l4d2"),
            QStringLiteral("Left 4 Dead 2"),
            550,
            {550},
            QStringLiteral("Left 4 Dead 2"),
            QStringLiteral("left4dead2"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Left 4 Dead 2")
        },
        {
            GameType::Portal,
            EngineType::Source1,
            QStringLiteral("portal"),
            QStringLiteral("Portal"),
            400,
            {400},
            QStringLiteral("Portal"),
            QStringLiteral("portal"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Portal")
        },
        {
            GameType::Portal2,
            EngineType::Source1,
            QStringLiteral("portal2"),
            QStringLiteral("Portal 2"),
            620,
            {620},
            QStringLiteral("Portal 2"),
            QStringLiteral("portal2"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("PORTAL 2")
        },
        {
            GameType::TF2,
            EngineType::Source1,
            QStringLiteral("tf2"),
            QStringLiteral("Team Fortress 2"),
            440,
            {440},
            QStringLiteral("Team Fortress 2"),
            QStringLiteral("tf"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Team Fortress 2")
        },
        {
            GameType::GMod,
            EngineType::Source1,
            QStringLiteral("gmod"),
            QStringLiteral("Garry's Mod"),
            4000,
            {4000},
            QStringLiteral("GarrysMod"),
            QStringLiteral("garrysmod"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Garry's Mod")
        },
        {
            GameType::BlackMesa,
            EngineType::Source1,
            QStringLiteral("blackmesa"),
            QStringLiteral("Black Mesa"),
            362890,
            {362890},
            QStringLiteral("Black Mesa"),
            QStringLiteral("bms"),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("Black Mesa")
        },
        {
            GameType::Custom,
            EngineType::Source1,
            QStringLiteral("custom"),
            QStringLiteral("Custom Game"),
            0,
            {},
            QStringLiteral(""),
            QStringLiteral(""),
            QStringLiteral("gameinfo.txt"),
            QStringLiteral("")
        }
    };
    return definitions;
}

} // namespace

const std::vector<GameDefinition>& GameRegistry::allDefinitions() {
    return buildDefinitions();
}

const GameDefinition* GameRegistry::findByType(GameType type) {
    const auto& defs = buildDefinitions();
    for (const auto& def : defs) {
        if (def.type == type) {
            return &def;
        }
    }
    return nullptr;
}

const GameDefinition* GameRegistry::findById(const QString& id) {
    const auto& defs = buildDefinitions();
    for (const auto& def : defs) {
        if (def.id.compare(id, Qt::CaseInsensitive) == 0) {
            return &def;
        }
    }
    return nullptr;
}

const GameDefinition* GameRegistry::findByAppId(int appId) {
    if (appId <= 0) return nullptr;
    const auto& defs = buildDefinitions();
    for (const auto& def : defs) {
        if (std::find(def.allAppIds.begin(), def.allAppIds.end(), appId) != def.allAppIds.end()) {
            return &def;
        }
    }
    return nullptr;
}

std::vector<const GameDefinition*> GameRegistry::findAllByAppId(int appId) {
    if (appId <= 0) return {};
    std::vector<const GameDefinition*> results;
    const auto& defs = buildDefinitions();
    for (const auto& def : defs) {
        if (std::find(def.allAppIds.begin(), def.allAppIds.end(), appId) != def.allAppIds.end()) {
            results.push_back(&def);
        }
    }
    return results;
}

QString GameRegistry::gameTypeToString(GameType type) {
    const auto* def = findByType(type);
    return def ? def->id : QStringLiteral("unknown");
}

GameType GameRegistry::stringToGameType(const QString& id) {
    const auto* def = findById(id);
    return def ? def->type : GameType::Unknown;
}

} // namespace Domain::Game

