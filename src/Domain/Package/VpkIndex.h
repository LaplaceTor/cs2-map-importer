#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringView>

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace Domain::Package {

/**
 * @brief Metadata for a single indexed VPK archive.
 */
struct VpkArchiveMeta {
    uint16_t id = 0;
    Core::Path::FilesystemPath path;
    qint64 size = 0;
    qint64 lastModified = 0; // Milliseconds since epoch
    QString sha256;          // Lowercase 64-char hex string
};

/**
 * @brief Immutable per-VPK entry snapshot with content-addressed .vpkidx serialization.
 *
 * Stores the archive's lowercase SHA-256 digest and its normalized, deduplicated
 * list of archive-relative entry paths.
 */
class VpkArchiveIndex {
public:
    VpkArchiveIndex() = default;

    /**
     * @brief Constructs an archive index snapshot with lowercase SHA-256 and normalized entries.
     */
    VpkArchiveIndex(QString sha256, std::vector<QString> entries);

    /**
     * @brief Validates whether a string view is an exact 64-character ASCII hex digest.
     */
    static bool isValidSha256Hex(QStringView hex) noexcept;

    /**
     * @brief Loads and validates a per-VPK snapshot from a binary .vpkidx file.
     * @param filePath Path to the .vpkidx file.
     * @param expectedSha256 Optional expected SHA-256 hex digest (case-insensitive check).
     */
    static Core::Result<VpkArchiveIndex> loadFromFile(
        const Core::Path::FilesystemPath& filePath,
        const QString& expectedSha256 = {});

    /**
     * @brief Atomically serializes this per-VPK snapshot to a binary .vpkidx file.
     * @param filePath Destination .vpkidx file path.
     * @param expectedBaseDir Optional Tier 1/Tier 2 security boundary directory enforced by AtomicFile.
     */
    Core::Result<void> saveToFile(
        const Core::Path::FilesystemPath& filePath,
        const Core::Path::FilesystemPath& expectedBaseDir = {}) const;

    const QString& sha256() const noexcept { return m_sha256; }
    const std::vector<QString>& entries() const noexcept { return m_entries; }
    std::size_t entryCount() const noexcept { return m_entries.size(); }

private:
    static constexpr quint64 MagicHeader = 0x31414B5056325343ULL; // "CS2VPKA1" in LE
    static constexpr quint32 CurrentVersion = 1;
    static constexpr qint64 MinHeaderSize = 80;
    static constexpr quint32 MaxEntryByteLength = 4096;

    QString m_sha256;
    std::vector<QString> m_entries;
};

/**
 * @brief Input source pairing archive metadata with its immutable per-VPK snapshot.
 */
struct VpkArchiveSource {
    VpkArchiveMeta meta;
    std::shared_ptr<const VpkArchiveIndex> archiveIndex;
};

/**
 * @brief In-memory fast lookup index composed from prioritized VPK archive snapshots.
 *
 * Provides O(1) point-lookup for asset paths to their winning VPK file and
 * O(1) coverage verification for indexed VPK paths, preventing blind search/miss
 * trial across multiple VPKs. Also stores CS2 native asset stems for deduplication.
 *
 * ### Concurrency & Lifecycle Contract:
 * - **Immutability & Thread Safety**: Once composed or populated, VpkIndex is completely
 *   IMMUTABLE. All query APIs (findVpkForEntry, hasEntry, hasCs2NativeAsset, coversVpk) are
 *   const and thread-safe without requiring external synchronization. A single instance may
 *   be safely shared across concurrent worker threads and workflow tasks.
 * - **Lifecycle**: When passed as a borrowed pointer (e.g. to AssetLocateOptions or
 *   AssetLocateStrategy), the caller retains ownership and must ensure the index
 *   remains allocated and unchanged throughout the duration of the call.
 */
class VpkIndex {
public:
    VpkIndex() = default;

    /**
     * @brief Composes an immutable VpkIndex from prioritized per-VPK snapshots.
     * @param sources Archive sources ordered by priority (highest priority first).
     * @param isCs2 Whether this is an index for CS2 (enables native stem indexing).
     */
    static VpkIndex compose(std::vector<VpkArchiveSource> sources, bool isCs2);

    /**
     * @brief Lexically normalizes an archive path for case-insensitive lookup keys.
     */
    static QString normalizeArchivePathKey(const Core::Path::FilesystemPath& path);

    /**
     * @brief Finds the exact VPK archive containing the specified entry path.
     * @param entryPath Case-insensitive relative path (e.g. "materials/brick/wall01.vmt").
     * @return FilesystemPath to the winning VPK, or std::nullopt if not present in any indexed VPK.
     */
    std::optional<Core::Path::FilesystemPath> findVpkForEntry(const QString& entryPath) const;

    /**
     * @brief Checks whether the exact entry exists in any indexed VPK.
     */
    bool hasEntry(const QString& entryPath) const;

    /**
     * @brief Checks whether the specified VPK archive is covered by this index.
     */
    bool coversVpk(const Core::Path::FilesystemPath& vpkPath) const;

    /**
     * @brief For CS2 indices, checks whether a Source 1 asset already exists natively in CS2.
     * Performs extension and compiled suffix (_c) stem matching.
     */
    bool hasCs2NativeAsset(const QString& relativeAssetPath) const;

    /**
     * @brief Normalizes a relative entry path (converts backslashes, trims slashes, lowercases).
     */
    static QString normalizePath(const QString& rawPath);

    /**
     * @brief Extracts normalized asset stem (strips '_c' and file extension, handles sound/sounds).
     */
    static QString extractAssetStem(const QString& rawPath);

    // Getters
    const std::vector<VpkArchiveMeta>& vpks() const noexcept { return m_vpks; }
    std::size_t entryCount() const noexcept { return m_entryToVpk.size(); }
    bool isCs2Index() const noexcept { return m_isCs2Index; }
    qint64 generatedTimestamp() const noexcept { return m_generatedTimestamp; }

    // Builder setters
    void setVpks(std::vector<VpkArchiveMeta> vpks);
    void setEntries(QHash<QString, uint16_t> entries) { m_entryToVpk = std::move(entries); }
    void setCs2NativeStems(QSet<QString> stems) { m_cs2NativeStems = std::move(stems); }
    void setIsCs2Index(bool isCs2) noexcept { m_isCs2Index = isCs2; }
    void setGeneratedTimestamp(qint64 ts) noexcept { m_generatedTimestamp = ts; }

private:
    std::vector<VpkArchiveMeta> m_vpks;
    QSet<QString> m_indexedVpkPaths;       // normalized archive path keys
    QHash<QString, uint16_t> m_entryToVpk; // normalizedPath -> vpkId
    QSet<QString> m_cs2NativeStems;        // normalized stems for CS2 deduplication
    bool m_isCs2Index = false;
    qint64 m_generatedTimestamp = 0;
};

} // namespace Domain::Package
