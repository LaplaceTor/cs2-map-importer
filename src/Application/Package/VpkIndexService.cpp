#include "Application/Package/VpkIndexService.h"

#include <chrono>
#include <condition_variable>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QThread>

#include "Application/Async/AsyncTaskRunner.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/Hash/Sha256.h"
#include "Core/Logging/ApplicationLogger.h"
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Package/VpkIndex.h"
#include "Domain/Package/VpkIndexBuilder.h"

namespace Application::Package {

struct VpkIndexService::InFlightArchiveLoad {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>> result;
    bool fromCache = false;
    bool persisted = true;
    Core::Error::Error persistenceError;
};

namespace {

struct FingerprintedVpk {
    Core::Path::FilesystemPath vpkPath;
    QString pathKey;
    qint64 size = 0;
    qint64 lastModified = 0;
    QString sha256;
};

void persistArchiveSnapshotBestEffort(
    const Domain::Package::VpkArchiveIndex& archiveIndex,
    const Core::Path::FilesystemPath& vpkPath,
    const QString& sha256,
    const Core::Path::FilesystemPath& archiveDir,
    bool& outPersisted,
    Core::Error::Error& outPersistenceError) {
    outPersisted = true;
    outPersistenceError = Core::Error::Error();

    const auto cachePath = archiveDir.resolveBelow(sha256 + QStringLiteral(".vpkidx"));
    if (!cachePath.has_value()) {
        outPersisted = false;
        outPersistenceError = Core::Error::Error(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkIndexService", "Failed to resolve archive index cache path"),
            sha256);
        Core::Logging::ApplicationLogger::warning(
            QStringLiteral("Failed to resolve cache path for VPK archive index '%1' (SHA-256 %2)")
                .arg(vpkPath.toString(), sha256));
        return;
    }

    auto saveRes = archiveIndex.saveToFile(*cachePath, archiveDir);
    if (saveRes.isFailure()) {
        outPersisted = false;
        outPersistenceError = saveRes.error();
        Core::Logging::ApplicationLogger::warning(
            QStringLiteral("Failed to persist VPK archive index for '%1' to '%2': %3 (%4)")
                .arg(vpkPath.toString(),
                     cachePath->toString(),
                     outPersistenceError.message(),
                     outPersistenceError.details()));
    }
}

void ensureArchivePersistedInDirectory(
    const Domain::Package::VpkArchiveIndex& archiveIndex,
    const Core::Path::FilesystemPath& vpkPath,
    const QString& sha256,
    const Core::Path::FilesystemPath& archiveDir,
    bool& outPersisted,
    Core::Error::Error& outPersistenceError) {
    const auto cachePath = archiveDir.resolveBelow(sha256 + QStringLiteral(".vpkidx"));
    if (cachePath.has_value() && cachePath->exists()) {
        return;
    }
    persistArchiveSnapshotBestEffort(archiveIndex, vpkPath, sha256, archiveDir, outPersisted, outPersistenceError);
}

Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>> loadOrBuildArchiveSnapshot(
    const Core::Path::FilesystemPath& vpkPath,
    const QString& sha256,
    const Core::Path::FilesystemPath& archiveDir,
    const Core::Async::CancellationToken& token,
    bool& outFromCache,
    bool& outPersisted,
    Core::Error::Error& outPersistenceError) {
    outFromCache = false;
    outPersisted = true;
    outPersistenceError = Core::Error::Error();

    const Core::Error::ExecutionContext ctx{
        QStringLiteral("Resolving VPK archive index"),
        sha256,
        vpkPath.toString()
    };

    return Core::Error::ExecutionGuard::guard<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>(
        [&]() -> Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>> {
            const auto cachePath = archiveDir.resolveBelow(sha256 + QStringLiteral(".vpkidx"));
            if (cachePath.has_value() && cachePath->exists()) {
                auto loadRes = Domain::Package::VpkArchiveIndex::loadFromFile(*cachePath, sha256);
                if (loadRes.isSuccess()) {
                    outFromCache = true;
                    outPersisted = true;
                    return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::success(
                        std::make_shared<const Domain::Package::VpkArchiveIndex>(std::move(loadRes.value())));
                }

                Core::Logging::ApplicationLogger::warning(
                    QStringLiteral("Failed to load cached VPK archive index '%1' (SHA-256 %2): %3 (%4); rebuilding from archive")
                        .arg(cachePath->toString(), sha256, loadRes.error().message(), loadRes.error().details()));
            }

            if (token.isCancelled()) {
                return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::cancelled(
                    QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
            }

            auto buildRes = Domain::Package::VpkIndexBuilder::buildArchive(vpkPath, sha256, token);
            if (buildRes.isCancelled()) {
                return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::cancelled(
                    buildRes.message());
            }
            if (buildRes.isFailure()) {
                Core::Logging::ApplicationLogger::warning(
                    QStringLiteral("Failed to build VPK archive index for '%1' (SHA-256 %2): %3 (%4)")
                        .arg(vpkPath.toString(), sha256, buildRes.error().message(), buildRes.error().details()));
                return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::failure(
                    buildRes.error(),
                    buildRes.message());
            }

            auto archivePtr = std::make_shared<const Domain::Package::VpkArchiveIndex>(std::move(buildRes.value()));
            outFromCache = false;
            persistArchiveSnapshotBestEffort(*archivePtr, vpkPath, sha256, archiveDir, outPersisted, outPersistenceError);

            return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::success(
                std::move(archivePtr));
        },
        ctx);
}

} // namespace

std::shared_ptr<VpkIndexService> VpkIndexService::create(Core::Path::FilesystemPath cacheRootDir) {
    return std::make_shared<VpkIndexService>(PassKey{}, std::move(cacheRootDir));
}

VpkIndexService::VpkIndexService(
    PassKey,
    Core::Path::FilesystemPath cacheRootDir,
    QObject* parent)
    : QObject(parent)
    , m_cacheRootDir(std::move(cacheRootDir)) {
}

VpkIndexService::~VpkIndexService() = default;

Core::Path::FilesystemPath VpkIndexService::indexDirectory() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return Core::Path::FilesystemPath(QDir(appDir).filePath(QStringLiteral("data/indices")));
}

Core::Path::FilesystemPath VpkIndexService::archiveIndexDirectory(const QString& gameId) {
    const QString category = Domain::Game::GameInstallationResolver::resolveIndexCategory(gameId);
    const auto resolved = indexDirectory().resolveBelow(category);
    return resolved.value_or(indexDirectory() / QStringLiteral("custom"));
}

Core::Path::FilesystemPath VpkIndexService::archiveIndexPath(const QString& gameId, const QString& sha256) {
    if (!Domain::Package::VpkArchiveIndex::isValidSha256Hex(sha256)) {
        return Core::Path::FilesystemPath();
    }
    const auto resolved = archiveIndexDirectory(gameId).resolveBelow(sha256.toLower() + QStringLiteral(".vpkidx"));
    return resolved.value_or(Core::Path::FilesystemPath());
}

Core::Path::FilesystemPath VpkIndexService::effectiveArchiveDirectory(const QString& cacheCategory) const {
    const Core::Path::FilesystemPath baseDir = m_cacheRootDir.isEmpty()
        ? indexDirectory()
        : m_cacheRootDir;
    const QString normalizedCategory = Domain::Game::GameInstallationResolver::resolveIndexCategory(cacheCategory);
    const auto resolved = baseDir.resolveBelow(normalizedCategory);
    return resolved.value_or(baseDir / QStringLiteral("custom"));
}

Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>> VpkIndexService::resolveArchiveSnapshot(
    const Core::Path::FilesystemPath& vpkPath,
    const QString& sha256,
    const Core::Path::FilesystemPath& archiveDir,
    const Core::Async::CancellationToken& token,
    bool& outReused,
    bool& outPersisted,
    Core::Error::Error& outPersistenceError) {
    outReused = false;
    outPersisted = true;
    outPersistenceError = Core::Error::Error();

    while (true) {
        if (token.isCancelled()) {
            return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::cancelled(
                QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
        }

        std::shared_ptr<const Domain::Package::VpkArchiveIndex> cachedArchive;
        std::shared_ptr<InFlightArchiveLoad> entry;
        bool isInitiator = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto cacheIt = m_archiveCache.constFind(sha256);
            if (cacheIt != m_archiveCache.constEnd()) {
                cachedArchive = cacheIt.value();
            } else {
                const auto inFlightIt = m_inFlightArchives.constFind(sha256);
                if (inFlightIt != m_inFlightArchives.constEnd()) {
                    entry = inFlightIt.value();
                    isInitiator = false;
                } else {
                    entry = std::make_shared<InFlightArchiveLoad>();
                    m_inFlightArchives.insert(sha256, entry);
                    isInitiator = true;
                }
            }
        }

        if (cachedArchive) {
            outReused = true;
            ensureArchivePersistedInDirectory(
                *cachedArchive,
                vpkPath,
                sha256,
                archiveDir,
                outPersisted,
                outPersistenceError);
            return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::success(
                std::move(cachedArchive));
        }

        if (isInitiator) {
            bool fromCache = false;
            bool loadPersisted = true;
            Core::Error::Error loadPersistErr;
            auto finalResult = loadOrBuildArchiveSnapshot(
                vpkPath,
                sha256,
                archiveDir,
                token,
                fromCache,
                loadPersisted,
                loadPersistErr);

            // Commit to global cache and remove from in-flight map before waking waiters
            // so waiters that retry on initiator cancellation never see the stale entry.
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_inFlightArchives.find(sha256);
                if (it != m_inFlightArchives.end() && it.value() == entry) {
                    m_inFlightArchives.erase(it);
                }
                if (finalResult.isSuccess() && finalResult.value()) {
                    m_archiveCache.insert(sha256, finalResult.value());
                }
            }

            {
                std::lock_guard<std::mutex> entryLock(entry->mutex);
                entry->result = finalResult;
                entry->fromCache = fromCache;
                entry->persisted = loadPersisted;
                entry->persistenceError = loadPersistErr;
                entry->done = true;
            }
            entry->cv.notify_all();

            outReused = fromCache;
            outPersisted = loadPersisted;
            outPersistenceError = std::move(loadPersistErr);
            return finalResult;
        }

        {
            std::unique_lock<std::mutex> entryLock(entry->mutex);
            while (!entry->done) {
                if (token.isCancelled()) {
                    return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::cancelled(
                        QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
                }
                entry->cv.wait_for(entryLock, std::chrono::milliseconds(50));
            }

            if (token.isCancelled()) {
                return Core::Result<std::shared_ptr<const Domain::Package::VpkArchiveIndex>>::cancelled(
                    QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
            }

            // Initiator was cancelled by its own token while waiter is still active: retry.
            if (entry->result.isCancelled()) {
                continue;
            }

            outReused = true;
            outPersisted = entry->persisted;
            outPersistenceError = entry->persistenceError;
            auto waitedResult = entry->result;
            entryLock.unlock();

            if (waitedResult.isSuccess() && waitedResult.value()) {
                ensureArchivePersistedInDirectory(
                    *waitedResult.value(),
                    vpkPath,
                    sha256,
                    archiveDir,
                    outPersisted,
                    outPersistenceError);
            }
            return waitedResult;
        }
    }
}

std::shared_ptr<const Domain::Package::VpkIndex> VpkIndexService::index(const QString& gameId) const {
    const QString key = Domain::Game::GameInstallationResolver::resolveIndexCategory(gameId);
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_indices.value(key, nullptr);
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
    const QString normalized = Domain::Game::GameInstallationResolver::resolveIndexCategory(key);
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_persistenceErrors.contains(normalized);
}

Core::Error::Error VpkIndexService::persistenceError(const QString& key) const {
    const QString normalized = Domain::Game::GameInstallationResolver::resolveIndexCategory(key);
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_persistenceErrors.value(normalized);
}

Core::Result<VpkIndexResult> VpkIndexService::ensureIndexSync(
    const QString& gameId,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token) {
    const QString category = Domain::Game::GameInstallationResolver::resolveIndexCategory(gameId);
    return ensureIndexSyncInternal(category, category, vpkPaths, isCs2, token);
}

Core::Result<VpkIndexResult> VpkIndexService::ensureIndexSyncInternal(
    const QString& logicalKey,
    const QString& cacheCategory,
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        return Core::Result<VpkIndexResult>::cancelled(
            QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
    }

    const QString key = cacheCategory.isEmpty()
        ? Domain::Game::GameInstallationResolver::resolveIndexCategory(logicalKey)
        : cacheCategory;

    // 1. Deduplicate by normalized archive path key (first occurrence wins) and compute strict SHA-256 fingerprints
    std::vector<FingerprintedVpk> fingerprinted;
    fingerprinted.reserve(vpkPaths.size());
    QSet<QString> seenPathKeys;
    seenPathKeys.reserve(static_cast<qsizetype>(vpkPaths.size()));

    for (const auto& vpkPath : vpkPaths) {
        if (token.isCancelled()) {
            return Core::Result<VpkIndexResult>::cancelled(
                QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
        }

        const QString pathKey = Domain::Package::VpkIndex::normalizeArchivePathKey(vpkPath);
        if (pathKey.isEmpty() || seenPathKeys.contains(pathKey)) {
            continue;
        }
        seenPathKeys.insert(pathKey);

        if (!vpkPath.exists() || !vpkPath.isFile()) {
            continue;
        }

        auto shaRes = Core::Hash::Sha256::computeFileHash(vpkPath, token);
        if (shaRes.isCancelled()) {
            return Core::Result<VpkIndexResult>::cancelled(shaRes.message());
        }
        if (shaRes.isFailure()) {
            return Core::Result<VpkIndexResult>::failure(
                shaRes.error(),
                QCoreApplication::translate("VpkIndexService", "Failed to compute SHA-256 for VPK: %1")
                    .arg(vpkPath.toString()));
        }

        const QFileInfo fi(vpkPath.toString());
        FingerprintedVpk entry;
        entry.vpkPath = vpkPath;
        entry.pathKey = pathKey;
        entry.size = fi.size();
        entry.lastModified = fi.lastModified().toMSecsSinceEpoch();
        entry.sha256 = shaRes.value().toLower();
        fingerprinted.push_back(std::move(entry));
    }

    // 2. Process-local composite fast path (0 disk .vpkidx reads, 0 PackArchive::open, 0 re-compose)
    std::shared_ptr<const Domain::Package::VpkIndex> fastPathIndex;
    bool fastPathPersisted = true;
    Core::Error::Error fastPathPersistErr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto existing = m_indices.value(key, nullptr);
        if (existing
            && existing->isCs2Index() == isCs2
            && existing->vpks().size() == fingerprinted.size()) {
            bool matches = true;
            const auto& existingVpks = existing->vpks();
            for (std::size_t i = 0; i < fingerprinted.size(); ++i) {
                if (Domain::Package::VpkIndex::normalizeArchivePathKey(existingVpks[i].path) != fingerprinted[i].pathKey
                    || existingVpks[i].sha256 != fingerprinted[i].sha256) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                if (isCs2) {
                    m_cs2Index = existing;
                }
                if (logicalKey != key) {
                    m_indices[logicalKey] = existing;
                }
                if (key == m_activeSource1GameId || logicalKey == m_activeSource1GameId) {
                    m_activeSource1Index = existing;
                }
                if (m_persistenceErrors.contains(key)) {
                    fastPathPersisted = false;
                    fastPathPersistErr = m_persistenceErrors.value(key);
                }
                fastPathIndex = existing;
            }
        }
    }

    if (fastPathIndex) {
        dispatchIndexReady(key);
        return Core::Result<VpkIndexResult>::success(
            VpkIndexResult{
                std::move(fastPathIndex),
                fastPathPersisted,
                std::move(fastPathPersistErr),
                fingerprinted.size(),
                0
            });
    }

    // 3. Per-VPK cache lookup + single-flight load/build + selective parse
    const Core::Path::FilesystemPath archiveDir = effectiveArchiveDirectory(key);
    std::vector<Domain::Package::VpkArchiveSource> sources;
    sources.reserve(fingerprinted.size());

    std::size_t reusedArchiveCount = 0;
    std::size_t rebuiltArchiveCount = 0;
    bool persisted = true;
    Core::Error::Error persistenceErr;

    for (const auto& fp : fingerprinted) {
        if (token.isCancelled()) {
            return Core::Result<VpkIndexResult>::cancelled(
                QCoreApplication::translate("VpkIndexService", "Index build cancelled"));
        }

        bool reused = false;
        bool entryPersisted = true;
        Core::Error::Error entryPersistErr;
        auto archiveRes = resolveArchiveSnapshot(
            fp.vpkPath,
            fp.sha256,
            archiveDir,
            token,
            reused,
            entryPersisted,
            entryPersistErr);

        if (archiveRes.isCancelled()) {
            return Core::Result<VpkIndexResult>::cancelled(archiveRes.message());
        }
        if (archiveRes.isFailure()) {
            // Skip failed archive so it is omitted from VpkIndex::compose / coversVpk.
            continue;
        }

        if (reused) {
            ++reusedArchiveCount;
        } else {
            ++rebuiltArchiveCount;
        }

        if (!entryPersisted && persisted) {
            persisted = false;
            persistenceErr = std::move(entryPersistErr);
        }

        Domain::Package::VpkArchiveSource source;
        source.meta.path = fp.vpkPath;
        source.meta.size = fp.size;
        source.meta.lastModified = fp.lastModified;
        source.meta.sha256 = fp.sha256;
        source.archiveIndex = std::move(archiveRes.value());
        sources.push_back(std::move(source));
    }

    // 4. Compose immutable VpkIndex and publish
    auto shared = std::make_shared<const Domain::Package::VpkIndex>(
        Domain::Package::VpkIndex::compose(std::move(sources), isCs2));

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!persisted) {
            m_persistenceErrors[key] = persistenceErr;
            if (logicalKey != key) {
                m_persistenceErrors[logicalKey] = persistenceErr;
            }
        } else {
            m_persistenceErrors.remove(key);
            if (logicalKey != key) {
                m_persistenceErrors.remove(logicalKey);
            }
        }
        m_indices[key] = shared;
        if (logicalKey != key) {
            m_indices[logicalKey] = shared;
        }
        if (isCs2) {
            m_cs2Index = shared;
        }
        if (key == m_activeSource1GameId || logicalKey == m_activeSource1GameId) {
            m_activeSource1Index = shared;
        }
    }

