#pragma once

#include <memory>
#include <mutex>

#include <QHash>
#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Package/PackArchive.h"

namespace Domain::Package {

/**
 * @brief Thread-safe on-demand pool/cache for opened PackArchive instances.
 *
 * Prevents redundant opening and reparsing of large pack archives (e.g. VPKs)
 * during repeated asset lookups. Archives are opened lazily on first access
 * and retained in memory until clear() or destruction.
 *
 * ### Concurrency & Lifecycle Contract:
 * - **Thread Safety**: Fully thread-safe and reentrant. All member access is guarded
 *   by an internal mutex. A single instance may be shared concurrently across worker
 *   threads and workflow tasks without external locking.
 * - **Lifecycle**: The pool owns cached archive instances. When passed by pointer to
 *   options or context structures (e.g. AssetLocateOptions, AssetExtractOptions), the
 *   caller retains ownership of the pool and must ensure its lifetime encompasses all
 *   calls borrowing it.
 */
class PackArchivePool {
public:
    PackArchivePool() = default;
    ~PackArchivePool() = default;

    PackArchivePool(const PackArchivePool&) = delete;
    PackArchivePool& operator=(const PackArchivePool&) = delete;

    PackArchivePool(PackArchivePool&& other) noexcept;
    PackArchivePool& operator=(PackArchivePool&& other) noexcept;

    /**
     * @brief Retrieves an existing open archive for the given path, or opens
     *        and caches it if not yet loaded.
     *
     * @param archivePath Path to the pack archive.
     * @param token Optional cancellation token to abort long-running archive open operations.
     * @return Result containing shared pointer to the open PackArchive, or failure/cancelled.
     */
    Core::Result<std::shared_ptr<PackArchive>> getOrOpen(
        const Core::Path::FilesystemPath& archivePath,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Checks whether an archive for the given path is currently loaded.
     */
    bool contains(const Core::Path::FilesystemPath& archivePath) const;

    /**
     * @brief Returns the number of archives currently cached in the pool.
     */
    std::size_t size() const noexcept;

    /**
     * @brief Clears all cached archives, closing open file handles.
     */
    void clear() noexcept;

private:
    mutable std::mutex m_mutex;
    QHash<QString, std::shared_ptr<PackArchive>> m_archives;
};

} // namespace Domain::Package
