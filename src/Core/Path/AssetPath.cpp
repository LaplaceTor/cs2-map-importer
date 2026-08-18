#include "AssetPath.h"
#include <QFileInfo>

namespace Core::Path {

AssetPath::AssetPath()
    : m_rawPath() {
}

AssetPath::AssetPath(const QString& path)
    : m_rawPath(path) {
}

QString AssetPath::rawPath() const {
    return m_rawPath;
}

QString AssetPath::absolutePath() const {
    if (m_rawPath.isEmpty()) {
        return QString();
    }
    return QFileInfo(m_rawPath).absoluteFilePath();
}

QString AssetPath::fileName() const {
    return PathUtils::filename(m_rawPath);
}

QString AssetPath::extension() const {
    return PathUtils::extension(m_rawPath);
}

bool AssetPath::exists() const {
    if (m_rawPath.isEmpty()) {
        return false;
    }
    return QFileInfo(m_rawPath).exists();
}

bool AssetPath::isEmpty() const {
    return m_rawPath.isEmpty();
}

bool AssetPath::isValid() const {
    return !m_rawPath.isEmpty();
}

} // namespace Core::Path
