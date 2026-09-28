#pragma once

#include <QString>
#include "Core/Path/FilesystemPath.h"

namespace Domain::Asset {

/**
 * @brief Represents the discovery location of an asset across search targets.
 */
struct AssetLocation {
    /** Target path (directory or VPK) where the asset resides. */
    Core::Path::FilesystemPath sourceTargetPath;
    /** Game-relative normalized asset path. */
    QString relativePath;
    /** True if found inside a VPK archive, false if on disk as a loose file. */
    bool isInsidePack = false;
    /** Full filesystem path on disk if it is a loose file. */
    Core::Path::FilesystemPath looseFilePath;
};

} // namespace Domain::Asset
