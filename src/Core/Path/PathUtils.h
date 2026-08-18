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
        return FilesystemPath(
            QDir(baseDir.toString()).filePath(assetPath.toString())
        );
    }

    static std::optional<AssetPath> makeAssetPath(const FilesystemPath& baseDir, const FilesystemPath& filePath) {
        if (!baseDir.isValid() || !filePath.isValid()) {
            return std::nullopt;
        }

        QString absBase = QFileInfo(baseDir.toString()).absoluteFilePath();
        QString absFile = QFileInfo(filePath.toString()).absoluteFilePath();

        QString canonicalBase = QFileInfo(absBase).canonicalFilePath();
        if (canonicalBase.isEmpty()) {
            canonicalBase = QDir::cleanPath(absBase);
        }

        QString canonicalFile = QFileInfo(absFile).canonicalFilePath();
        if (canonicalFile.isEmpty()) {
            QString parentCanonical = QFileInfo(QFileInfo(absFile).path()).canonicalFilePath();
            if (!parentCanonical.isEmpty()) {
                canonicalFile = parentCanonical + QLatin1Char('/') + QFileInfo(absFile).fileName();
            } else {
                canonicalFile = QDir::cleanPath(absFile);
            }
        }

        if (!canonicalBase.endsWith(QLatin1Char('/'))) {
            canonicalBase += QLatin1Char('/');
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
