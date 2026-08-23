#pragma once

#include "Domain/Game/GameType.h"
#include <QString>
#include <vector>

namespace Domain::Game {

struct GameDefinition {
    GameType type = GameType::Unknown;
    EngineType engine = EngineType::Source1;
    QString id;
    QString displayName;
    int primaryAppId = 0;
    std::vector<int> allAppIds;
    QString defaultFolderName;
    QString modSubdirectory;
    QString gameInfoFileName = QStringLiteral("gameinfo.txt");
    QString expectedGameTitle;

    bool isSource2() const noexcept {
        return engine == EngineType::Source2;
    }
};

} // namespace Domain::Game

