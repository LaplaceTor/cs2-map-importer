#pragma once

#include <vector>
#include <optional>
#include <QHash>
#include <QSet>
#include <QString>
#include <cstdint>

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
 * @brief In-memory and persistent fast lookup index for game VPK archives.
 *
 * Provides O(1) point-lookup for asset paths to their winning VPK file,
 * preventing blind search/miss trial across multiple VPKs.
 * Also stores CS2 native asset stems for deduplication.
 */
class VpkIndex {
public:
    VpkIndex() = default;

    /**
     * @brief Loads and deserializes a VpkIndex from a binary .idx file.
     */
    static Core::Result<VpkIndex> loadFromFile(const Core::Path::FilesystemPath& filePath);

    /**
     * @brief Serializes this VpkIndex to a binary .idx file.
     */
    Core::Result<void> saveToFile(const Core::Path::FilesystemPath& filePath) const;

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
     * @brief For CS2 indices, checks whether a Source 1 asset already exists natively in CS2.
     * Performs extension and compiled suffix (_c) stem matching.
     */
    bool hasCs2NativeAsset(const QString& relativeAssetPath) const;

    /**
     * @brief Fast metadata sanity check against disk (verifies file existence, size, and mtime).
     */
    bool matchesDiskMetadata() const;

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
    void setVpks(std::vector<VpkArchiveMeta> vpks) { m_vpks = std::move(vpks); }
    void setEntries(QHash<QString, uint16_t> entries) { m_entryToVpk = std::move(entries); }
    void setCs2NativeStems(QSet<QString> stems) { m_cs2NativeStems = std::move(stems); }
    void setIsCs2Index(bool isCs2) noexcept { m_isCs2Index = isCs2; }
    void setGeneratedTimestamp(qint64 ts) noexcept { m_generatedTimestamp = ts; }

private:
    static constexpr quint64 MagicHeader = 0x584449504B505632ULL; // 'CS2VPKID' in LE
    static constexpr quint32 CurrentVersion = 1;

    std::vector<VpkArchiveMeta> m_vpks;
    QHash<QString, uint16_t> m_entryToVpk; // normalizedPath -> vpkId
    QSet<QString> m_cs2NativeStems;        // normalized stems for CS2 deduplication
    bool m_isCs2Index = false;
    qint64 m_generatedTimestamp = 0;
};

} // namespace Domain::Package
