#pragma once

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QTemporaryFile>
#include <memory>

#include "Core/Error/ImportException.h"
#include "Core/Error/ImportError.h"

namespace Core::FileSystem {

class AtomicFile {
public:
    explicit AtomicFile(const QString& targetFilePath);
    ~AtomicFile();

    // Disable copy
    AtomicFile(const AtomicFile&) = delete;
    AtomicFile& operator=(const AtomicFile&) = delete;

    // Enable move
    AtomicFile(AtomicFile&& other) noexcept;
    AtomicFile& operator=(AtomicFile&& other) noexcept;

    const QString& targetFilePath() const { return m_targetFilePath; }
    const QString& TargetFilePath() const { return m_targetFilePath; }

    const QString& tempFilePath() const { return m_tempFilePath; }
    const QString& TempFilePath() const { return m_tempFilePath; }

    bool open();
    bool Open() { return open(); }

    bool write(const QByteArray& data);
    bool Write(const QByteArray& data) { return write(data); }

    bool commit();
    bool Commit() { return commit(); }

    void rollback();
    void Rollback() { rollback(); }

    bool isCommitted() const { return m_committed; }
    bool IsCommitted() const { return m_committed; }

    static bool writeAtomic(const QString& targetFilePath, const QByteArray& data);
    static bool WriteAtomic(const QString& targetFilePath, const QByteArray& data) {
        return writeAtomic(targetFilePath, data);
    }

private:
    QString m_targetFilePath;
    QString m_tempFilePath;
    std::unique_ptr<QTemporaryFile> m_tempFile;
    bool m_committed = false;
    bool m_isOpen = false;
};

} // namespace Core::FileSystem

namespace Core {
    using FileSystem::AtomicFile;
}

using Core::FileSystem::AtomicFile;
