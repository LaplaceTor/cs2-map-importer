#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>

#include "Core/Async/CancellationToken.h"
#include "Core/Error/Error.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Package/VpkIndex.h"

namespace Application::Package {

/**
 * @brief Result payload representing the outcome of ensuring a VPK index.
 *
 * Encapsulates the resolved in-memory index alongside its disk persistence status.
 * Under best-effort persistence semantics, index construction may succeed even if
 * saving the serialized .idx cache to disk fails (partial success).
 */
struct VpkIndexResult {
    std::shared_ptr<const Domain::Package::VpkIndex> index;
    bool persisted = true;
    Core::Error::Error persistenceError;

    [[nodiscard]] bool isPersisted() const noexcept {
        return persisted;
    }

    [[nodiscard]] bool hasPersistenceError() const noexcept {
        return !persisted;
    }
};

/**
 * @brief Application service managing persistent VPK indices and background indexing tasks.
 *
 * Stores index binary files in '<AppDir>/data/indices/<game_id>.idx'.
 * Automatically verifies VPK file sizes, modification times, and SHA-256 hashes.
 * Dispatches background indexing using AsyncTaskRunner::runSystemTask.
 *
 * Index construction follows best-effort disk persistence semantics:
 * in-memory index construction may succeed even if saving to disk fails (partial success).
 * Outcomes are returned via VpkIndexResult so callers can directly inspect persistence status.
 */
class VpkIndexService : public QObject, public std::enable_shared_from_this<VpkIndexService> {
    Q_OBJECT

    // PassKey idiom: enforces that VpkIndexService can only be constructed
    // through its static create() factory, guaranteeing std::shared_ptr management.
    struct PassKey {
        explicit PassKey() = default;
    };

public:
    static std::shared_ptr<VpkIndexService> create();

    explicit VpkIndexService(PassKey, QObject* parent = nullptr);
    ~VpkIndexService() override = default;

    VpkIndexService(const VpkIndexService&) = delete;
    VpkIndexService& operator=(const VpkIndexService&) = delete;

    /**
     * @brief Gets root directory for storing index binary files: <AppDir>/data/indices.
     */
    static Core::Path::FilesystemPath indexDirectory();

    /**
     * @brief Gets the path to a specific game's index file: <AppDir>/data/indices/<game_id>.idx.
     */
    static Core::Path::FilesystemPath indexPath(const QString& gameId);

    /**
     * @brief Retrieves the in-memory cached index for a game, if loaded.
     */
    std::shared_ptr<const Domain::Package::VpkIndex> index(const QString& gameId) const;

    /**
     * @brief Retrieves the in-memory cached CS2 native index, if loaded.
     */
    std::shared_ptr<const Domain::Package::VpkIndex> cs2Index() const;

    /**
     * @brief Retrieves the currently active Source 1 game's in-memory index, if loaded.
     */
    std::shared_ptr<const Domain::Package::VpkIndex> activeSource1Index() const;

    /**
     * @brief Gets the identifier of the currently active Source 1 game.
     */
    QString activeSource1GameId() const;

    /**
     * @brief Checks if persistence to disk failed for the specified game's index.
     * Note: Callers can also inspect VpkIndexResult::hasPersistenceError() on the result returned by ensure* methods.
     */
    bool hasPersistenceError(const QString& key) const;

    /**
     * @brief Retrieves the persistence error for the specified game's index, if any.
     * Note: Callers can also inspect VpkIndexResult::persistenceError on the result returned by ensure* methods.
     */
    Core::Error::Error persistenceError(const QString& key) const;

