#pragma once

#include <QHash>
#include <QString>

#include "Core/Result/Result.h"
#include "Domain/Material/MaterialConvertTypes.h"
#include "Domain/Material/VmtDocument.h"

namespace Domain::Material {

/**
 * @brief Two-stage pure Source 1 VMT -> CS2 VMAT translator.
 *
 * Stage 1 (`plan`) routes the exact normalized Source 1 shader name to a modern CS2
 * TargetShader and produces the deterministic list of texture exports required.
 * Stage 2 (`buildVmat`) serializes the final `Layer0 { ... }` `.vmat` document via
 * `Core::KeyValues::KeyValuesWriter` with explicit roughness decisions and static
 * FeatureRule arbitration.
 */
class MaterialTranslator {
public:
    /**
     * @brief Plans shader routing, texture synthesis items, and explicit `.vtex` preset.
     */
    [[nodiscard]] static MaterialPlan plan(
        const VmtMaterial& vmt,
        const QHash<QString, VtfProbeInfo>& vtfProbes,
        const MaterialTranslatePolicy& policy = {},
        MaterialUsage usage = MaterialUsage::WorldSurface);

    /**
     * @brief Builds the `.vmat` text from a parsed VMT, its plan, and exported PNG paths
     *        keyed by `TextureExportItem::role`.
     */
    static Core::Result<VmatBuildOutput> buildVmat(
        const VmtMaterial& vmt,
        const MaterialPlan& plan,
        const QHash<QString, QString>& exportedPaths,
        const MaterialTranslatePolicy& policy = {});
};

} // namespace Domain::Material
