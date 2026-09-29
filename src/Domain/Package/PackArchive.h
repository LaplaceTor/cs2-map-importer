#pragma once

#include <memory>
#include <vector>

#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace vpkpp {
class PackFile;
}

namespace Domain::Package {

/**
 * @brief Unified read access to game pack archives.
 *
 * Wraps vpkpp::PackFile for VPK archives and, through the bsppp::PakLump
 * registration, BSP embedded packs. The format is detected from the file
 * extension when opening.
 *
 * Entry paths are pack-relative with '/' separators and no leading slash;
 * lookups are case-insensitive. All results follow the Core::Result contract.
 */
class PackArchive {
public:
    ~PackArchive();
    PackArchive(PackArchive&& other) noexcept;
    PackArchive& operator=(PackArchive&& other) noexcept;

    /**
     * @brief Opens a pack archive (.vpk or .bsp embedded pack).
     *
     * Failure reasons: empty or invalid path (InvalidPath), missing file
     * (FileNotFound), unsupported format or parse failure (InvalidFile),
     * or cancellation (Cancelled).
     */
    static Core::Result<PackArchive> open(
        const Core::Path::FilesystemPath& archivePath,
        const Core::Async::CancellationToken& token = {});

    bool isOpen() const noexcept;

    /**
     * @brief Lists all entry paths contained in the archive.
     */
    Core::Result<std::vector<QString>> listEntries() const;

    /**
     * @brief Checks whether an entry exists (case-insensitive).
     * Returns cancelled result if token is cancelled.
     */
    Core::Result<bool> hasEntry(
        const QString& entryPath,
        const Core::Async::CancellationToken& token = {}) const;

    /**
     * @brief Reads an entry's full contents into memory.
     */
    Core::Result<std::vector<std::byte>> readEntry(const QString& entryPath) const;

    /**
     * @brief Extracts an entry to the given destination file path,
     *        creating parent directories as needed.
     * Supports cancellation and cleans up partial output files.
     */
    Core::Result<void> extractEntryToFile(
        const QString& entryPath,
        const Core::Path::FilesystemPath& destFile,
        const Core::Async::CancellationToken& token = {},
        const Core::Path::FilesystemPath& expectedBaseDir = {}) const;

    /**
     * @brief Extracts every entry below the given directory, preserving the
     *        pack-relative structure.
     */
    Core::Result<void> extractAllToDirectory(
        const Core::Path::FilesystemPath& destDir,
        const Core::Async::CancellationToken& token = {}) const;

private:
    PackArchive() = default;

    std::unique_ptr<vpkpp::PackFile> m_packFile;
};

} // namespace Domain::Package
