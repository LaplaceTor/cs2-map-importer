#pragma once

#include <memory>
#include <mutex>

#include <QHash>
#include <QString>

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
     * @return Result containing shared pointer to the open PackArchive, or failure.
     */
    Core::Result<std::shared_ptr<PackArchive>> getOrOpen(const Core::Path::FilesystemPath& archivePath);

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
