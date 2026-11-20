#include "Application/Package/VpkIndexService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include "Application/Async/AsyncTaskRunner.h"
#include "Core/Logging/ApplicationLogger.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Package/VpkIndex.h"
#include "Domain/Package/VpkIndexBuilder.h"

namespace Application::Package {

std::shared_ptr<VpkIndexService> VpkIndexService::create() {
    return std::make_shared<VpkIndexService>(PassKey{});
}

VpkIndexService::VpkIndexService(PassKey, QObject* /*parent*/)
    : QObject(nullptr) {
}

Core::Path::FilesystemPath VpkIndexService::indexDirectory() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return Core::Path::FilesystemPath(QDir(appDir).filePath(QStringLiteral("data/indices")));
}

Core::Path::FilesystemPath VpkIndexService::indexPath(const QString& gameId) {
    return indexDirectory() / (gameId.toLower() + QStringLiteral(".idx"));
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::index(const QString& gameId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_indices.value(gameId.toLower(), nullptr);
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::cs2Index() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cs2Index;
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::activeSource1Index() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeSource1Index;
}

QString VpkIndexService::activeSource1GameId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeSource1GameId;
}

bool VpkIndexService::hasPersistenceError(const QString& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_persistenceErrors.contains(key.toLower());
}

Core::Error::Error VpkIndexService::persistenceError(const QString& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_persistenceErrors.value(key.toLower());
}

Core::Result<VpkIndexResult> VpkIndexService::ensureIndexSync(
    const QString& gameId,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token) {
    const QString key = gameId.toLower();
    const Core::Path::FilesystemPath indexPath = VpkIndexService::indexPath(key);

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
                    m_persistenceErrors.remove(key);
                    m_indices[key] = shared;
                    if (isCs2) {
                        m_cs2Index = shared;
                    }
                    if (key == m_activeSource1GameId) {
                        m_activeSource1Index = shared;
                    }
                }
                dispatchIndexReady(key);
                return Core::Result<VpkIndexResult>::success(VpkIndexResult{std::move(shared), true, Core::Error::Error()});
            }
        }
    }

    if (token.isCancelled()) {
        return Core::Result<VpkIndexResult>::cancelled(
            QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
    }

    // 2. Rebuild index (file missing, modified, corrupt, or VPK list changed)
    auto buildRes = Domain::Package::VpkIndexBuilder::build(vpkPaths, isCs2, token);
    if (buildRes.isFailure()) {
        return Core::Result<VpkIndexResult>::failure(
            buildRes.error(),
            QCoreApplication::translate("VpkIndexService", "Failed to build VPK index for %1").arg(gameId));
    }

    auto newIndex = buildRes.value();

    // 3. Save to disk (<AppDir>/data/indices/<game_id>.idx) on a best-effort basis
    Core::Error::Error persistenceErr;
    bool persisted = true;
    auto saveRes = newIndex.saveToFile(indexPath, indexDirectory());
    if (saveRes.isFailure()) {
        persistenceErr = saveRes.error();
        persisted = false;
        Core::Logging::ApplicationLogger::warning(
            QStringLiteral("Failed to persist VPK index for '%1' to '%2': %3 (%4)")
                .arg(gameId)
                .arg(indexPath.toString())
                .arg(persistenceErr.message())
                .arg(persistenceErr.details()));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_persistenceErrors[key] = persistenceErr;
        }
        dispatchIndexPersistenceFailed(key, persistenceErr.message());
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_persistenceErrors.remove(key);
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

    dispatchIndexUpdated(key);
    dispatchIndexReady(key);
    return Core::Result<VpkIndexResult>::success(VpkIndexResult{std::move(shared), persisted, std::move(persistenceErr)});
}

void VpkIndexService::ensureIndexAsync(
    const QString& gameId,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    QObject* context,
    std::function<void(const Core::Result<VpkIndexResult>&)> callback) {
    auto self = weak_from_this().lock();
    if (!self) {
        if (callback) {
            callback(Core::Result<VpkIndexResult>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("VpkIndexService", "VpkIndexService instance must be managed by std::shared_ptr to start async operations")));
        }
        return;
    }

    (void)Application::Async::AsyncTaskRunner::runSystemTask<VpkIndexResult>(
        QStringLiteral("VpkIndex_") + gameId,
        context,
        [self, gameId, vpkPaths, isCs2](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Ensuring VPK index for '%1'...").arg(gameId));
            return self->ensureIndexSync(gameId, vpkPaths, isCs2, token);
        },
        callback);
}

Core::Result<VpkIndexResult> VpkIndexService::ensureCs2IndexFromGameInfoSync(
    const Core::Path::FilesystemPath& cs2BasePath,
    const Core::Async::CancellationToken& token) {
    auto targetsRes = Domain::Game::GameInstallationResolver::resolveCs2VpkTargets(cs2BasePath);
    if (targetsRes.isFailure()) {
        return Core::Result<VpkIndexResult>::failure(
            targetsRes.error(),
            targetsRes.message());
    }

    return ensureIndexSync(QStringLiteral("cs2"), targetsRes.value(), true, token);
}

