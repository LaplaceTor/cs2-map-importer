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

AssetType AssetPath::type() const {
    return detectType(extension());
}

bool AssetPath::isEmpty() const {
    return m_rawPath.isEmpty();
}

bool AssetPath::isValid() const {
    return !m_rawPath.isEmpty();
}

AssetType AssetPath::detectType(const QString& ext) {
    const QString lowerExt = ext.toLower();

    if (lowerExt == QStringLiteral("mdl") || lowerExt == QStringLiteral("vmdl") || lowerExt == QStringLiteral("smd") || lowerExt == QStringLiteral("fbx")) {
        return AssetType::Model;
    }
    if (lowerExt == QStringLiteral("pcf") || lowerExt == QStringLiteral("vpcf")) {
        return AssetType::Particle;
    }
    if (lowerExt == QStringLiteral("vmt") || lowerExt == QStringLiteral("vmat") || lowerExt == QStringLiteral("vtf")) {
        return AssetType::Material;
    }
    if (lowerExt == QStringLiteral("vmf") || lowerExt == QStringLiteral("bsp") || lowerExt == QStringLiteral("vmap")) {
        return AssetType::Map;
    }

    return AssetType::Unknown;
}

} // namespace Core::Path