    if (!persisted) {
        dispatchIndexPersistenceFailed(key, persistenceErr.message());
    }
    dispatchIndexUpdated(key);
    dispatchIndexReady(key);
    return Core::Result<VpkIndexResult>::success(
        VpkIndexResult{
            std::move(shared),
            persisted,
            std::move(persistenceErr),
            reusedArchiveCount,
            rebuiltArchiveCount
        });
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
            auto res = self->ensureIndexSync(gameId, vpkPaths, isCs2, token);
            if (res.isSuccess()) {
                sysLog.info(QStringLiteral("VPK index ready for '%1': reused %2 archive(s) via SHA-256 cache, parsed %3 archive(s)")
                    .arg(gameId)
                    .arg(res.value().reusedArchiveCount)
                    .arg(res.value().rebuiltArchiveCount));
            }
            return res;
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
            auto res = self->ensureCs2IndexFromGameInfoSync(cs2BasePath, token);
            if (res.isSuccess()) {
                sysLog.info(QStringLiteral("CS2 VPK index ready: reused %1 archive(s) via SHA-256 cache, parsed %2 archive(s)")
                    .arg(res.value().reusedArchiveCount)
                    .arg(res.value().rebuiltArchiveCount));
            }
            return res;
        },
        callback);
}

Core::Result<VpkIndexResult> VpkIndexService::ensureSource1IndexFromGameInfoSync(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    const Core::Async::CancellationToken& token) {
    QString cacheCategory;
    auto targetsRes = Domain::Game::GameInstallationResolver::resolveSource1VpkTargets(
        gameId,
        gameInfoPathOrDir,
        &cacheCategory);
    if (targetsRes.isFailure()) {
        return Core::Result<VpkIndexResult>::failure(
            targetsRes.error(),
            targetsRes.message());
    }

    const QString logicalKey = Domain::Game::GameInstallationResolver::resolveIndexCategory(gameId);
    return ensureIndexSyncInternal(logicalKey, cacheCategory, targetsRes.value(), false, token);
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
            auto res = self->ensureSource1IndexFromGameInfoSync(gameId, gameInfoPathOrDir, token);
            if (res.isSuccess()) {
                sysLog.info(QStringLiteral("Source 1 VPK index ready for '%1': reused %2 archive(s) via SHA-256 cache, parsed %3 archive(s)")
                    .arg(gameId)
                    .arg(res.value().reusedArchiveCount)
                    .arg(res.value().rebuiltArchiveCount));
            }
            return res;
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
        m_activeSource1GameId = Domain::Game::GameInstallationResolver::resolveIndexCategory(gameId);
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
