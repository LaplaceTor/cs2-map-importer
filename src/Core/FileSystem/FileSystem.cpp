#include <QCoreApplication>
#include "FileSystem.h"
#include "AtomicFile.h"
#include "Core/Path/FilesystemPath.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QDateTime>

namespace Core::FileSystem {

namespace {

bool isSubdirectoryOrEqual(const QString& childPath, const QString& parentPath) {
    if (childPath.isEmpty() || parentPath.isEmpty()) {
        return false;
    }
    return Core::Path::FilesystemPath(childPath).isSubpathOf(Core::Path::FilesystemPath(parentPath));
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
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot create directory: Path is empty"));
    }

    QDir dir(path);
    if (dir.exists()) {
        return;
    }

    if (!QDir().mkpath(path)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("FileSystem", "Failed to create directory: %1").arg(path));
    }
}

void FileSystem::remove(const QString& path) {
    if (path.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot remove: Path is empty"));
    }

    QFileInfo info(path);
    if (!info.exists()) {
        return; // Already doesn't exist
    }

    if (info.isDir()) {
        QDir dir(path);
        if (!dir.removeRecursively()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Failed to remove directory recursively: %1").arg(path));
        }
    } else {
        QFile file(path);
        if (!file.remove()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Failed to remove file: %1 (%2)").arg(path, file.errorString()));
        }
    }
}

void FileSystem::copy(const QString& source, const QString& destination, bool overwrite, const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::Cancelled,
            QCoreApplication::translate("FileSystem", "Copy cancelled"));
    }

    if (source.isEmpty() || destination.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot copy: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("FileSystem", "Cannot copy: Source path does not exist: %1").arg(source));
    }

    QFileInfo dstInfoCheck(destination);
    if (srcInfo == dstInfoCheck) {
        return; // Self-copy is a no-op
    }

    if (srcInfo.isDir()) {
        if (isSubdirectoryOrEqual(destination, source)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("FileSystem", "Cannot copy directory: Destination is inside source directory (%1 -> %2)").arg(source, destination));
        }
        if (isSubdirectoryOrEqual(source, destination)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("FileSystem", "Cannot copy directory: Source is inside destination directory (%1 -> %2)").arg(source, destination));
        }

        copyDirectoryHelper(source, destination, overwrite, token);
        return;
    }

    QFileInfo dstInfo(destination);
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Cannot copy: Destination file already exists: %1").arg(destination));
        }
        QFile dstFile(destination);
        if (!dstFile.remove()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Cannot copy: Failed to overwrite existing destination file: %1").arg(destination));
        }
    } else {
        QDir parentDir = dstInfo.dir();
        if (!parentDir.exists()) {
            createDirectory(parentDir.absolutePath());
        }
    }

    // Chunked file copying with cancellation checks (64 KB chunks)
    QFile srcFile(source);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("FileSystem", "Failed to open source file for reading: %1 (%2)").arg(source, srcFile.errorString()));
    }

    QFile dstFile(destination);
    if (!dstFile.open(QIODevice::WriteOnly)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("FileSystem", "Failed to open destination file for writing: %1 (%2)").arg(destination, dstFile.errorString()));
    }

    constexpr qint64 ChunkSize = 64 * 1024;
    QByteArray buffer(ChunkSize, Qt::Uninitialized);

    while (!srcFile.atEnd()) {
        if (token.isCancelled()) {
            dstFile.close();
            dstFile.remove(); // Clean up partial destination file on cancel
            throw Core::Error::Exception(
                Core::Error::ErrorCode::Cancelled,
                QCoreApplication::translate("FileSystem", "File copy cancelled"));
        }

        qint64 bytesRead = srcFile.read(buffer.data(), ChunkSize);
        if (bytesRead < 0) {
            dstFile.close();
            dstFile.remove();
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Failed reading from %1: %2").arg(source, srcFile.errorString()));
        }

        if (bytesRead > 0) {
            qint64 bytesWritten = dstFile.write(buffer.constData(), bytesRead);
            if (bytesWritten != bytesRead) {
                dstFile.close();
                dstFile.remove();
                throw Core::Error::Exception(
                    Core::Error::ErrorCode::OperationFailed,
                    QCoreApplication::translate("FileSystem", "Failed writing to %1: %2").arg(destination, dstFile.errorString()));
            }
        }
    }
}

