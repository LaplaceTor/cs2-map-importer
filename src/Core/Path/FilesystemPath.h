#pragma once

#include <optional>
#include <QString>

class QFileDevice;

namespace Core::Path {

/**
 * @brief Normalized host filesystem path representation with dual-tier security boundaries.
 *
 * Security Architecture (Two Distinct Guarantees):
 * -------------------------------------------------
 * 1. Tier 1 - Logical Path Containment (Pre-Open Planning):
 *    - Functions: isSubpathOf(), contains(), resolveBelow()
 *    - Guarantees: Rejects lexical traversal ('..', leading slashes, drive colons ':', NTFS ADS streams)
 *      and resolves existing symbolic links / directory junctions via std::filesystem::weakly_canonical.
 *    - Scope: Intended for path planning, existence probing, and filtering.
 *    - Limitation: Cannot prevent TOCTOU (Time-of-Check to Time-of-Use) race conditions if intermediate
 *      directories are altered or swapped with junctions AFTER path resolution but BEFORE file creation.
 *
 * 2. Tier 2 - Physical Kernel Handle Validation (Post-Open Write Boundary):
 *    - Functions: verifyHandleWithinBase(), verifyFileWithinBase()
 *    - Guarantees: Interrogates the underlying OS file object via Win32 GetFinalPathNameByHandleW
 *      (FILE_NAME_NORMALIZED | VOLUME_NAME_DOS) on the already-opened handle.
 *    - Scope: MANDATORY for security-sensitive write operations (archive extraction, file copying, atomic writes).
 *    - Benefit: Guarantees that the physical storage being written to strictly resides within baseDir,
 *      completely immune to symlink/junction races and TOCTOU directory redirection.
 */
class FilesystemPath {
public:
    FilesystemPath();
    explicit FilesystemPath(const QString& path);

    bool isEmpty() const;
    bool isValid() const;
    bool exists() const;
    bool isFile() const;
    bool isDirectory() const;
    bool isAbsolute() const;
    bool isRelative() const;

    /**
     * @brief [Tier 1 Logical Path] Checks whether this path logically and canonically resides within baseDir.
     *
     * Performs physical canonical resolution using std::filesystem::weakly_canonical,
     * correctly following Windows directory junctions, NTFS reparse points, and symbolic links
     * even if baseDir or child elements do not yet exist on disk.
     *
     * Note: This is a pre-opening logical check. For safe file writes, callers must also invoke
     * verifyFileWithinBase() / verifyHandleWithinBase() on the opened file handle.
     *
     * @param baseDir The expected parent directory.
     * @return true if this path resolves within baseDir; false otherwise.
     */
    bool isSubpathOf(const FilesystemPath& baseDir) const;
    bool contains(const FilesystemPath& childPath) const;

    /**
     * @brief [Tier 1 Logical Path] Safely resolves an untrusted relative subpath strictly underneath this directory.
     *
     * Enforces:
     * 1. Relative subpath check (rejects absolute paths and leading slashes).
     * 2. Rejection of traversal components ('../', '..').
     * 3. Rejection of drive letter colons (':') and NTFS Alternate Data Streams (ADS).
     * 4. Canonical containment via isSubpathOf() and std::filesystem::weakly_canonical.
     *
     * Note on TOCTOU: Safe write pipelines must pair this with verifyFileWithinBase() on the opened
     * destination file handle before committing data.
     *
     * @param subpath Relative subpath to resolve below this directory.
     * @return Resolved FilesystemPath if safely below this directory, or std::nullopt.
     */
    std::optional<FilesystemPath> resolveBelow(const QString& subpath) const;
    std::optional<FilesystemPath> resolveBelow(const FilesystemPath& subpath) const;

    /**
     * @brief [Tier 2 Physical Handle] Verifies that an open Win32 file handle physically resides within baseDir.
     *
     * Queries the kernel file object via GetFinalPathNameByHandleW to obtain the actual normalized
     * physical destination path, closing TOCTOU race conditions where intermediate directory symlinks
     * or junctions were introduced after path resolution.
     *
     * @param win32Handle The open Win32 HANDLE (cast to void*).
     * @param baseDir The expected containing directory.
     * @return true if the open handle's physical target is strictly inside baseDir; false otherwise.
     */
    static bool verifyHandleWithinBase(void* win32Handle, const FilesystemPath& baseDir);

    /**
     * @brief [Tier 2 Physical Handle] Verifies that an open QFileDevice (QFile, QSaveFile, etc.)
     *        physically resides within baseDir.
     *
     * Extracts the underlying OS handle and verifies its final destination via GetFinalPathNameByHandleW.
     *
     * @param file An open QFileDevice instance.
     * @param baseDir The expected containing directory.
     * @return true if the open file's physical target is strictly inside baseDir; false otherwise.
     */
    static bool verifyFileWithinBase(const QFileDevice& file, const FilesystemPath& baseDir);

    QString fileName() const;
    QString extension() const;
    FilesystemPath parentPath() const;
    FilesystemPath absolutePath() const;
    FilesystemPath canonicalPath() const;
    FilesystemPath join(const QString& subpath) const;
    FilesystemPath join(const FilesystemPath& subpath) const;
    QString toString() const;

    FilesystemPath operator/(const QString& subpath) const;
    FilesystemPath operator/(const FilesystemPath& subpath) const;

    bool operator==(const FilesystemPath& other) const;
    bool operator!=(const FilesystemPath& other) const;

private:
    QString m_path;
};

} // namespace Core::Path
