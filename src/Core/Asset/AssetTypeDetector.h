#pragma once

#include "AssetType.h"
#include <QString>

namespace Core::Path {
class AssetPath;
class FilesystemPath;
}

namespace Core::Asset {

class AssetTypeDetector {
public:
    static AssetType detect(const Core::Path::AssetPath& path);
    static AssetType detect(const Core::Path::FilesystemPath& path);
    static AssetType detect(const QString& path);
    static AssetType detectFromExtension(const QString& ext);
};

} // namespace Core::Asset
