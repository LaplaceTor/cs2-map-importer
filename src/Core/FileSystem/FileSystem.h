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
    static bool Exists(const QString& path) { return exists(path); }

    static bool isFile(const QString& path);
    static bool IsFile(const QString& path) { return isFile(path); }

    static bool isDirectory(const QString& path);
    static bool IsDirectory(const QString& path) { return isDirectory(path); }

    static bool createDirectory(const QString& path);
    static bool CreateDirectory(const QString& path) { return createDirectory(path); }

    static bool remove(const QString& path);
    static bool Remove(const QString& path) { return remove(path); }

    static bool copy(const QString& source, const QString& destination, bool overwrite = true);
    static bool Copy(const QString& source, const QString& destination, bool overwrite = true) {
        return copy(source, destination, overwrite);
    }

    static bool move(const QString& source, const QString& destination, bool overwrite = true);
    static bool Move(const QString& source, const QString& destination, bool overwrite = true) {
        return move(source, destination, overwrite);
    }

    static QByteArray readAll(const QString& filePath);
    static QByteArray ReadAll(const QString& filePath) { return readAll(filePath); }

    static bool writeAll(const QString& filePath, const QByteArray& data);
    static bool WriteAll(const QString& filePath, const QByteArray& data) {
        return writeAll(filePath, data);
    }

private:
    static bool copyDirectoryHelper(const QString& source, const QString& destination, bool overwrite);
};

} // namespace Core::FileSystem
