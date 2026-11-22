#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <QHash>
#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace Domain::Package::Internal {

struct BspZipEntry {
    QString rawPath; // Normalized '/' separators, no leading '/', original casing preserved.
    std::uint16_t compressionMethod = 0; // 0=Store, 8=Deflate, 14=LZMA
    std::uint32_t crc32 = 0;
    std::uint64_t compressedSize = 0;
    std::uint64_t uncompressedSize = 0;
    std::uint64_t localHeaderOffset = 0;
};

class BspPakZipReader {
public:
    /// Opens a .bsp file, locates Lump 40 (PAKFILE) (decompressing Valve LZMA if needed),
    /// and parses the in-memory ZIP Central Directory.
    /// Result semantics:
    /// - Success: Valid BSP PAKFILE (entries() may still be empty for an empty ZIP).
    /// - Skipped: No PAKFILE lump or empty PAKFILE lump.
    /// - Cancelled: Cancelled via token.
    /// - Failure: Missing file, invalid path, non-VBSP file, or corrupted lump/ZIP data.
    static Core::Result<BspPakZipReader> open(
        const Core::Path::FilesystemPath& bspPath,
        const Core::Async::CancellationToken& token = {});

    const std::vector<BspZipEntry>& entries() const noexcept { return m_entries; }

    /// Case-insensitive lookup (entryPath normalizes '\' -> '/' and strips leading '/').
    bool hasEntry(const QString& entryPath) const;

    /// Reads an entry by relative path (case-insensitive).
    Core::Result<std::vector<std::byte>> readEntry(const QString& entryPath) const;

    /// Lock-free thread-safe entry decompression by index (for BspPackExtractor worker pool).
    Core::Result<std::vector<std::byte>> readEntry(std::size_t index) const;

private:
    std::vector<std::byte> m_pakBytes;
    std::vector<BspZipEntry> m_entries;
    QHash<QString, std::size_t> m_lowerPathToIndex;
};

} // namespace Domain::Package::Internal