    /**
     * @brief Synchronously ensures that the index for the given game is loaded and up to date.
     *
     * Best-effort persistence: Rebuilds the in-memory index if the cache file is missing,
     * corrupt, or modified, and attempts to persist it to disk (<AppDir>/data/indices/<game_id>.idx).
     * In-memory index construction may succeed even if persistence fails (partial success).
     * The returned VpkIndexResult contains the valid index alongside the persistence outcome.
     */
    Core::Result<VpkIndexResult> ensureIndexSync(
        const QString& gameId,
        const std::vector<Core::Path::FilesystemPath>& vpkPaths,
        bool isCs2 = false,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously ensures the index using a background system task.
     *
     * Best-effort persistence: in-memory index construction may succeed even if disk persistence fails.
     */
    void ensureIndexAsync(
        const QString& gameId,
        const std::vector<Core::Path::FilesystemPath>& vpkPaths,
        bool isCs2 = false,
        QObject* context = nullptr,
        std::function<void(const Core::Result<VpkIndexResult>&)> callback = nullptr);

    /**
     * @brief Synchronously resolves and ensures the CS2 native VPK index by parsing gameinfo.gi.
     * Only indexes VPKs under SearchPaths -> Game folders (csgo, csgo_imported, csgo_core, core).
     *
     * Best-effort persistence: in-memory index construction may succeed even if disk persistence fails.
     */
    Core::Result<VpkIndexResult> ensureCs2IndexFromGameInfoSync(
        const Core::Path::FilesystemPath& cs2BasePath,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously resolves and ensures the CS2 native VPK index.
     *
     * Best-effort persistence: in-memory index construction may succeed even if disk persistence fails.
     */
    void ensureCs2IndexFromGameInfoAsync(
        const Core::Path::FilesystemPath& cs2BasePath,
        QObject* context = nullptr,
        std::function<void(const Core::Result<VpkIndexResult>&)> callback = nullptr);

    void ensureCs2IndexFromGameInfoAsync(
        const QString& cs2BasePath,
        QObject* context = nullptr,
        std::function<void(const Core::Result<VpkIndexResult>&)> callback = nullptr);

    /**
     * @brief Synchronously resolves and ensures a Source 1 game's VPK index from gameinfo.txt or directory.
     *
     * Best-effort persistence: in-memory index construction may succeed even if disk persistence fails.
     */
    Core::Result<VpkIndexResult> ensureSource1IndexFromGameInfoSync(
        const QString& gameId,
        const Core::Path::FilesystemPath& gameInfoPathOrDir,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously resolves and ensures a Source 1 game's VPK index.
     *
     * Best-effort persistence: in-memory index construction may succeed even if disk persistence fails.
     */
    void ensureSource1IndexFromGameInfoAsync(
        const QString& gameId,
        const Core::Path::FilesystemPath& gameInfoPathOrDir,
        QObject* context = nullptr,
        std::function<void(const Core::Result<VpkIndexResult>&)> callback = nullptr);

    void ensureSource1IndexFromGameInfoAsync(
        const QString& gameId,
        const QString& gameInfoPathOrDir,
        QObject* context = nullptr,
        std::function<void(const Core::Result<VpkIndexResult>&)> callback = nullptr);

    /**
     * @brief Sets the active Source 1 game and triggers asynchronous index verification/loading.
     */
    void setActiveSource1Game(const QString& gameId, const Core::Path::FilesystemPath& gameInfoPathOrDir);
    void setActiveSource1Game(const QString& gameId, const QString& gameInfoPathOrDir);

signals:
    void indexReady(const QString& gameId);
    void indexUpdated(const QString& gameId);
    void indexPersistenceFailed(const QString& key, const QString& errorMsg);

private:
    void dispatchIndexReady(const QString& gameId);
    void dispatchIndexUpdated(const QString& gameId);
    void dispatchIndexPersistenceFailed(const QString& key, const QString& errorMsg);

    mutable std::mutex m_mutex;
    QHash<QString, std::shared_ptr<const Domain::Package::VpkIndex>> m_indices;
    QHash<QString, Core::Error::Error> m_persistenceErrors;
    std::shared_ptr<const Domain::Package::VpkIndex> m_cs2Index;
    std::shared_ptr<const Domain::Package::VpkIndex> m_activeSource1Index;
    QString m_activeSource1GameId;
};

} // namespace Application::Package
