#pragma once

#include <QString>
#include <QFileInfo>
#include <QDir>

namespace Core::Path {

class PathUtils {
public:
    static QString normalize(const QString& path) {
        if (path.isEmpty()) return QString();
        return QDir::cleanPath(path);
    }

    static QString filename(const QString& path) {
        return QFileInfo(path).fileName();
    }

    static QString extension(const QString& path) {
        return QFileInfo(path).suffix();
    }

    static QString directory(const QString& path) {
        return QFileInfo(path).path();
    }

    static QString relativePath(const QString& path, const QString& baseDir) {
        return QDir(baseDir).relativeFilePath(path);
    }
};

} // namespace Core::Path
