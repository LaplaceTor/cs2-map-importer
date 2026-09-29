#pragma once

#include <optional>
#include <QString>

class QFile;

namespace Core::Path {

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
     * @brief Determines whether this path is physically contained within baseDir.
     *
     * Performs both lexical path containment and physical canonical resolution
     * using std::filesystem::weakly_canonical, correctly resolving Windows directory junctions,
     * NTFS reparse points, and symbolic links even if baseDir or child components do not yet exist.
     *
     * @param baseDir The containing directory to check against.
     * @return true if this path is physically within baseDir; false otherwise.
     */
    bool isSubpathOf(const FilesystemPath& baseDir) const;
    bool contains(const FilesystemPath& childPath) const;

    /**
     * @brief Safely resolves a subpath strictly underneath this directory.
     *
     * Security contract:
     * 1. Lexical constraint: Rejects absolute paths, leading slashes, parent-directory
     *    traversal components ('../' or '..'), drive colons (':'), and NTFS Alternate Data Streams.
     * 2. Physical boundary guarantee: Verifies that the resolved path is physically contained
     *    within baseDir via std::filesystem::weakly_canonical, resolving symlinks, directory junctions,
     *    and reparse points even if baseDir or child components do not yet exist on disk.
     *
     * Note on TOCTOU: In concurrently modified or hostile environments where intermediate directories
     * might be swapped with junctions between path resolution and file creation, callers performing
     * safe writes should additionally call verifyFileWithinBase() or verifyHandleWithinBase() on the
     * open file handle before writing data.
     *
     * @param subpath Relative subpath to resolve below this directory.
     * @return Resolved FilesystemPath if safely below this directory, or std::nullopt.
     */
    std::optional<FilesystemPath> resolveBelow(const QString& subpath) const;
    std::optional<FilesystemPath> resolveBelow(const FilesystemPath& subpath) const;

    /**
     * @brief Verifies that an open Win32 file handle physically points to a location within baseDir.
     *
     * Queries the kernel file object via GetFinalPathNameByHandleW to obtain the actual normalized
     * destination path, closing the TOCTOU race condition where intermediate directory symlinks/junctions
     * were introduced after path resolution.
     *
     * @param win32Handle The open Win32 HANDLE (cast to void*).
     * @param baseDir The expected containing directory.
     * @return true if the open handle's physical target is strictly inside baseDir; false otherwise.
     */
    static bool verifyHandleWithinBase(void* win32Handle, const FilesystemPath& baseDir);

    /**
     * @brief Verifies that an open QFile physically points to a location within baseDir.
     *
     * Extracts the underlying OS handle and verifies its final destination via GetFinalPathNameByHandleW.
     *
     * @param file An open QFile instance.
     * @param baseDir The expected containing directory.
     * @return true if the open file's physical target is strictly inside baseDir; false otherwise.
     */
    static bool verifyFileWithinBase(const QFile& file, const FilesystemPath& baseDir);

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
