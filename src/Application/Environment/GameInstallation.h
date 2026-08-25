#pragma once

#include "Domain/Game/GameType.h"
#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameRegistry.h"
#include "Core/Path/FilesystemPath.h"
#include <QDir>
#include <QString>

namespace Application::Environment {

/**
 * @brief Plain UI-facing Application Contract DTO.
 *
 * Exposes strings and primitives so UI ViewModels never need to depend on or inspect
 * Domain/Core implementation types like FilesystemPath, GameType, or GameInfo.
 */
struct GameInstallationInfo {
    QString gameId;
    QString displayName;
    QString gameTitle;
    QString basePath;
    QString gameInfoPath;
    bool isValid = false;
    bool isSource2 = false;
};

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

    // Source 2 layout helpers
    QString modName() const {
        const auto* def = Domain::Game::GameRegistry::findByType(m_type);
        if (def && m_type != Domain::Game::GameType::Custom && m_type != Domain::Game::GameType::Unknown && !def->modName().isEmpty()) {
            return def->modName();
        }
        if (m_gameInfoPath.isValid()) {
            return m_gameInfoPath.parentPath().fileName();
        }
        return QString();
    }

    Core::Path::FilesystemPath contentDirectory() const {
        if (!m_baseDirectory.isValid()) return Core::Path::FilesystemPath();
        const QString name = modName();
        if (name.isEmpty()) return Core::Path::FilesystemPath();
        return Core::Path::FilesystemPath(QDir(m_baseDirectory.toString()).filePath(
            m_isSource2 ? (QStringLiteral("content/") + name) : name));
    }

    Core::Path::FilesystemPath modDirectory() const {
        if (m_gameInfoPath.isValid()) {
            return m_gameInfoPath.parentPath();
        }
        if (!m_baseDirectory.isValid()) return Core::Path::FilesystemPath();
        const auto* def = Domain::Game::GameRegistry::findByType(m_type);
        if (def && !def->modSubdirectory.isEmpty()) {
            return Core::Path::FilesystemPath(QDir(m_baseDirectory.toString()).filePath(def->modSubdirectory));
        }
        return Core::Path::FilesystemPath();
    }

    Core::Path::FilesystemPath addonGameDirectory(const QString& addonName = QString()) const {
        if (!m_baseDirectory.isValid() || !m_isSource2) return Core::Path::FilesystemPath();
        const QString name = modName();
        if (name.isEmpty()) return Core::Path::FilesystemPath();
        QString rel = QStringLiteral("game/") + name + QStringLiteral("_addons");
        if (!addonName.isEmpty()) {
            rel += QLatin1Char('/') + addonName;
        }
        return Core::Path::FilesystemPath(QDir(m_baseDirectory.toString()).filePath(rel));
    }

    Core::Path::FilesystemPath addonContentDirectory(const QString& addonName = QString()) const {
        if (!m_baseDirectory.isValid() || !m_isSource2) return Core::Path::FilesystemPath();
        const QString name = modName();
        if (name.isEmpty()) return Core::Path::FilesystemPath();
        QString rel = QStringLiteral("content/") + name + QStringLiteral("_addons");
        if (!addonName.isEmpty()) {
            rel += QLatin1Char('/') + addonName;
        }
        return Core::Path::FilesystemPath(QDir(m_baseDirectory.toString()).filePath(rel));
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

