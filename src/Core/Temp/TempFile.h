#pragma once

#include <QString>
#include <QFile>
#include <QTemporaryFile>
#include <memory>
#include <utility>

#include "Core/Path/FilesystemPath.h"
#include "Core/Error/Exception.h"
#include "Core/Error/ErrorCode.h"

namespace Core::Temp {

/**
 * @brief Manages the lifecycle of a temporary file.
 *
 * Can either:
 * - Generate a new temporary file in the OS temp folder (via TempFile::create()).
 * - Adopt an existing file path on disk (via TempFile(path) or setPath()),
 *   ensuring it is removed when leaving scope.
 */
class TempFile {
public:
    /// Default constructor: creates an unmanaged/empty instance.
    TempFile() = default;

    /// Adopts an existing file path for RAII cleanup upon destruction.
    explicit TempFile(const QString& path, bool autoRemove = true)
        : m_path(path)
        , m_autoRemove(autoRemove && !path.isEmpty()) {}

    explicit TempFile(const Core::Path::FilesystemPath& path, bool autoRemove = true)
        : m_path(path.toString())
        , m_autoRemove(autoRemove && !path.isEmpty()) {}

    ~TempFile() {
        if (!m_autoRemove && m_tempFile) {
            m_tempFile->setAutoRemove(false);
        }
        cleanup();
    }

    // Disable copy
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    // Enable move
    TempFile(TempFile&& other) noexcept
        : m_tempFile(std::move(other.m_tempFile))
        , m_path(std::move(other.m_path))
        , m_autoRemove(other.m_autoRemove) {
        other.m_autoRemove = false;
    }

    TempFile& operator=(TempFile&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_tempFile = std::move(other.m_tempFile);
            m_path = std::move(other.m_path);
            m_autoRemove = other.m_autoRemove;
            other.m_autoRemove = false;
        }
        return *this;
    }

    /// Creates a new temporary file in the system temp directory backed by QTemporaryFile.
    static TempFile create(const QString& templatePattern = QString()) {
        TempFile tf;
        tf.m_tempFile = templatePattern.isEmpty()
            ? std::make_unique<QTemporaryFile>()
            : std::make_unique<QTemporaryFile>(templatePattern);
        if (!tf.m_tempFile->open()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("Failed to create temporary file: %1").arg(tf.m_tempFile->errorString()));
        }
        tf.m_tempFile->close();
        tf.m_path = tf.m_tempFile->fileName();
        tf.m_autoRemove = true;
        return tf;
    }

    /// Sets or adopts a new file path into this instance, cleaning up any previously managed file.
    void setPath(const QString& path, bool autoRemove = true) {
        cleanup();
        m_tempFile.reset();
        m_path = path;
        m_autoRemove = autoRemove && !path.isEmpty();
    }

    void setPath(const Core::Path::FilesystemPath& path, bool autoRemove = true) {
        setPath(path.toString(), autoRemove);
    }

    QString path() const {
        return m_path;
    }

    Core::Path::FilesystemPath filesystemPath() const {
        return Core::Path::FilesystemPath(m_path);
    }

    bool exists() const {
        return !m_path.isEmpty() && QFile::exists(m_path);
    }

    bool isValid() const {
        return !m_path.isEmpty();
    }

    bool isActive() const {
        return m_autoRemove;
    }

    void setActive(bool active) {
        m_autoRemove = active;
    }

    /// Disarms auto-removal so the file will not be deleted.
    void dismiss() {
        m_autoRemove = false;
        if (m_tempFile) {
            m_tempFile->setAutoRemove(false);
        }
    }

    /// Releases ownership of the file, disarms auto-removal, and returns the path.
    QString release() {
        dismiss();
        return m_path;
    }

    /// Triggers immediate cleanup and disarms this instance.
    bool cleanup() {
        if (!m_autoRemove) {
            return false;
        }
        m_autoRemove = false;
        if (m_tempFile) {
            m_tempFile.reset();
            return !QFile::exists(m_path);
        }
        if (!m_path.isEmpty() && QFile::exists(m_path)) {
            return QFile::remove(m_path);
        }
        return false;
    }

private:
    std::unique_ptr<QTemporaryFile> m_tempFile;
    QString m_path;
    bool m_autoRemove = false;
};

} // namespace Core::Temp