void VpkIndexService::ensureCs2IndexFromGameInfoAsync(
    const Core::Path::FilesystemPath& cs2BasePath,
    QObject* context,
    std::function<void(const Core::Result<VpkIndexResult>&)> callback) {
    auto self = weak_from_this().lock();
    if (!self) {
        if (callback) {
            callback(Core::Result<VpkIndexResult>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("VpkIndexService", "VpkIndexService instance must be managed by std::shared_ptr to start async operations")));
        }
        return;
    }

    (void)Application::Async::AsyncTaskRunner::runSystemTask<VpkIndexResult>(
        QStringLiteral("VpkIndex_CS2"),
        context,
        [self, cs2BasePath](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Resolving and indexing CS2 VPKs from gameinfo.gi..."));
            return self->ensureCs2IndexFromGameInfoSync(cs2BasePath, token);
        },
        callback);
}

Core::Result<VpkIndexResult> VpkIndexService::ensureSource1IndexFromGameInfoSync(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    const Core::Async::CancellationToken& token) {
    auto targetsRes = Domain::Game::GameInstallationResolver::resolveSource1VpkTargets(gameId, gameInfoPathOrDir);
    if (targetsRes.isFailure()) {
        return Core::Result<VpkIndexResult>::failure(
            targetsRes.error(),
            targetsRes.message());
    }

    return ensureIndexSync(gameId, targetsRes.value(), false, token);
}

void VpkIndexService::ensureSource1IndexFromGameInfoAsync(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    QObject* context,
    std::function<void(const Core::Result<VpkIndexResult>&)> callback) {
    auto self = weak_from_this().lock();
    if (!self) {
        if (callback) {
            callback(Core::Result<VpkIndexResult>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("VpkIndexService", "VpkIndexService instance must be managed by std::shared_ptr to start async operations")));
        }
        return;
    }

    (void)Application::Async::AsyncTaskRunner::runSystemTask<VpkIndexResult>(
        QStringLiteral("VpkIndex_") + gameId,
        context,
        [self, gameId, gameInfoPathOrDir](const Application::Async::SystemTaskLog& sysLog, Core::Async::CancellationToken token) {
            sysLog.info(QStringLiteral("Resolving and indexing Source 1 VPKs for '%1'...").arg(gameId));
            return self->ensureSource1IndexFromGameInfoSync(gameId, gameInfoPathOrDir, token);
        },
        callback);
}

void VpkIndexService::ensureCs2IndexFromGameInfoAsync(
    const QString& cs2BasePath,
    QObject* context,
    std::function<void(const Core::Result<VpkIndexResult>&)> callback) {
    ensureCs2IndexFromGameInfoAsync(Core::Path::FilesystemPath(cs2BasePath), context, std::move(callback));
}

void VpkIndexService::ensureSource1IndexFromGameInfoAsync(
    const QString& gameId,
    const QString& gameInfoPathOrDir,
    QObject* context,
    std::function<void(const Core::Result<VpkIndexResult>&)> callback) {
    ensureSource1IndexFromGameInfoAsync(gameId, Core::Path::FilesystemPath(gameInfoPathOrDir), context, std::move(callback));
}

void VpkIndexService::setActiveSource1Game(const QString& gameId, const Core::Path::FilesystemPath& gameInfoPathOrDir) {
    auto self = weak_from_this().lock();
    if (!self) {
        return;
    }

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

void VpkIndexService::dispatchIndexReady(const QString& gameId) {
    if (QThread::currentThread() == thread()) {
        emit indexReady(gameId);
    } else {
        auto weakSelf = weak_from_this();
        QMetaObject::invokeMethod(this, [weakSelf, gameId]() {
            if (auto self = weakSelf.lock()) {
                emit self->indexReady(gameId);
            }
        }, Qt::QueuedConnection);
    }
}

void VpkIndexService::dispatchIndexUpdated(const QString& gameId) {
    if (QThread::currentThread() == thread()) {
        emit indexUpdated(gameId);
    } else {
        auto weakSelf = weak_from_this();
        QMetaObject::invokeMethod(this, [weakSelf, gameId]() {
            if (auto self = weakSelf.lock()) {
                emit self->indexUpdated(gameId);
            }
        }, Qt::QueuedConnection);
    }
}

void VpkIndexService::dispatchIndexPersistenceFailed(const QString& key, const QString& errorMsg) {
    if (QThread::currentThread() == thread()) {
        emit indexPersistenceFailed(key, errorMsg);
    } else {
        auto weakSelf = weak_from_this();
        QMetaObject::invokeMethod(this, [weakSelf, key, errorMsg]() {
            if (auto self = weakSelf.lock()) {
                emit self->indexPersistenceFailed(key, errorMsg);
            }
        }, Qt::QueuedConnection);
    }
}

} // namespace Application::Package
