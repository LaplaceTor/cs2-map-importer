#pragma once

#include <QString>
#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <memory>

#include "Core/Error/Exception.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Path/FilesystemPath.h"

namespace Core::FileSystem {

class AtomicFile {
public:
    explicit AtomicFile(
        const QString& targetFilePath,
        const Core::Path::FilesystemPath& expectedBaseDir = {});
    ~AtomicFile();

    // Disable copy
    AtomicFile(const AtomicFile&) = delete;
    AtomicFile& operator=(const AtomicFile&) = delete;

    // Enable move
    AtomicFile(AtomicFile&& other) noexcept;
    AtomicFile& operator=(AtomicFile&& other) noexcept;

    const QString& targetFilePath() const { return m_targetFilePath; }
    const Core::Path::FilesystemPath& expectedBaseDir() const { return m_expectedBaseDir; }
    QString tempFilePath() const;

    void open();

    /**
     * @brief Returns the underlying open QIODevice for streaming writes.
     *
     * Callers must call open() before device() (so Tier 1 path containment and Tier 2
     * verifyFileWithinBase() checks have completed) and call commit() after writing.
     *
     * @return Open QIODevice pointer, or nullptr if not currently open.
     */
    QIODevice* device();

    void write(const QByteArray& data);
    void write(const char* data, qint64 size);
    void commit();
    void rollback();
    bool isCommitted() const { return m_committed; }

    static void writeAtomic(
        const QString& targetFilePath,
        const QByteArray& data,
        const Core::Path::FilesystemPath& expectedBaseDir = {});

private:
    QString m_targetFilePath;
    Core::Path::FilesystemPath m_expectedBaseDir;
    std::unique_ptr<QSaveFile> m_saveFile;
    bool m_committed = false;
    bool m_isOpen = false;
};

} // namespace Core::FileSystem
