#pragma once

#include <QString>
#include <QStringList>

#include <optional>

#include "Core/Result/Result.h"
#include "Domain/Material/VtfCodec.h"

namespace Domain::Material {

/**
 * @brief Compilation presets for explicit Source 2 .vtex DMX assets.
 */
enum class VtexPreset : int {
    Sprite2DAlpha,
    Sprite2DOpaque,
    DualInputColorTrans,
    NormalMap2D,
    Cubemap2D,
    Volume3DLut,
};

/**
 * @brief Input parameters for generating a self-contained DMX KeyValues2 .vtex document.
 */
struct VtexBuildRequest {
    VtexPreset preset = VtexPreset::Sprite2DAlpha;
    QString inputRelativePath;
    QString secondaryRelativePath;
    std::optional<VtfHeaderInfo> sourceVtfHeader;
    bool forceNoLod = false;
};

/**
 * @brief Pure in-memory builder for Source 2 DMX .vtex descriptors and .mks sprite-sheet scripts.
 *
 * Used exclusively by explicit .vtex asset pipelines (ExplicitSprite, SpriteCard,
 * standalone cubemaps, standalone 3D LUTs, and explicit normal maps).
 */
class VtexBuilder {
public:
    /**
     * @brief Builds a self-contained DMX KeyValues2 .vtex document for the requested preset.
     *
     * Validates that all input paths are relative forward-slash paths without drive colons,
     * backslashes, leading slashes, or parent-directory traversal.
     */
    static Core::Result<QString> buildVtex(const VtexBuildRequest& request);

    /**
     * @brief Builds a flat-packed .mks sequence script for a multi-frame sprite VTF.
     */
    static Core::Result<QString> buildMultiFrameMks(
        const QStringList& frameFileNames,
        bool loop = true);

    /**
     * @brief Builds a flat-packed .mks sprite-sheet script from decoded VTF_RSRC_SHEET metadata.
     */
    static Core::Result<QString> buildSheetMks(
        const QString& sheetImageFileName,
        int imageWidth,
        int imageHeight,
        const VtfSheetData& sheetData);
};

} // namespace Domain::Material
