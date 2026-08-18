#include "DirectorySnapshot.h"
#include "Core/Error/ImportException.h"
#include "Core/Error/ImportError.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace Core::FileSystem {

DirectorySnapshot::DirectorySnapshot(const QString& directoryPath) {
    *this = capture(directoryPath);
}

DirectorySnapshot DirectorySnapshot::capture(const QString& directoryPath) {
    DirectorySnapshot snapshot;
    snapshot.m_rootPath = QDir::cleanPath(directoryPath);

    if (snapshot.m_rootPath.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot capture directory snapshot: Path is empty"));
    }

    QDir dir(snapshot.m_rootPath);
    if (!dir.exists()) {
        throw ImportException(ImportErrorCode::DirectoryNotFound, QStringLiteral("Directory does not exist: %1").arg(snapshot.m_rootPath));
    }

    QDirIterator it(snapshot.m_rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();

        QString relPath = dir.relativeFilePath(info.filePath());

        FileEntry entry;
        entry.path = relPath;
        entry.exists = info.exists();
        entry.size = info.size();
        entry.lastModified = info.lastModified();

        snapshot.m_entries.insert(relPath, entry);
    }

    return snapshot;
}

SnapshotDiff DirectorySnapshot::diff(const DirectorySnapshot& newSnapshot) const {
    SnapshotDiff result;
    result.added = added(newSnapshot);
    result.removed = removed(newSnapshot);
    result.modified = modified(newSnapshot);
    return result;
}

QList<FileEntry> DirectorySnapshot::added(const DirectorySnapshot& newSnapshot) const {
    QList<FileEntry> result;
    const auto& newEntries = newSnapshot.entries();

    for (auto it = newEntries.constBegin(); it != newEntries.constEnd(); ++it) {
        const QString& relPath = it.key();
        const FileEntry& newEntry = it.value();

        if (!newEntry.exists) {
            continue;
        }

        auto oldIt = m_entries.find(relPath);
        if (oldIt == m_entries.end() || !oldIt.value().exists) {
            result.append(newEntry);
        }
    }

    return result;
}

QList<FileEntry> DirectorySnapshot::removed(const DirectorySnapshot& newSnapshot) const {
    QList<FileEntry> result;
    const auto& newEntries = newSnapshot.entries();

    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString& relPath = it.key();
        const FileEntry& oldEntry = it.value();

        if (!oldEntry.exists) {
            continue;
        }

        auto newIt = newEntries.find(relPath);
        if (newIt == newEntries.end() || !newIt.value().exists) {
            result.append(oldEntry);
        }
    }

    return result;
}

QList<FileEntry> DirectorySnapshot::modified(const DirectorySnapshot& newSnapshot) const {
    QList<FileEntry> result;
    const auto& newEntries = newSnapshot.entries();

    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString& relPath = it.key();
        const FileEntry& oldEntry = it.value();

        if (!oldEntry.exists) {
            continue;
        }

        auto newIt = newEntries.find(relPath);
        if (newIt != newEntries.end() && newIt.value().exists) {
            const FileEntry& newEntry = newIt.value();
            if (oldEntry.size != newEntry.size || oldEntry.lastModified != newEntry.lastModified) {
                result.append(newEntry);
            }
        }
    }

    return result;
}

} // namespace Core::FileSystem
