#pragma once

#include <QString>
#include <QFile>
#include <utility>

#include "Core/Path/FilesystemPath.h"

namespace Core::Temp {

/**
 * @brief RAII guard to clean up (remove) a file when leaving scope.
 *
 * Useful for scenarios where a temporary file or copy is created at a specific
 * path (e.g. required by an external tool) and must be cleaned up on scope exit
 * unless dismissed.
 */
class TempFileCleanup {
public:
    TempFileCleanup() = default;

    explicit TempFileCleanup(const QString& path, bool active = true)
        : m_path(path), m_active(active && !path.isEmpty()) {}

    explicit TempFileCleanup(const Core::Path::FilesystemPath& path, bool active = true)
        : m_path(path.toString()), m_active(active && !path.isEmpty()) {}

    ~TempFileCleanup() {
        cleanup();
    }

    // Disable copy
    TempFileCleanup(const TempFileCleanup&) = delete;
    TempFileCleanup& operator=(const TempFileCleanup&) = delete;

    // Enable move
    TempFileCleanup(TempFileCleanup&& other) noexcept
        : m_path(std::move(other.m_path)), m_active(other.m_active) {
        other.m_active = false;
    }

    TempFileCleanup& operator=(TempFileCleanup&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_path = std::move(other.m_path);
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

    void setPath(const QString& path, bool active = true) {
        cleanup();
        m_path = path;
        m_active = active && !path.isEmpty();
    }

    void setPath(const Core::Path::FilesystemPath& path, bool active = true) {
        setPath(path.toString(), active);
    }

    QString path() const {
        return m_path;
    }

    Core::Path::FilesystemPath filesystemPath() const {
        return Core::Path::FilesystemPath(m_path);
    }

    bool isActive() const {
        return m_active;
    }

    void setActive(bool active) {
        m_active = active;
    }

    /// Disarms the cleanup guard so the file will not be deleted.
    void dismiss() {
        m_active = false;
    }

    /// Releases ownership of the cleanup duty and returns the path.
    QString release() {
        m_active = false;
        return m_path;
    }

    /// Manually triggers cleanup immediately and disarms the guard.
    bool cleanup() {
        if (m_active && !m_path.isEmpty() && QFile::exists(m_path)) {
            m_active = false;
            return QFile::remove(m_path);
        }
        m_active = false;
        return false;
    }

private:
    QString m_path;
    bool m_active = false;
};

} // namespace Core::Temp
