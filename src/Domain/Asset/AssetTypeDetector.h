#pragma once

#include "Domain/Asset/AssetType.h"
#include <QString>

namespace Core::Path {
class FilesystemPath;
}

namespace Domain::Asset {

class AssetPath;

class AssetTypeDetector {
public:
    static AssetType detect(const AssetPath& path);
    static AssetType detect(const Core::Path::FilesystemPath& path);
    static AssetType detect(const QString& path);
    static AssetType detectFromExtension(const QString& ext);
};

} // namespace Domain::Asset

