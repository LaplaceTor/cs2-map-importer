#pragma once

#include "Application/Environment/GameInstallationInfo.h"
#include "Domain/Game/GameType.h"
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Core/Path/FilesystemPath.h"
#include <QString>

namespace Application::Environment {

/**
 * @brief Application internal representation of a game installation.
 *
 * Holds full Domain/Core metadata for lower-layer pipelines and workflows,
 * while providing .toInfo() for UI contract conversion.
 */
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

    GameInstallationInfo toInfo() const {
        GameInstallationInfo info;
        info.gameId = m_gameId;
        info.displayName = m_displayName;
        info.gameTitle = m_gameTitle;
        info.basePath = m_baseDirectory.toString();
        info.gameInfoPath = m_gameInfoPath.toString();
        info.isValid = m_isValid;
        info.isSource2 = m_isSource2;
        return info;
    }

    Domain::Game::ResolvedGameInstallation toResolved() const {
        Domain::Game::ResolvedGameInstallation res;
        res.type = m_type;
        res.baseDirectory = m_baseDirectory;
        res.gameInfoPath = m_gameInfoPath;
        res.gameInfo = m_gameInfo;
        res.isValid = m_isValid;
        res.isSource2 = m_isSource2;
        return res;
    }

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
