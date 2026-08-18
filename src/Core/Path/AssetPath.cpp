#include "AssetPath.h"
#include <QFileInfo>
#include <QStringList>

namespace Core::Path {

AssetPath::AssetPath()
    : m_path()
    , m_isValid(false) {
}

AssetPath::AssetPath(const QString& path) {
    processPath(path);
}

void AssetPath::processPath(const QString& path) {
    m_path.clear();
    m_isValid = false;

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
    m_isValid = true;
}

bool AssetPath::isEmpty() const {
    return m_path.isEmpty();
}

bool AssetPath::isValid() const {
    return m_isValid;
}

QString AssetPath::fileName() const {
    if (!m_isValid) {
        return QString();
    }
    return QFileInfo(m_path).fileName();
}

QString AssetPath::extension() const {
    if (!m_isValid) {
        return QString();
    }
    return QFileInfo(m_path).suffix();
}

QString AssetPath::directory() const {
    if (!m_isValid) {
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

bool AssetPath::operator==(const AssetPath& other) const {
    return m_path == other.m_path && m_isValid == other.m_isValid;
}

bool AssetPath::operator!=(const AssetPath& other) const {
    return !(*this == other);
}

} // namespace Core::Path
