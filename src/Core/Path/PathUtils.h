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

    /**
     * @brief Sanitizes an asset/material path for Source 2 / CS2 resource compiler compatibility.
     * Replaces characters such as '{', '}', '^', '#', '`', '|', '?', '*', ':', '"', '<', '>' with a safe replacement.
     */
    static QString sanitizeAssetName(const QString& assetName, const QString& replacement = QStringLiteral("_")) {
        if (assetName.isEmpty()) return QString();
        QString result;
        result.reserve(assetName.size());
        for (const QChar& c : assetName) {
            const ushort u = c.unicode();
            if (u < 32 || c == QLatin1Char('{') || c == QLatin1Char('}') || c == QLatin1Char('^') ||
                c == QLatin1Char('#') || c == QLatin1Char('`') || c == QLatin1Char('|') ||
                c == QLatin1Char('?') || c == QLatin1Char('*') || c == QLatin1Char(':') ||
                c == QLatin1Char('"') || c == QLatin1Char('<') || c == QLatin1Char('>')) {
                result.append(replacement);
            } else {
                result.append(c);
            }
        }
        return result;
    }
};

} // namespace Core::Path
