#include "Application/Package/VpkIndexService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "Application/Async/AsyncTaskRunner.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameValidator.h"
#include "Domain/Package/VpkIndexBuilder.h"

namespace Application::Package {

VpkIndexService::VpkIndexService(QObject* parent)
    : QObject(parent) {
}

Core::Path::FilesystemPath VpkIndexService::getIndexDirectory() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return Core::Path::FilesystemPath(QDir(appDir).filePath(QStringLiteral("data/indices")));
}

Core::Path::FilesystemPath VpkIndexService::getIndexPath(const QString& gameId) {
    return getIndexDirectory() / (gameId.toLower() + QStringLiteral(".idx"));
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::getIndex(const QString& gameId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_indices.value(gameId.toLower(), nullptr);
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::getCs2Index() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cs2Index;
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::getActiveSource1Index() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeSource1Index;
}

QString VpkIndexService::activeSource1GameId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeSource1GameId;
}

Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> VpkIndexService::ensureIndexSync(
    const QString& gameId,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token) {
    const QString key = gameId.toLower();
    const Core::Path::FilesystemPath indexPath = getIndexPath(key);

    // 1. Try to load existing index file from disk
    if (indexPath.exists()) {
        auto loadRes = Domain::Package::VpkIndex::loadFromFile(indexPath);
        if (loadRes.isSuccess()) {
            auto loadedIndex = loadRes.value();

            // Verify that indexed VPKs match current vpkPaths exactly
            bool matches = (loadedIndex.vpks().size() == vpkPaths.size());
            if (matches) {
                for (std::size_t i = 0; i < vpkPaths.size(); ++i) {
                    if (loadedIndex.vpks()[i].path != vpkPaths[i]) {
                        matches = false;
                        break;
                    }
                }
            }

            // Fast size & mtime sanity check
            if (matches && loadedIndex.matchesDiskMetadata()) {
                auto shared = std::make_shared<Domain::Package::VpkIndex>(std::move(loadedIndex));
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_indices[key] = shared;
                    if (isCs2) {
                        m_cs2Index = shared;
                    }
                    if (key == m_activeSource1GameId) {
                        m_activeSource1Index = shared;
                    }
                }
                emit indexReady(key);
                return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::success(shared);
            }
        }
    }

    if (token.isCancelled()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::cancelled(
            QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
    }

    // 2. Rebuild index (file missing, modified, corrupt, or VPK list changed)
    auto buildRes = Domain::Package::VpkIndexBuilder::build(vpkPaths, isCs2, token);
    if (buildRes.isFailure()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            buildRes.error(),
            QCoreApplication::translate("VpkIndexService", "Failed to build VPK index for %1").arg(gameId));
    }

    auto newIndex = buildRes.value();

    // 3. Save to disk (<AppDir>/data/indices/<game_id>.idx)
    auto saveRes = newIndex.saveToFile(indexPath);
    if (saveRes.isFailure()) {
        // Log or proceed with in-memory instance
    }

    auto shared = std::make_shared<Domain::Package::VpkIndex>(std::move(newIndex));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_indices[key] = shared;
        if (isCs2) {
            m_cs2Index = shared;
        }
        if (key == m_activeSource1GameId) {
            m_activeSource1Index = shared;
        }
    }

    emit indexUpdated(key);
    emit indexReady(key);
    return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::success(shared);
}

void VpkIndexService::ensureIndexAsync(
    const QString& gameId,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    QObject* context,
    std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback) {
    (void)Application::Async::AsyncTaskRunner::runSystemTask<std::shared_ptr<const Domain::Package::VpkIndex>>(
        QStringLiteral("VpkIndex_") + gameId,
        context,
        [this, gameId, vpkPaths, isCs2](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Ensuring VPK index for '%1'...").arg(gameId));
            return ensureIndexSync(gameId, vpkPaths, isCs2, token);
        },
        callback);
}

Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> VpkIndexService::ensureCs2IndexFromGameInfoSync(
    const Core::Path::FilesystemPath& cs2BasePath,
    const Core::Async::CancellationToken& token) {
    if (cs2BasePath.isEmpty() || !cs2BasePath.isValid()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkIndexService", "CS2 base path is invalid or empty"));
    }

    const Core::Path::FilesystemPath giPath = cs2BasePath / QStringLiteral("game/csgo/gameinfo.gi");
    if (!giPath.exists()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VpkIndexService", "CS2 gameinfo.gi not found at %1").arg(giPath.toString()));
    }

    auto parseRes = Domain::Game::GameInfoParser::parse(giPath, Domain::Game::EngineType::Source2);
    if (parseRes.isFailure()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            parseRes.error(),
            QCoreApplication::translate("VpkIndexService", "Failed to parse CS2 gameinfo.gi"));
    }

    const auto& doc = parseRes.value().document();
    const auto& rootNode = doc.root();

    const auto* fileSystemNode = rootNode.findChild(QStringLiteral("FileSystem"));
    const auto* searchPathsNode = fileSystemNode ? fileSystemNode->findChild(QStringLiteral("SearchPaths"))
                                                 : rootNode.findChild(QStringLiteral("SearchPaths"));

    std::vector<Core::Path::FilesystemPath> vpkPaths;

    if (searchPathsNode) {
        // Collect Game entries defined in gameinfo.gi SearchPaths (e.g. csgo, csgo_imported, csgo_core, core)
        for (const auto& child : searchPathsNode->children()) {
            if (child.name().compare(QStringLiteral("Game"), Qt::CaseInsensitive) == 0) {
                const QString val = child.value().trimmed();
                if (val.isEmpty()) {
                    continue;
                }
                const Core::Path::FilesystemPath gameDir = cs2BasePath / QStringLiteral("game") / val;
                const Core::Path::FilesystemPath vpkCandidate = gameDir / QStringLiteral("pak01_dir.vpk");
                if (vpkCandidate.exists()) {
                    vpkPaths.push_back(vpkCandidate);
                }
            }
        }
    }

    // Fallback: if no Game keys matched, check default csgo and core
    if (vpkPaths.empty()) {
        const Core::Path::FilesystemPath csgoVpk = cs2BasePath / QStringLiteral("game/csgo/pak01_dir.vpk");
        if (csgoVpk.exists()) {
            vpkPaths.push_back(csgoVpk);
        }
        const Core::Path::FilesystemPath coreVpk = cs2BasePath / QStringLiteral("game/core/pak01_dir.vpk");
        if (coreVpk.exists()) {
            vpkPaths.push_back(coreVpk);
        }
    }

    return ensureIndexSync(QStringLiteral("cs2"), vpkPaths, true, token);
}

