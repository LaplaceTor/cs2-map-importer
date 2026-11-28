#pragma once

#include "Core/Result/Result.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Material/MaterialConvertOptions.h"

namespace Workflow::Material {

/**
 * @brief Orchestrates in-process Source 1 VMT/VTF -> CS2 VMAT/VTEX/MKS/PNG conversion
 *        and optional batch compilation via ResourceCompilerTool.
 *
 * Execution Pipeline:
 * 1. Resource Discovery & Deduplication: locates VMT/VTF sources across loose directories
 *    and VPK archives via AssetLocateStrategy using a single PackArchivePool; skips native
 *    CS2 assets when !overwriteNativeAssets.
 * 2. Three Conversion Tracks:
 *    - Regular VMTs -> PNG textures + .vmat (never emits .mks).
 *    - Explicit Sprite VMTs -> Straight RGBA8888 PNG(s) (+ .mks for multi-frame/sheet) + .vtex.
 *    - Skyboxes -> 6-face HDR/LDR decode & preprocess -> 4x3 PNG + sky.vfx .vmat.
 * 3. Batch Compilation: compiles all newly generated .vmat and .vtex assets in a single
 *    ResourceCompilerTool::compileResources call when compileResources is true.
 * 4. Cooperative Cancellation & Rollback: cleans up all newly created files on cancellation
 *    or fatal error when cleanupOnFailure is true.
 */
class MaterialConvertWorkflow {
public:
    MaterialConvertWorkflow() = default;

    /**
     * @brief Executes the material conversion and optional compilation workflow.
     */
    Core::Result<MaterialConvertResult> execute(
        const MaterialConvertOptions& options,
        const Common::ImportContext& context = Common::ImportContext{});

    /**
     * @brief Static convenience helper for running the workflow.
     */
    static Core::Result<MaterialConvertResult> run(
        const MaterialConvertOptions& options,
        const Common::ImportContext& context = Common::ImportContext{})
    {
        MaterialConvertWorkflow workflow;
        return workflow.execute(options, context);
    }
};

} // namespace Workflow::Material
