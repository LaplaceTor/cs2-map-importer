#pragma once

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QList>

namespace Core::FileSystem {

struct FileEntry {
    QString path;
    bool exists = false;
    qint64 size = 0;
    QDateTime lastModified;

    bool operator==(const FileEntry& other) const {
        return path == other.path && exists == other.exists &&
               size == other.size && lastModified == other.lastModified;
    }
    bool operator!=(const FileEntry& other) const {
        return !(*this == other);
    }
};

struct SnapshotDiff {
    QList<FileEntry> added;
    QList<FileEntry> removed;
    QList<FileEntry> modified;
};

class DirectorySnapshot {
public:
    DirectorySnapshot() = default;
    explicit DirectorySnapshot(const QString& directoryPath);

    static DirectorySnapshot capture(const QString& directoryPath);

    const QString& rootPath() const { return m_rootPath; }

    const QMap<QString, FileEntry>& entries() const { return m_entries; }

    bool contains(const QString& relativePath) const { return m_entries.contains(relativePath); }

    FileEntry fileEntry(const QString& relativePath) const { return m_entries.value(relativePath); }

    SnapshotDiff diff(const DirectorySnapshot& newSnapshot) const;

    QList<FileEntry> added(const DirectorySnapshot& newSnapshot) const;

    QList<FileEntry> removed(const DirectorySnapshot& newSnapshot) const;

    QList<FileEntry> modified(const DirectorySnapshot& newSnapshot) const;

private:
    void checkSameRoot(const DirectorySnapshot& other) const;

    QString m_rootPath;
    QMap<QString, FileEntry> m_entries; // Key: relative path
};

} // namespace Core::FileSystem
