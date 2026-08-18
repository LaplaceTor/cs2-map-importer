#pragma once

#include <QString>
#include <QByteArray>
#include <QSaveFile>
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
    QString tempFilePath() const;

    void open();
    void write(const QByteArray& data);
    void commit();
    void rollback();
    bool isCommitted() const { return m_committed; }

    static void writeAtomic(const QString& targetFilePath, const QByteArray& data);

private:
    QString m_targetFilePath;
    std::unique_ptr<QSaveFile> m_saveFile;
    bool m_committed = false;
    bool m_isOpen = false;
};

} // namespace Core::FileSystem
