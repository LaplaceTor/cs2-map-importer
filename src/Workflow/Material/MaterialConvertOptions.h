#pragma once

#include <memory>
#include <vector>

#include <QStringList>

#include "Core/Path/FilesystemPath.h"
#include "Domain/Game/SearchTarget.h"
#include "Domain/Material/MaterialConvertTypes.h"

namespace Domain::Package {
class PackArchivePool;
class VpkIndex;
}

namespace Workflow::Material {

/**
 * @brief Input options for MaterialConvertWorkflow::execute.
 */
struct MaterialConvertOptions {
    std::vector<Domain::Game::SearchTarget> searchTargets;
    Core::Path::FilesystemPath destContentDir;
    Core::Path::FilesystemPath cs2BaseDir;
    std::shared_ptr<const Domain::Package::VpkIndex> vpkIndex;
    std::shared_ptr<const Domain::Package::VpkIndex> cs2Index;
    Domain::Package::PackArchivePool* archivePool = nullptr;

    QStringList materialRelativePaths;
    QStringList explicitSpriteVmtPaths;
    QStringList skyboxNames;

    Domain::Material::MaterialTranslatePolicy translatePolicy;
    bool overwriteNativeAssets = false;
    bool compileResources = true;
    bool cleanupOnFailure = true;
};

/**
 * @brief Aggregated result statistics and output paths from MaterialConvertWorkflow::execute.
 */
struct MaterialConvertResult {
    int totalRequested = 0;
    int convertedMaterials = 0;
    int convertedSprites = 0;
    int convertedSkyboxes = 0;
    int skippedNative = 0;
    int skippedUnsupported = 0;
    int failedCount = 0;
    int compiledCount = 0;

    QStringList generatedFiles;
    QStringList compiledAssetPaths;
    QStringList warnings;
    QStringList errors;
};

} // namespace Workflow::Material