void FileSystem::copyDirectoryHelper(const QString& source, const QString& destination, bool overwrite, const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::Cancelled,
            QCoreApplication::translate("FileSystem", "Directory copy cancelled"));
    }

    QDir srcDir(source);
    createDirectory(destination);

    QDirIterator it(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (token.isCancelled()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::Cancelled,
                QCoreApplication::translate("FileSystem", "Directory copy cancelled"));
        }

        it.next();
        QString relPath = srcDir.relativeFilePath(it.filePath());
        QString targetPath = QDir(destination).filePath(relPath);

        QFileInfo itemInfo = it.fileInfo();
        if (itemInfo.isDir()) {
            createDirectory(targetPath);
        } else if (itemInfo.isFile()) {
            copy(it.filePath(), targetPath, overwrite, token);
        }
    }
}

void FileSystem::move(
    const QString& source,
    const QString& destination,
    bool overwrite,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::Cancelled,
            QCoreApplication::translate("FileSystem", "Move cancelled"));
    }

    if (source.isEmpty() || destination.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot move: Source or destination path is empty"));
    }

    QFileInfo srcInfo(source);
    if (!srcInfo.exists()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("FileSystem", "Cannot move: Source path does not exist: %1").arg(source));
    }

    QFileInfo dstInfoCheck(destination);
    if (srcInfo == dstInfoCheck) {
        return; // Self-move is a no-op
    }

    if (srcInfo.isDir()) {
        if (isSubdirectoryOrEqual(destination, source)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("FileSystem", "Cannot move directory: Destination is inside source directory (%1 -> %2)").arg(source, destination));
        }
        if (isSubdirectoryOrEqual(source, destination)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("FileSystem", "Cannot move directory: Source is inside destination directory (%1 -> %2)").arg(source, destination));
        }
    }

    QFileInfo dstInfo(destination);
    QString backupPath;
    if (dstInfo.exists()) {
        if (!overwrite) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Cannot move: Destination path already exists: %1").arg(destination));
        }

        backupPath = destination + QStringLiteral(".bak_%1").arg(QDateTime::currentMSecsSinceEpoch());
        if (exists(backupPath)) {
            remove(backupPath);
        }

        QDir dir;
        if (!dir.rename(destination, backupPath)) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QCoreApplication::translate("FileSystem", "Cannot move: Failed to create temporary backup for existing destination: %1").arg(destination));
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
        copy(source, destination, overwrite, token);
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

    if (token.isCancelled()) {
        if (exists(destination)) {
            remove(destination);
        }
        if (!backupPath.isEmpty() && exists(backupPath)) {
            dir.rename(backupPath, destination);
        }
        throw Core::Error::Exception(
            Core::Error::ErrorCode::Cancelled,
            QCoreApplication::translate("FileSystem", "Move cancelled"));
    }

    // Copy succeeded: remove source. If removal fails, the destination copy is
    // intentionally KEPT: removeRecursively may have partially deleted the source,
    // making the destination the only complete copy. The failure is reported with
    // all residual paths so the caller can decide how to recover.
    try {
        remove(source);
    } catch (const Core::Error::Exception& ex) {
        throw Core::Error::Exception(
            ex.errorCode(),
            QCoreApplication::translate("FileSystem", "Move partially completed: destination copy kept, source removal failed"),
            QStringLiteral("destination=%1 source=%2 backup=%3 cause=%4")
                .arg(destination,
                     source,
                     backupPath.isEmpty() ? QStringLiteral("<none>") : backupPath,
                     ex.details().isEmpty() ? ex.message() : ex.details()));
    } catch (const std::exception& ex) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("FileSystem", "Move partially completed: destination copy kept, source removal failed"),
            QStringLiteral("destination=%1 source=%2 backup=%3 cause=%4")
                .arg(destination,
                     source,
                     backupPath.isEmpty() ? QStringLiteral("<none>") : backupPath,
                     QString::fromUtf8(ex.what())));
    }

    if (!backupPath.isEmpty() && exists(backupPath)) {
        remove(backupPath);
    }
}

QByteArray FileSystem::readAll(const QString& filePath) {
    if (filePath.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot read file: Path is empty"));
    }

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("FileSystem", "Cannot read file: File does not exist: %1").arg(filePath));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::PermissionDenied,
            QCoreApplication::translate("FileSystem", "Cannot open file for reading: %1 (%2)").arg(filePath, file.errorString()));
    }

    return file.readAll();
}

void FileSystem::writeAll(const QString& filePath, const QByteArray& data) {
    if (filePath.isEmpty()) {
        throw Core::Error::Exception(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("FileSystem", "Cannot write file: Path is empty"));
    }

    AtomicFile::writeAtomic(filePath, data);
}

} // namespace Core::FileSystem
