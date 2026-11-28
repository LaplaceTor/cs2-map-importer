#pragma once

#include <QString>
#include <QStringList>

#include "Application/Common/BaseImportDTOs.h"

namespace Application::Material {

/**
 * @brief User-level request DTO for Source 1 material, sprite, and skybox conversion.
 */
struct MaterialConvertRequest : public Common::BaseImportRequest {
    QStringList materialRelativePaths;
    QStringList explicitSpriteVmtPaths;
    QStringList skyboxNames;
    bool preserveDetailViaComplex = true;
    bool overwriteNativeAssets = false;
    bool compileResources = true;

    bool operator==(const MaterialConvertRequest& other) const = default;
};

/**
 * @brief User-level outcome DTO of a material conversion and compilation operation.
 */
struct MaterialConvertResultDTO {
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

    bool operator==(const MaterialConvertResultDTO& other) const = default;
};

} // namespace Application::Material
