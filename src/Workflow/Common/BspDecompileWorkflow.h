#pragma once

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/BspDecompiler.h"
#include "Workflow/Common/ImportContext.h"

namespace Workflow::Common {

/**
 * @brief Thin workflow orchestrator bridging ImportContext to Domain::Bsp::BspDecompiler.
 *
 * Wires cooperative cancellation and structured logging from ImportContext into
 * BspDecompileConfig, logs start/finish summary milestones, and delegates directly
 * to Domain::Bsp::BspDecompiler::decompile.
 */
class BspDecompileWorkflow {
public:
    static Core::Result<Domain::Bsp::BspDecompileResult> decompile(
        const Core::Path::FilesystemPath& bspPath,
        const Core::Path::FilesystemPath& outputVmfPath,
        ImportContext& context,
        Domain::Bsp::BspDecompileConfig config = {},
        const Core::Path::FilesystemPath& expectedBaseDir = {});
};

} // namespace Workflow::Common
