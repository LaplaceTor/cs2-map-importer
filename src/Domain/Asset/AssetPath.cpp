#include "AssetPath.h"
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace Domain::Asset {

AssetPath::AssetPath()
    : m_path() {
}

AssetPath::AssetPath(const QString& path) {
    processPath(path);
}

void AssetPath::processPath(const QString& path) {
    m_path.clear();

    if (path.isEmpty()) {
        return;
    }

    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Must not be an absolute path or contain drive letters / schemes
    if (normalized.startsWith(QLatin1Char('/')) || normalized.contains(QLatin1Char(':'))) {
        return;
    }

    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    if (parts.isEmpty()) {
        return;
    }

    for (const QString& part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
            return;
        }
    }

    m_path = parts.join(QLatin1Char('/'));
}

bool AssetPath::isEmpty() const {
    return m_path.isEmpty();
}

bool AssetPath::isValid() const {
    return !m_path.isEmpty();
}

QString AssetPath::fileName() const {
    if (!isValid()) {
        return QString();
    }
    return QFileInfo(m_path).fileName();
}

QString AssetPath::extension() const {
    if (!isValid()) {
        return QString();
    }
    return QFileInfo(m_path).suffix();
}

QString AssetPath::directory() const {
    if (!isValid()) {
        return QString();
    }
    int lastSlash = m_path.lastIndexOf(QLatin1Char('/'));
    if (lastSlash == -1) {
        return QString();
    }
    return m_path.left(lastSlash);
}

QString AssetPath::toString() const {
    return m_path;
}

Core::Path::FilesystemPath AssetPath::toFilesystemPath(const Core::Path::FilesystemPath& baseDir) const {
    return resolve(baseDir);
}

Core::Path::FilesystemPath AssetPath::resolve(const Core::Path::FilesystemPath& baseDir) const {
    if (!baseDir.isValid() || !isValid()) {
        return Core::Path::FilesystemPath();
    }
    return Core::Path::FilesystemPath(
        QDir(baseDir.toString()).filePath(m_path)
    );
}

std::optional<AssetPath> AssetPath::fromFilesystemPath(const Core::Path::FilesystemPath& baseDir,
                                                      const Core::Path::FilesystemPath& filePath) {
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

QString AssetPath::sanitizeAssetName(const QString& assetName, const QString& replacement) {
    if (assetName.isEmpty()) {
        return QString();
    }
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

bool AssetPath::operator==(const AssetPath& other) const {
    return m_path == other.m_path;
}

bool AssetPath::operator!=(const AssetPath& other) const {
    return !(*this == other);
}

bool AssetPath::operator<(const AssetPath& other) const {
    return m_path < other.m_path;
}

} // namespace Domain::Asset

