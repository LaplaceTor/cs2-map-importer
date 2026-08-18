#pragma once

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include "Core/Error/ImportException.h"
#include "Core/Error/ImportError.h"

namespace Core::FileSystem {

class FileSystem {
public:
    static bool exists(const QString& path);
    static bool isFile(const QString& path);
    static bool isDirectory(const QString& path);
    static bool createDirectory(const QString& path);
    static bool remove(const QString& path);

    /**
     * @brief Copies a file or directory from source to destination.
     *
     * For files: if destination exists and overwrite is true, destination is overwritten.
     * For directories: performs a recursive merge copy (creates target subdirectories if missing
     * and overwrites individual files within destination if overwrite is true).
     */
    static bool copy(const QString& source, const QString& destination, bool overwrite = true);

    static bool move(const QString& source, const QString& destination, bool overwrite = true);
    static QByteArray readAll(const QString& filePath);
    static bool writeAll(const QString& filePath, const QByteArray& data);

private:
    static bool copyDirectoryHelper(const QString& source, const QString& destination, bool overwrite);
};

} // namespace Core::FileSystem
