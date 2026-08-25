#pragma once

#include "Domain/Game/SearchTarget.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <vector>
#include <utility>

namespace Domain::Game {

class GameInfo {
public:
    GameInfo() = default;

    const QString& game() const noexcept { return m_game; }
    void setGame(QString game) { m_game = std::move(game); }

    const QString& title() const noexcept { return m_title; }
    void setTitle(QString title) { m_title = std::move(title); }

    int steamAppId() const noexcept { return m_steamAppId; }
    void setSteamAppId(int appId) noexcept { m_steamAppId = appId; }

    int toolsAppId() const noexcept { return m_toolsAppId; }
    void setToolsAppId(int appId) noexcept { m_toolsAppId = appId; }

    const Core::Path::FilesystemPath& gameInfoPath() const noexcept { return m_gameInfoPath; }
    void setGameInfoPath(Core::Path::FilesystemPath path) { m_gameInfoPath = std::move(path); }

    const Core::Path::FilesystemPath& modDirectory() const noexcept { return m_modDirectory; }
    void setModDirectory(Core::Path::FilesystemPath path) { m_modDirectory = std::move(path); }

    const Core::Path::FilesystemPath& baseDirectory() const noexcept { return m_baseDirectory; }
    void setBaseDirectory(Core::Path::FilesystemPath path) { m_baseDirectory = std::move(path); }

    const std::vector<SearchTarget>& searchTargets() const noexcept { return m_searchTargets; }
    std::vector<SearchTarget>& searchTargets() noexcept { return m_searchTargets; }
    void setSearchTargets(std::vector<SearchTarget> targets) { m_searchTargets = std::move(targets); }

    const Core::KeyValues::KeyValuesDocument& document() const noexcept { return m_document; }
    Core::KeyValues::KeyValuesDocument& document() noexcept { return m_document; }
    void setDocument(Core::KeyValues::KeyValuesDocument doc) { m_document = std::move(doc); }

private:
    QString m_game;
    QString m_title;
    int m_steamAppId = 0;
    int m_toolsAppId = 0;
    Core::Path::FilesystemPath m_gameInfoPath;
    Core::Path::FilesystemPath m_modDirectory;
    Core::Path::FilesystemPath m_baseDirectory;
    std::vector<SearchTarget> m_searchTargets;
    Core::KeyValues::KeyValuesDocument m_document;
};

} // namespace Domain::Game