void VpkIndexService::ensureCs2IndexFromGameInfoAsync(
    const Core::Path::FilesystemPath& cs2BasePath,
    QObject* context,
    std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback) {
    (void)Application::Async::AsyncTaskRunner::runSystemTask<std::shared_ptr<const Domain::Package::VpkIndex>>(
        QStringLiteral("VpkIndex_CS2"),
        context,
        [this, cs2BasePath](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Resolving and indexing CS2 VPKs from gameinfo.gi..."));
            return ensureCs2IndexFromGameInfoSync(cs2BasePath, token);
        },
        callback);
}

Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> VpkIndexService::ensureSource1IndexFromGameInfoSync(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    const Core::Async::CancellationToken& token) {
    if (gameInfoPathOrDir.isEmpty() || !gameInfoPathOrDir.isValid()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkIndexService", "Game path is invalid or empty"));
    }

    Core::Path::FilesystemPath targetGameInfoPath;
    if (gameInfoPathOrDir.isFile()) {
        targetGameInfoPath = gameInfoPathOrDir;
    } else {
        // Try locating gameinfo.txt
        const Core::Path::FilesystemPath candidate = gameInfoPathOrDir / QStringLiteral("gameinfo.txt");
        if (candidate.exists()) {
            targetGameInfoPath = candidate;
        } else {
            // Check known game definition
            const auto* def = Domain::Game::GameRegistry::findById(gameId);
            if (def) {
                targetGameInfoPath = Domain::Game::GameValidator::getExpectedGameInfoPath(gameInfoPathOrDir, def->type);
            }
        }
    }

    if (!targetGameInfoPath.exists()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VpkIndexService", "gameinfo.txt not found at %1").arg(targetGameInfoPath.toString()));
    }

    auto parseRes = Domain::Game::GameInfoParser::parse(targetGameInfoPath, Domain::Game::EngineType::Source1);
    if (parseRes.isFailure()) {
        return Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>::failure(
            parseRes.error(),
            QCoreApplication::translate("VpkIndexService", "Failed to parse gameinfo.txt"));
    }

    const auto& searchTargets = parseRes.value().searchTargets();
    std::vector<Core::Path::FilesystemPath> vpkPaths;

    for (const auto& target : searchTargets) {
        if (target.isVpk() && target.path().exists()) {
            vpkPaths.push_back(target.path());
        } else if (target.isDirectory()) {
            const Core::Path::FilesystemPath vpkCandidate = target.path() / QStringLiteral("pak01_dir.vpk");
            if (vpkCandidate.exists()) {
                vpkPaths.push_back(vpkCandidate);
            }
        }
    }

    return ensureIndexSync(gameId, vpkPaths, false, token);
}

void VpkIndexService::ensureSource1IndexFromGameInfoAsync(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    QObject* context,
    std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback) {
    (void)Application::Async::AsyncTaskRunner::runSystemTask<std::shared_ptr<const Domain::Package::VpkIndex>>(
        QStringLiteral("VpkIndex_") + gameId,
        context,
        [this, gameId, gameInfoPathOrDir](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Resolving and indexing Source 1 VPKs for '%1'...").arg(gameId));
            return ensureSource1IndexFromGameInfoSync(gameId, gameInfoPathOrDir, token);
        },
        callback);
}

void VpkIndexService::ensureCs2IndexFromGameInfoAsync(
    const QString& cs2BasePath,
    QObject* context,
    std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback) {
    ensureCs2IndexFromGameInfoAsync(Core::Path::FilesystemPath(cs2BasePath), context, std::move(callback));
}

void VpkIndexService::ensureSource1IndexFromGameInfoAsync(
    const QString& gameId,
    const QString& gameInfoPathOrDir,
    QObject* context,
    std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback) {
    ensureSource1IndexFromGameInfoAsync(gameId, Core::Path::FilesystemPath(gameInfoPathOrDir), context, std::move(callback));
}

void VpkIndexService::setActiveSource1Game(const QString& gameId, const Core::Path::FilesystemPath& gameInfoPathOrDir) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeSource1GameId = gameId.toLower();
        m_activeSource1Index = m_indices.value(m_activeSource1GameId, nullptr);
    }

    ensureSource1IndexFromGameInfoAsync(gameId, gameInfoPathOrDir);
}

void VpkIndexService::setActiveSource1Game(const QString& gameId, const QString& gameInfoPathOrDir) {
    setActiveSource1Game(gameId, Core::Path::FilesystemPath(gameInfoPathOrDir));
}

} // namespace Application::Package
