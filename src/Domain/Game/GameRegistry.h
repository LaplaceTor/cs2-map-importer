#pragma once

#include "Domain/Game/GameDefinition.h"
#include <QString>
#include <vector>

namespace Domain::Game {

class GameRegistry {
public:
    static const std::vector<GameDefinition>& allDefinitions();
    static const GameDefinition* findByType(GameType type);
    static const GameDefinition* findById(const QString& id);
    static const GameDefinition* findByAppId(int appId);
    static std::vector<const GameDefinition*> findAllByAppId(int appId);
    static QString gameTypeToString(GameType type);
    static GameType stringToGameType(const QString& id);
};

} // namespace Domain::Game

