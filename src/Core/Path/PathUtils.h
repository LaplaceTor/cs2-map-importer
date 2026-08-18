#pragma once

#include "AssetPath.h"
#include "FilesystemPath.h"
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <optional>

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

    static FilesystemPath resolveAssetPath(const FilesystemPath& baseDir, const AssetPath& assetPath) {
        if (!baseDir.isValid() || !assetPath.isValid()) {
            return FilesystemPath();
        }
        QString baseStr = baseDir.toString();
        if (!baseStr.endsWith(QLatin1Char('/')) && !baseStr.endsWith(QLatin1Char('\\'))) {
            baseStr += QLatin1Char('/');
        }
        return FilesystemPath(baseStr + assetPath.toString());
    }

    static std::optional<AssetPath> makeAssetPath(const FilesystemPath& baseDir, const FilesystemPath& filePath) {
        if (!baseDir.isValid() || !filePath.isValid()) {
            return std::nullopt;
        }

        QString canonicalBase = QDir(baseDir.toString()).canonicalPath();
        QString canonicalFile = QDir(filePath.toString()).canonicalPath();

        if (canonicalBase.isEmpty() || canonicalFile.isEmpty()) {
            canonicalBase = QDir::cleanPath(baseDir.toString());
            canonicalFile = QDir::cleanPath(filePath.toString());
        }

        if (!canonicalBase.endsWith(QLatin1Char('/')) && !canonicalBase.endsWith(QLatin1Char('\\'))) {
            canonicalBase += QLatin1Char('/');
        }

        if (canonicalFile == canonicalBase.left(canonicalBase.length() - 1)) {
            return std::nullopt;
        }

        if (!canonicalFile.startsWith(canonicalBase, Qt::CaseInsensitive)) {
            return std::nullopt;
        }

        QString rel = canonicalFile.mid(canonicalBase.length());
        AssetPath candidate(rel);
        if (candidate.isValid()) {
            return candidate;
        }
        return std::nullopt;
    }
};

} // namespace Core::Path
