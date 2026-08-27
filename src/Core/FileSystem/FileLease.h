#pragma once

#include <QString>

namespace Core::FileSystem {

enum class FileLeaseError {
    None,
    InvalidPath,
    NotFound,
    AlreadyInUse,   // Win32 ERROR_SHARING_VIOLATION / ERROR_LOCK_VIOLATION
    AccessDenied,   // Win32 ERROR_ACCESS_DENIED
    Unsupported,
    Unknown
};

struct FileLeaseResult {
    FileLeaseError error = FileLeaseError::None;
    QString message;

    bool isSuccess() const noexcept { return error == FileLeaseError::None; }
};

/**
 * @brief Move-only RAII abstraction for holding an OS-level exclusive file lease.
 *
 * When acquired on Windows, an OS file handle is opened with dwShareMode == 0,
 * which strictly forbids other processes or threads from opening the file with conflicting
 * access. When the object is destroyed or released, the OS handle is closed.
 * If the holding process terminates or crashes, the OS automatically reclaims the handle.
 */
class FileLease
{
public:
    FileLease() noexcept = default;
    ~FileLease();

    FileLease(const FileLease&) = delete;
    FileLease& operator=(const FileLease&) = delete;

    FileLease(FileLease&& other) noexcept;
    FileLease& operator=(FileLease&& other) noexcept;

    /**
     * @brief Attempts to acquire an exclusive OS handle on the specified file.
     * @param filePath Host filesystem path to the target file.
     * @return FileLeaseResult containing the error status and system error description.
     */
    FileLeaseResult acquireExclusive(const QString& filePath);

    /**
     * @brief Releases the currently held OS file handle, if any.
     */
    void release() noexcept;

    /**
     * @brief Checks whether an OS file handle is currently held.
     */
    bool isHeld() const noexcept;

    /**
     * @brief Returns the path of the leased file, or an empty string if no file is leased.
     */
    QString filePath() const;

private:
    void* m_handle = nullptr;

    QString m_filePath;
};

} // namespace Core::FileSystem
