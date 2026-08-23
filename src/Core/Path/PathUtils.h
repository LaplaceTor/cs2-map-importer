#pragma once

#include "FilesystemPath.h"
#include <QDir>
#include <QFileInfo>
#include <QString>

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

    /**
     * @brief Sanitizes a filename by replacing illegal host filesystem characters (< > : " / \ | ? * and control chars).
     */
    static QString sanitizeFilename(const QString& filename, const QString& replacement = QStringLiteral("_")) {
        if (filename.isEmpty()) return QString();
        QString result;
        result.reserve(filename.size());
        for (const QChar& c : filename) {
            const ushort u = c.unicode();
            if (u < 32 || c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char(':') ||
                c == QLatin1Char('"') || c == QLatin1Char('/') || c == QLatin1Char('\\') ||
                c == QLatin1Char('|') || c == QLatin1Char('?') || c == QLatin1Char('*')) {
                result.append(replacement);
            } else {
                result.append(c);
            }
        }
        return result;
    }
};

} // namespace Core::Path

