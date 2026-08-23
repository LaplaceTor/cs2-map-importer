#pragma once

#include "Domain/Game/GameType.h"
#include "Domain/Game/GameInfo.h"
#include "Core/Path/FilesystemPath.h"
#include <QString>

namespace Application::Environment {

class GameInstallation {
public:
    GameInstallation() = default;

    Domain::Game::GameType type() const noexcept { return m_type; }
    void setType(Domain::Game::GameType type) noexcept { m_type = type; }

    const QString& gameId() const noexcept { return m_gameId; }
    void setGameId(QString id) { m_gameId = std::move(id); }

    const QString& displayName() const noexcept { return m_displayName; }
    void setDisplayName(QString name) { m_displayName = std::move(name); }

    const QString& gameTitle() const noexcept { return m_gameTitle; }
    void setGameTitle(QString title) { m_gameTitle = std::move(title); }

    int appId() const noexcept { return m_appId; }
    void setAppId(int id) noexcept { m_appId = id; }

    const Core::Path::FilesystemPath& baseDirectory() const noexcept { return m_baseDirectory; }
    void setBaseDirectory(Core::Path::FilesystemPath dir) { m_baseDirectory = std::move(dir); }

    const Core::Path::FilesystemPath& gameInfoPath() const noexcept { return m_gameInfoPath; }
    void setGameInfoPath(Core::Path::FilesystemPath path) { m_gameInfoPath = std::move(path); }

    bool isValid() const noexcept { return m_isValid; }
    void setValid(bool valid) noexcept { m_isValid = valid; }

    bool isSource2() const noexcept { return m_isSource2; }
    void setSource2(bool isS2) noexcept { m_isSource2 = isS2; }

    const Domain::Game::GameInfo& gameInfo() const noexcept { return m_gameInfo; }
    void setGameInfo(Domain::Game::GameInfo info) { m_gameInfo = std::move(info); }

private:
    Domain::Game::GameType m_type = Domain::Game::GameType::Unknown;
    QString m_gameId;
    QString m_displayName;
    QString m_gameTitle;
    int m_appId = 0;
    Core::Path::FilesystemPath m_baseDirectory;
    Core::Path::FilesystemPath m_gameInfoPath;
    bool m_isValid = false;
    bool m_isSource2 = false;
    Domain::Game::GameInfo m_gameInfo;
};

} // namespace Application::Environment

