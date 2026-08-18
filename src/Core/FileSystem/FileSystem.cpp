#include "FileSystem.h"
#include "AtomicFile.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QDateTime>

namespace Core::FileSystem {

namespace {

bool isSubdirectoryOrEqual(const QString& childPath, const QString& parentPath) {
    QString cleanParent = QDir::cleanPath(parentPath);
    QString cleanChild = QDir::cleanPath(childPath);

    QFileInfo parentInfo(cleanParent);
    QFileInfo childInfo(cleanChild);

    if (parentInfo.exists() && childInfo.exists()) {
        QString canonParent = parentInfo.canonicalFilePath();
        QString canonChild = childInfo.canonicalFilePath();
        if (!canonParent.isEmpty() && !canonChild.isEmpty()) {
            cleanParent = canonParent;
            cleanChild = canonChild;
        }
    }

    if (cleanParent == cleanChild) {
        return true;
    }

    if (!cleanParent.endsWith(QLatin1Char('/')) && !cleanParent.endsWith(QLatin1Char('\\'))) {
        cleanParent += QLatin1Char('/');
    }

    return cleanChild.startsWith(cleanParent, Qt::CaseInsensitive);
}

} // namespace

bool FileSystem::exists(const QString& path) {
    if (path.isEmpty()) return false;
    return QFileInfo::exists(path);
}

bool FileSystem::isFile(const QString& path) {
    if (path.isEmpty()) return false;
    QFileInfo info(path);
    return info.exists() && info.isFile();
}

bool FileSystem::isDirectory(const QString& path) {
    if (path.isEmpty()) return false;
    QFileInfo info(path);
    return info.exists() && info.isDir();
}

void FileSystem::createDirectory(const QString& path) {
    if (path.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot create directory: Path is empty"));
    }

    QDir dir(path);
    if (dir.exists()) {
        return;
    }

    if (!QDir().mkpath(path)) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::OperationFailed,
            QStringLiteral("Failed to create directory: %1").arg(path));
    }
}

void FileSystem::remove(const QString& path) {
    if (path.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot remove: Path is empty"));
    }

    QFileInfo info(path);
    if (!info.exists()) {
        return; // Already doesn't exist
    }

    if (info.isDir()) {
        QDir dir(path);
        if (!dir.removeRecursively()) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to remove directory recursively: %1").arg(path));
        }
    } else {
        QFile file(path);
        if (!file.remove()) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to remove file: %1 (%2)").arg(path, file.errorString()));
        }
    }
}

void FileSystem::copy(const QString& source, const QString& destination, bool overwrite) {
    if (source.isEmpty() || destination.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot copy: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::FileNotFound,
            QStringLiteral("Cannot copy: Source path does not exist: %1").arg(source));
    }

    QFileInfo dstInfoCheck(destination);
    if (srcInfo == dstInfoCheck) {
        return; // Self-copy is a no-op
    }

    if (srcInfo.isDir()) {
        if (isSubdirectoryOrEqual(destination, source)) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::InvalidPath,
                QStringLiteral("Cannot copy directory: Destination is inside source directory (%1 -> %2)").arg(source, destination));
        }
        if (isSubdirectoryOrEqual(source, destination)) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::InvalidPath,
                QStringLiteral("Cannot copy directory: Source is inside destination directory (%1 -> %2)").arg(source, destination));
        }

        copyDirectoryHelper(source, destination, overwrite);
        return;
    }

    QFileInfo dstInfo(destination);
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Cannot copy: Destination file already exists: %1").arg(destination));
        }
        QFile dstFile(destination);
        if (!dstFile.remove()) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Cannot copy: Failed to overwrite existing destination file: %1").arg(destination));
        }
    } else {
        QDir parentDir = dstInfo.dir();
        if (!parentDir.exists()) {
            createDirectory(parentDir.absolutePath());
        }
    }

    if (!QFile::copy(source, destination)) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::OperationFailed,
            QStringLiteral("Failed to copy file from %1 to %2").arg(source, destination));
    }
}

