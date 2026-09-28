#pragma once

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "Core/Async/CancellationToken.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Error/Exception.h"

namespace Core::FileSystem {

class FileSystem {
public:
    static bool exists(const QString& path);
    static bool isFile(const QString& path);
    static bool isDirectory(const QString& path);

    static void createDirectory(const QString& path);
    static void remove(const QString& path);

    /**
     * @brief Copies a file or directory from source to destination.
     *
     * For files: if destination exists and overwrite is true, destination is overwritten.
     * For directories: performs a recursive merge copy (creates target subdirectories if missing
     * and overwrites individual files within destination if overwrite is true).
     *
     * Supports cooperative cancellation via CancellationToken. Chunked file copy cleans up
     * partially written destination files on cancellation.
     *
     * Throws Core::Error::Exception on failure or cancellation.
     */
    static void copy(
        const QString& source,
        const QString& destination,
        bool overwrite = true,
        const Core::Async::CancellationToken& token = {});

    static void move(
        const QString& source,
        const QString& destination,
        bool overwrite = true,
        const Core::Async::CancellationToken& token = {});
    static QByteArray readAll(const QString& filePath);
    static void writeAll(const QString& filePath, const QByteArray& data);

private:
    static void copyDirectoryHelper(
        const QString& source,
        const QString& destination,
        bool overwrite,
        const Core::Async::CancellationToken& token);
};

} // namespace Core::FileSystem
