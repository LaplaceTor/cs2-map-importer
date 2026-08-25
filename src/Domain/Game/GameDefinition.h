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
    std::vector<QString> titleAliases;

    bool isSource2() const noexcept {
        return engine == EngineType::Source2;
    }

    QString modName() const {
        if (modSubdirectory.isEmpty()) {
            return QString();
        }
        int lastSlash = modSubdirectory.lastIndexOf(QLatin1Char('/'));
        if (lastSlash != -1) {
            return modSubdirectory.mid(lastSlash + 1);
        }
        return modSubdirectory;
    }

    QString contentSubdirectory() const {
        if (isSource2()) {
            const QString name = modName();
            return name.isEmpty() ? QStringLiteral("content") : (QStringLiteral("content/") + name);
        }
        return modSubdirectory;
    }

    QString addonModSubdirectory() const {
        if (isSource2()) {
            const QString name = modName();
            return name.isEmpty() ? QStringLiteral("game_addons") : (QStringLiteral("game/") + name + QStringLiteral("_addons"));
        }
        return QString();
    }

    QString addonContentSubdirectory() const {
        if (isSource2()) {
            const QString name = modName();
            return name.isEmpty() ? QStringLiteral("content_addons") : (QStringLiteral("content/") + name + QStringLiteral("_addons"));
        }
        return QString();
    }
};

} // namespace Domain::Game

