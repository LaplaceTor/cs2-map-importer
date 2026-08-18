#include "FileSystem.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDirIterator>

namespace Core::FileSystem {

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

bool FileSystem::createDirectory(const QString& path) {
    if (path.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot create directory: Path is empty"));
    }

    QDir dir(path);
    if (dir.exists()) {
        return true;
    }

    if (!dir.mkpath(QStringLiteral("."))) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Failed to create directory: %1").arg(path));
    }
    return true;
}

bool FileSystem::remove(const QString& path) {
    if (path.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot remove: Path is empty"));
    }

    QFileInfo info(path);
    if (!info.exists()) {
        return true; // Already doesn't exist
    }

    if (info.isDir()) {
        QDir dir(path);
        if (!dir.removeRecursively()) {
            throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Failed to remove directory recursively: %1").arg(path));
        }
    } else {
        QFile file(path);
        if (!file.remove()) {
            throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Failed to remove file: %1 (%2)").arg(path, file.errorString()));
        }
    }
    return true;
}

bool FileSystem::copy(const QString& source, const QString& destination, bool overwrite) {
    if (source.isEmpty() || destination.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot copy: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw ImportException(ImportErrorCode::FileNotFound, QStringLiteral("Cannot copy: Source path does not exist: %1").arg(source));
    }

    if (srcInfo.isDir()) {
        return copyDirectoryHelper(source, destination, overwrite);
    }

    QFileInfo dstInfo(destination);
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Cannot copy: Destination file already exists: %1").arg(destination));
        }
        QFile dstFile(destination);
        if (!dstFile.remove()) {
            throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Cannot copy: Failed to overwrite existing destination file: %1").arg(destination));
        }
    } else {
        QDir parentDir = dstInfo.dir();
        if (!parentDir.exists()) {
            createDirectory(parentDir.absolutePath());
        }
    }

    if (!QFile::copy(source, destination)) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Failed to copy file from %1 to %2").arg(source, destination));
    }
    return true;
}

bool FileSystem::copyDirectoryHelper(const QString& source, const QString& destination, bool overwrite) {
    QDir srcDir(source);
    if (!createDirectory(destination)) {
        return false;
    }

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
    return true;
}

bool FileSystem::move(const QString& source, const QString& destination, bool overwrite) {
    if (source.isEmpty() || destination.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot move: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw ImportException(ImportErrorCode::FileNotFound, QStringLiteral("Cannot move: Source path does not exist: %1").arg(source));
    }

    QFileInfo dstInfo(destination);
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Cannot move: Destination path already exists: %1").arg(destination));
        }
        remove(destination);
    } else {
        QDir parentDir = dstInfo.dir();
        if (!parentDir.exists()) {
            createDirectory(parentDir.absolutePath());
        }
    }

    // Try QDir::rename first
    QDir dir;
    if (dir.rename(source, destination)) {
        return true;
    }

    // Fallback to copy & remove
    copy(source, destination, overwrite);
    remove(source);
    return true;
}

QByteArray FileSystem::readAll(const QString& filePath) {
    if (filePath.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot read file: Path is empty"));
    }

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        throw ImportException(ImportErrorCode::FileNotFound, QStringLiteral("Cannot read file: File does not exist: %1").arg(filePath));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw ImportException(ImportErrorCode::PermissionDenied, QStringLiteral("Cannot open file for reading: %1 (%2)").arg(filePath, file.errorString()));
    }

    return file.readAll();
}

bool FileSystem::writeAll(const QString& filePath, const QByteArray& data) {
    if (filePath.isEmpty()) {
        throw ImportException(ImportErrorCode::InvalidPath, QStringLiteral("Cannot write file: Path is empty"));
    }

    QFileInfo dstInfo(filePath);
    QDir parentDir = dstInfo.dir();
    if (!parentDir.exists()) {
        createDirectory(parentDir.absolutePath());
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw ImportException(ImportErrorCode::PermissionDenied, QStringLiteral("Cannot open file for writing: %1 (%2)").arg(filePath, file.errorString()));
    }

    qint64 written = file.write(data);
    if (written != data.size()) {
        throw ImportException(ImportErrorCode::OperationFailed, QStringLiteral("Failed to write all data to file: %1 (%2)").arg(filePath, file.errorString()));
    }

    return true;
}

} // namespace Core::FileSystem