void FileSystem::copyDirectoryHelper(const QString& source, const QString& destination, bool overwrite) {
    QDir srcDir(source);
    createDirectory(destination);

    QDirIterator it(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = srcDir.relativeFilePath(it.filePath());
        QString targetPath = QDir(destination).filePath(relPath);

        QFileInfo itemInfo = it.fileInfo();
        if (itemInfo.isDir()) {
            createDirectory(targetPath);
        } else if (itemInfo.isFile()) {
            copy(it.filePath(), targetPath, overwrite);
        }
    }
}

void FileSystem::move(const QString& source, const QString& destination, bool overwrite) {
    if (source.isEmpty() || destination.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot move: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::FileNotFound,
            QStringLiteral("Cannot move: Source path does not exist: %1").arg(source));
    }

    QFileInfo dstInfoCheck(destination);
    if (srcInfo == dstInfoCheck) {
        return; // Self-move is a no-op
    }

    if (srcInfo.isDir()) {
        if (isSubdirectoryOrEqual(destination, source)) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::InvalidPath,
                QStringLiteral("Cannot move directory: Destination is inside source directory (%1 -> %2)").arg(source, destination));
        }
        if (isSubdirectoryOrEqual(source, destination)) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::InvalidPath,
                QStringLiteral("Cannot move directory: Source is inside destination directory (%1 -> %2)").arg(source, destination));
        }
    }

    QFileInfo dstInfo(destination);
    QString backupPath;
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Cannot move: Destination path already exists: %1").arg(destination));
        }

        backupPath = destination + QStringLiteral(".bak_%1").arg(QDateTime::currentMSecsSinceEpoch());
        if (exists(backupPath)) {
            remove(backupPath);
        }

        QDir dir;
        if (!dir.rename(destination, backupPath)) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Cannot move: Failed to create temporary backup for existing destination: %1").arg(destination));
        }
    } else {
        QDir parentDir = dstInfo.dir();
        if (!parentDir.exists()) {
            createDirectory(parentDir.absolutePath());
        }
    }

    QDir dir;
    if (dir.rename(source, destination)) {
        if (!backupPath.isEmpty() && exists(backupPath)) {
            remove(backupPath);
        }
        return;
    }

    // QDir::rename failed (e.g. cross-volume move), fallback to copy & delete
    try {
        copy(source, destination, overwrite);
    } catch (...) {
        // Copy failed: clean up partial destination and restore backup if it existed
        if (exists(destination)) {
            remove(destination);
        }
        if (!backupPath.isEmpty() && exists(backupPath)) {
            dir.rename(backupPath, destination);
        }
        throw;
    }

    // Copy succeeded: remove source. If removing source fails, rollback destination and restore backup.
    try {
        remove(source);
    } catch (...) {
        if (exists(destination)) {
            remove(destination);
        }
        if (!backupPath.isEmpty() && exists(backupPath)) {
            dir.rename(backupPath, destination);
        }
        throw;
    }

    if (!backupPath.isEmpty() && exists(backupPath)) {
        remove(backupPath);
    }
}

QByteArray FileSystem::readAll(const QString& filePath) {
    if (filePath.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot read file: Path is empty"));
    }

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::FileNotFound,
            QStringLiteral("Cannot read file: File does not exist: %1").arg(filePath));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::PermissionDenied,
            QStringLiteral("Cannot open file for reading: %1 (%2)").arg(filePath, file.errorString()));
    }

    return file.readAll();
}

void FileSystem::writeAll(const QString& filePath, const QByteArray& data) {
    if (filePath.isEmpty()) {
        throw Core::Error::ImportException(
            Core::Error::ImportErrorCode::InvalidPath,
            QStringLiteral("Cannot write file: Path is empty"));
    }

    AtomicFile::writeAtomic(filePath, data);
}

} // namespace Core::FileSystem
