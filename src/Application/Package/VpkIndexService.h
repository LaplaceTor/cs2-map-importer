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
 * @brief Application service managing persistent VPK indices and background indexing tasks.
 *
 * Stores index binary files in '<AppDir>/data/indices/<game_id>.idx'.
 * Automatically verifies VPK file sizes, modification times, and SHA-256 hashes.
 * Dispatches background indexing using AsyncTaskRunner::runSystemTask.
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
     */
    bool hasPersistenceError(const QString& key) const;

    /**
     * @brief Retrieves the persistence error for the specified game's index, if any.
     */
    Core::Error::Error persistenceError(const QString& key) const;

    /**
     * @brief Synchronously ensures that the index for the given game is loaded and up to date.
     * Rebuilds and persists if the file is missing, corrupt, or modified.
     */
    Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> ensureIndexSync(
        const QString& gameId,
        const std::vector<Core::Path::FilesystemPath>& vpkPaths,
        bool isCs2 = false,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously ensures the index using a background system task.
     */
    void ensureIndexAsync(
        const QString& gameId,
        const std::vector<Core::Path::FilesystemPath>& vpkPaths,
        bool isCs2 = false,
        QObject* context = nullptr,
        std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback = nullptr);

    /**
     * @brief Synchronously resolves and ensures the CS2 native VPK index by parsing gameinfo.gi.
     * Only indexes VPKs under SearchPaths -> Game folders (csgo, csgo_imported, csgo_core, core).
     */
    Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> ensureCs2IndexFromGameInfoSync(
        const Core::Path::FilesystemPath& cs2BasePath,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously resolves and ensures the CS2 native VPK index.
     */
    void ensureCs2IndexFromGameInfoAsync(
        const Core::Path::FilesystemPath& cs2BasePath,
        QObject* context = nullptr,
        std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback = nullptr);

    void ensureCs2IndexFromGameInfoAsync(
        const QString& cs2BasePath,
        QObject* context = nullptr,
        std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback = nullptr);

    /**
     * @brief Synchronously resolves and ensures a Source 1 game's VPK index from gameinfo.txt or directory.
     */
    Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>> ensureSource1IndexFromGameInfoSync(
        const QString& gameId,
        const Core::Path::FilesystemPath& gameInfoPathOrDir,
        const Core::Async::CancellationToken& token = Core::Async::CancellationToken());

    /**
     * @brief Asynchronously resolves and ensures a Source 1 game's VPK index.
     */
    void ensureSource1IndexFromGameInfoAsync(
        const QString& gameId,
        const Core::Path::FilesystemPath& gameInfoPathOrDir,
        QObject* context = nullptr,
        std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback = nullptr);

    void ensureSource1IndexFromGameInfoAsync(
        const QString& gameId,
        const QString& gameInfoPathOrDir,
        QObject* context = nullptr,
        std::function<void(const Core::Result<std::shared_ptr<const Domain::Package::VpkIndex>>&)> callback = nullptr);

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
