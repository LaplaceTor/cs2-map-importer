#include "AtomicFile.h"
#include "FileSystem.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

namespace Core::FileSystem {

AtomicFile::AtomicFile(const QString& targetFilePath)
    : m_targetFilePath(targetFilePath) {
}

AtomicFile::~AtomicFile() {
    rollback();
}

AtomicFile::AtomicFile(AtomicFile&& other) noexcept
    : m_targetFilePath(std::move(other.m_targetFilePath)),
      m_tempFilePath(std::move(other.m_tempFilePath)),
      m_tempFile(std::move(other.m_tempFile)),
      m_committed(other.m_committed),
      m_isOpen(other.m_isOpen) {
    other.m_committed = true; // prevent destructor of moved-from object from doing rollback
    other.m_isOpen = false;
}

AtomicFile& AtomicFile::operator=(AtomicFile&& other) noexcept {
    if (this != &other) {
        rollback();
        m_targetFilePath = std::move(other.m_targetFilePath);
        m_tempFilePath = std::move(other.m_tempFilePath);
        m_tempFile = std::move(other.m_tempFile);
        m_committed = other.m_committed;
        m_isOpen = other.m_isOpen;

        other.m_committed = true;
        other.m_isOpen = false;
    }
    return *this;
}

bool AtomicFile::open() {
    if (m_committed) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Cannot open AtomicFile: Already committed"));
    }
    if (m_isOpen) {
        return true;
    }

    if (m_targetFilePath.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot open AtomicFile: Target path is empty"));
    }

    QFileInfo dstInfo(m_targetFilePath);
    QDir parentDir = dstInfo.dir();
    if (!parentDir.exists()) {
        FileSystem::createDirectory(parentDir.absolutePath());
    }

    QString templateName = parentDir.filePath(QStringLiteral(".atomic_%1_XXXXXX").arg(dstInfo.fileName()));
    m_tempFile = std::make_unique<QTemporaryFile>(templateName);
    m_tempFile->setAutoRemove(true);

    if (!m_tempFile->open()) {
        throw ImportException(ImportErrorCode::OperationFailed,
            QStringLiteral("Failed to create temporary file for atomic write: %1 (%2)")
                .arg(m_targetFilePath, m_tempFile->errorString()));
    }

    m_tempFilePath = m_tempFile->fileName();
    m_isOpen = true;
    return true;
}

bool AtomicFile::write(const QByteArray& data) {
    if (!m_isOpen) {
        open();
    }

    if (!m_tempFile) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("AtomicFile temporary file is not open"));
    }

    qint64 written = m_tempFile->write(data);
    if (written != data.size()) {
        throw ImportException(ImportErrorCode::OperationFailed,
            QStringLiteral("Failed to write to temporary file: %1 (%2)")
                .arg(m_tempFilePath, m_tempFile->errorString()));
    }
    return true;
}

bool AtomicFile::commit() {
    if (m_committed) {
        return true;
    }

    if (!m_isOpen || !m_tempFile) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Cannot commit AtomicFile: File was not opened or written"));
    }

    if (!m_tempFile->flush()) {
        throw ImportException(ImportErrorCode::OperationFailed,
            QStringLiteral("Failed to flush temporary file: %1 (%2)")
                .arg(m_tempFilePath, m_tempFile->errorString()));
    }

    m_tempFile->close();

    // Disable auto remove since we will rename or move it to target
    m_tempFile->setAutoRemove(false);

    // Try direct atomic rename first
    if (m_tempFile->rename(m_targetFilePath)) {
        m_committed = true;
        m_isOpen = false;
        return true;
    }

    // If target exists, backup target file before attempting replacement so original file is safe
    if (QFile::exists(m_targetFilePath)) {
        QString backupPath = m_targetFilePath + QStringLiteral(".bak_%1").arg(QDateTime::currentMSecsSinceEpoch());
        if (QFile::exists(backupPath)) {
            QFile::remove(backupPath);
        }

        if (!QFile::rename(m_targetFilePath, backupPath)) {
            m_tempFile->setAutoRemove(true);
            throw ImportException(ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to backup target file during commit: %1").arg(m_targetFilePath));
        }

        bool replaced = false;
        if (m_tempFile->rename(m_targetFilePath)) {
            replaced = true;
        } else {
            try {
                FileSystem::move(m_tempFilePath, m_targetFilePath, true);
                replaced = true;
            } catch (...) {
                replaced = false;
            }
        }

        if (replaced) {
            QFile::remove(backupPath);
            m_committed = true;
            m_isOpen = false;
            return true;
        } else {
            // Restore original file from backup on failure
            QFile::rename(backupPath, m_targetFilePath);
            m_tempFile->setAutoRemove(true);
            throw ImportException(ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to replace target file during commit: %1").arg(m_targetFilePath));
        }
    } else {
        // Target does not exist yet
        try {
            FileSystem::move(m_tempFilePath, m_targetFilePath, true);
            m_committed = true;
            m_isOpen = false;
            return true;
        } catch (...) {
            m_tempFile->setAutoRemove(true);
            throw ImportException(ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to replace target file during commit: %1").arg(m_targetFilePath));
        }
    }
}

void AtomicFile::rollback() {
    if (m_committed) {
        return;
    }

    if (m_tempFile) {
        if (m_tempFile->isOpen()) {
            m_tempFile->close();
        }
        m_tempFile->remove();
        m_tempFile.reset();
    } else if (!m_tempFilePath.isEmpty() && QFile::exists(m_tempFilePath)) {
        QFile::remove(m_tempFilePath);
    }

    m_isOpen = false;
}

bool AtomicFile::writeAtomic(const QString& targetFilePath, const QByteArray& data) {
    AtomicFile atomic(targetFilePath);
    atomic.open();
    atomic.write(data);
    return atomic.commit();
}

} // namespace Core::FileSystem
