#include "AssetTypeDetector.h"
#include "Core/Path/AssetPath.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Path/PathUtils.h"

namespace Core::Asset {

AssetType AssetTypeDetector::detect(const Core::Path::AssetPath& path) {
    return detectFromExtension(path.extension());
}

AssetType AssetTypeDetector::detect(const Core::Path::FilesystemPath& path) {
    return detectFromExtension(path.extension());
}

AssetType AssetTypeDetector::detect(const QString& path) {
    return detectFromExtension(Path::PathUtils::extension(path));
}

AssetType AssetTypeDetector::detectFromExtension(const QString& ext) {
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

} // namespace Core::Asset
