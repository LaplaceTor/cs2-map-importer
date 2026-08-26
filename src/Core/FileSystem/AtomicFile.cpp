#include "AtomicFile.h"
#include <QFileInfo>
#include <QDir>
#include <utility>

namespace Core::FileSystem {

AtomicFile::AtomicFile(const QString& targetFilePath)
    : m_targetFilePath(targetFilePath) {
}

AtomicFile::~AtomicFile() {
    rollback();
}

AtomicFile::AtomicFile(AtomicFile&& other) noexcept
    : m_targetFilePath(std::move(other.m_targetFilePath)),
      m_saveFile(std::move(other.m_saveFile)),
      m_committed(other.m_committed),
      m_isOpen(other.m_isOpen) {
    other.m_committed = true; // prevent destructor of moved-from object from doing rollback
    other.m_isOpen = false;
}

AtomicFile& AtomicFile::operator=(AtomicFile&& other) noexcept {
    if (this != &other) {
        rollback();
        m_targetFilePath = std::move(other.m_targetFilePath);
        m_saveFile = std::move(other.m_saveFile);
        m_committed = other.m_committed;
        m_isOpen = other.m_isOpen;

        other.m_committed = true;
        other.m_isOpen = false;
    }
    return *this;
}

QString AtomicFile::tempFilePath() const {
    if (m_saveFile) {
        return m_saveFile->fileName();
    }
    return QString();
}

void AtomicFile::open() {
    if (m_committed) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Cannot open AtomicFile: Already committed"));
    }
    if (m_isOpen) {
        return;
    }

    if (m_targetFilePath.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Cannot open AtomicFile: Target path is empty"));
    }

    QFileInfo dstInfo(m_targetFilePath);
    QDir parentDir = dstInfo.dir();
    if (!parentDir.exists()) {
        if (!parentDir.mkpath(QStringLiteral("."))) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("Failed to create parent directory for atomic write: %1").arg(parentDir.absolutePath()));
        }
    }

    m_saveFile = std::make_unique<QSaveFile>(m_targetFilePath);
    if (!m_saveFile->open(QIODevice::WriteOnly)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Failed to open QSaveFile for target '%1': %2")
                .arg(m_targetFilePath, m_saveFile->errorString()));
    }

    m_isOpen = true;
}

void AtomicFile::write(const QByteArray& data) {
    if (!m_isOpen) {
        open();
    }

    if (!m_saveFile) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("AtomicFile QSaveFile is not open"));
    }

    qint64 written = m_saveFile->write(data);
    if (written != data.size()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Failed to write to QSaveFile for target '%1': %2")
                .arg(m_targetFilePath, m_saveFile->errorString()));
    }
}

void AtomicFile::commit() {
    if (m_committed) {
        return;
    }

    if (!m_isOpen || !m_saveFile) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Cannot commit AtomicFile: File was not opened or written"));
    }

    if (!m_saveFile->commit()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QStringLiteral("Failed to commit QSaveFile for target '%1': %2")
                .arg(m_targetFilePath, m_saveFile->errorString()));
    }

    m_committed = true;
    m_isOpen = false;
    m_saveFile.reset();
}

void AtomicFile::rollback() {
    if (m_committed) {
        return;
    }

    if (m_saveFile) {
        m_saveFile->cancelWriting();
        m_saveFile.reset();
    }

    m_isOpen = false;
}

void AtomicFile::writeAtomic(const QString& targetFilePath, const QByteArray& data) {
    AtomicFile atomic(targetFilePath);
    atomic.open();
    atomic.write(data);
    atomic.commit();
}

} // namespace Core::FileSystem
