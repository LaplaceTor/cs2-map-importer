#pragma once

#include <QString>

namespace Core::FileSystem {

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
     * @param errorMessage Optional pointer to receive error description if acquisition fails.
     * @return true if exclusive lease was acquired successfully, false otherwise.
     */
    bool acquireExclusive(
        const QString& filePath,
        QString* errorMessage = nullptr);

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
#ifdef Q_OS_WIN
    void* m_handle = nullptr;
#endif

    QString m_filePath;
};

} // namespace Core::FileSystem

