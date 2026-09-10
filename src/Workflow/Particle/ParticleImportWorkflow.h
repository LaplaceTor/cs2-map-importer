#pragma once

#include "ParticleImportOptions.h"
#include "Workflow/Common/ImportContext.h"
#include "Core/Result/Result.h"

namespace Workflow::Particle {

/**
 * @brief Orchestrates the particle import and compilation pipeline.
 *
 * Execution Pipeline:
 * 1. Conversion: executes Source1ImportTool to convert the PCF into Source 2 .vpcf files.
 * 2. Compilation: executes ResourceCompilerTool to compile generated .vpcf files into .vpcf_c files.
 * 3. Result Aggregation: returns single-layer Core::Result<ParticleImportWorkflowResult>.
 *
 * Cooperative cancellation and structured logging are unified via Common::ImportContext.
 */
class ParticleImportWorkflow {
public:
    ParticleImportWorkflow() = default;

    /**
     * @brief Executes the complete particle import and compilation workflow.
     */
    Core::Result<ParticleImportWorkflowResult> execute(
        const ParticleImportOptions& options,
        const Common::ImportContext& context = Common::ImportContext{});

    /**
     * @brief Static convenience helper for running the workflow.
     */
    static Core::Result<ParticleImportWorkflowResult> run(
        const ParticleImportOptions& options,
        const Common::ImportContext& context = Common::ImportContext{})
    {
        ParticleImportWorkflow workflow;
        return workflow.execute(options, context);
    }
};

} // namespace Workflow::Particle
