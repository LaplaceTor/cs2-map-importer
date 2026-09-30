#include <QCoreApplication>
#include "AtomicFile.h"
#include <QFileInfo>
#include <QDir>
#include <utility>

namespace Core::FileSystem {

AtomicFile::AtomicFile(const QString& targetFilePath, const Core::Path::FilesystemPath& expectedBaseDir)
    : m_targetFilePath(targetFilePath)
    , m_expectedBaseDir(expectedBaseDir) {
}

AtomicFile::~AtomicFile() {
    rollback();
}

AtomicFile::AtomicFile(AtomicFile&& other) noexcept
    : m_targetFilePath(std::move(other.m_targetFilePath)),
      m_expectedBaseDir(std::move(other.m_expectedBaseDir)),
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
        m_expectedBaseDir = std::move(other.m_expectedBaseDir);
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
            QCoreApplication::translate("AtomicFile", "Cannot open AtomicFile: Already committed"));
    }
    if (m_isOpen) {
        return;
    }

    if (m_targetFilePath.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("AtomicFile", "Cannot open AtomicFile: Target path is empty"));
    }

    if (!m_expectedBaseDir.isEmpty()) {
        if (!Core::Path::FilesystemPath(m_targetFilePath).isSubpathOf(m_expectedBaseDir)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AtomicFile", "Security boundary violation: Target file '%1' is not within expected base directory '%2'")
                    .arg(m_targetFilePath, m_expectedBaseDir.toString()));
        }
    }

    QFileInfo dstInfo(m_targetFilePath);
    QDir parentDir = dstInfo.dir();
    if (!parentDir.exists()) {
        if (!parentDir.mkpath(QStringLiteral("."))) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("AtomicFile", "Failed to create parent directory for atomic write: %1").arg(parentDir.absolutePath()));
        }
    }

    m_saveFile = std::make_unique<QSaveFile>(m_targetFilePath);
    if (!m_saveFile->open(QIODevice::WriteOnly)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("AtomicFile", "Failed to open QSaveFile for target '%1': %2")
                .arg(m_targetFilePath, m_saveFile->errorString()));
    }

    if (!m_expectedBaseDir.isEmpty()) {
        if (!Core::Path::FilesystemPath::verifyFileWithinBase(*m_saveFile, m_expectedBaseDir)) {
            m_saveFile->cancelWriting();
            m_saveFile.reset();
            m_isOpen = false;
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AtomicFile", "Security boundary violation: Atomic write target '%1' escaped expected base directory '%2' via reparse point or symlink")
                    .arg(m_targetFilePath, m_expectedBaseDir.toString()));
        }
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
            QCoreApplication::translate("AtomicFile", "AtomicFile QSaveFile is not open"));
    }

    qint64 written = m_saveFile->write(data);
    if (written != data.size()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("AtomicFile", "Failed to write to QSaveFile for target '%1': %2")
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
            QCoreApplication::translate("AtomicFile", "Cannot commit AtomicFile: File was not opened or written"));
    }

    if (!m_saveFile->commit()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("AtomicFile", "Failed to commit QSaveFile for target '%1': %2")
                .arg(m_targetFilePath, m_saveFile->errorString()));
    }

    if (!m_expectedBaseDir.isEmpty()) {
        QFile checkFile(m_targetFilePath);
        if (checkFile.open(QIODevice::ReadOnly)) {
            bool valid = Core::Path::FilesystemPath::verifyFileWithinBase(checkFile, m_expectedBaseDir);
            checkFile.close();
            if (!valid) {
                QFile::remove(m_targetFilePath);
                throw Core::Error::Exception(
                    Core::Error::ErrorCode::InvalidPath,
                    QCoreApplication::translate("AtomicFile", "Security boundary violation: Committed file '%1' escaped expected base directory '%2'")
                        .arg(m_targetFilePath, m_expectedBaseDir.toString()));
            }
        }
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

void AtomicFile::writeAtomic(
    const QString& targetFilePath,
    const QByteArray& data,
    const Core::Path::FilesystemPath& expectedBaseDir) {
    AtomicFile atomic(targetFilePath, expectedBaseDir);
    atomic.open();
    atomic.write(data);
    atomic.commit();
}

} // namespace Core::FileSystem
