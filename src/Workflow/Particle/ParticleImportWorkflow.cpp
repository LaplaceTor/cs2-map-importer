#include <QCoreApplication>
#include "ParticleImportWorkflow.h"

#include "Domain/Tool/Source1ImportTool.h"
#include "Domain/Tool/ResourceCompilerTool.h"
#include "Domain/Tool/Cs2PathLayout.h"
#include "Domain/Tool/ToolErrors.h"
#include <QFile>

namespace Workflow::Particle {

namespace {

/**
 * @brief Best-effort removal of half-finished artifacts left behind by a cancelled
 * or failed workflow. The workflow owns the lifecycle of the files it generated,
 * so a cancelled import must not leave partial .vpcf/.vpcf_c assets in the addon
 * content directory.
 */
void cleanupGeneratedArtifacts(const QStringList& vpcfPaths, const Common::ImportContext& context)
{
    int removed = 0;
    for (const QString& path : vpcfPaths) {
        QFile generated(path);
        if (generated.exists()) {
            if (generated.remove()) {
                removed++;
            } else {
                context.warning(QCoreApplication::translate("ParticleImportWorkflow", "Failed to clean up generated artifact: %1 (%2)")
                    .arg(path, generated.errorString()));
            }
        }
        QFile compiled(path + QStringLiteral("_c"));
        if (compiled.exists()) {
            if (compiled.remove()) {
                removed++;
            } else {
                context.warning(QCoreApplication::translate("ParticleImportWorkflow", "Failed to clean up generated artifact: %1 (%2)")
                    .arg(compiled.fileName(), compiled.errorString()));
            }
        }
    }
    if (removed > 0) {
        context.info(QCoreApplication::translate("ParticleImportWorkflow", "Cleaned up %1 half-finished artifact(s) after cancelled/failed import").arg(removed));
    }
}

} // namespace

Core::Result<ParticleImportWorkflowResult> ParticleImportWorkflow::execute(
    const ParticleImportOptions& options,
    const Common::ImportContext& context)
{
    const QString trimmedAddon = options.addonName.trimmed();
    const int toolTimeoutMs = options.toolTimeoutMs > 0 ? options.toolTimeoutMs : 120000;
    // Deterministic working directory anchor: relative artifact paths reported by the
    // tools on stdout are resolved against the CS2 game directory.
    const auto toolWorkingDirectory = Domain::Tool::Cs2PathLayout::gameDirectory(options.cs2BaseDir);

    // Step 1: Convert PCF via Source1ImportTool
    auto convertResult = context.runStep(
        QCoreApplication::translate("ParticleImportWorkflow", "Converting PCF with source1import"),
        0.5,
        [&]() -> Core::Result<Domain::Tool::Source1ImportToolResult> {
            Domain::Tool::Source1ImportOptions s1Options;
            s1Options.source1GameInfoDir = options.s1GameInfoDir;
            s1Options.addonName = trimmedAddon;
            s1Options.inputFilePath = options.sourcePcfPath;
            s1Options.allowDepthBlend = options.allowDepthBlend;
            s1Options.disableDiffuse = options.disableDiffuse;
            s1Options.isCsgo = options.isCsgo;
            s1Options.timeoutMs = toolTimeoutMs;
            s1Options.workingDirectory = toolWorkingDirectory;
            s1Options.cancellationToken = context.token();

            auto s1Result = Domain::Tool::Source1ImportTool::importAsset(
                options.source1ImportExe, s1Options, context.loggingContext());
            if (s1Result.isFailure()) {
                return Core::Result<Domain::Tool::Source1ImportToolResult>::failure(
                    s1Result.error(),
                    QCoreApplication::translate("ParticleImportWorkflow", "PCF conversion failed: %1").arg(s1Result.message()));
            }

            const auto& s1Data = s1Result.value();
            if (s1Data.generatedVpcfPaths.isEmpty()) {
                return Core::Result<Domain::Tool::Source1ImportToolResult>::failure(
                    Domain::Tool::ToolErrors::noMatchingFiles(
                        options.sourcePcfPath.toString(),
                        s1Data.rawOutput),
                    QCoreApplication::translate("ParticleImportWorkflow", "No .vpcf files generated from PCF conversion"));
            }

            return s1Result;
        });

    if (convertResult.isCancelled()) {
        return Core::Result<ParticleImportWorkflowResult>::cancelled(convertResult.message());
    }
    if (convertResult.isFailure()) {
        return Core::Result<ParticleImportWorkflowResult>::failure(convertResult.error(), convertResult.message());
    }

    const auto& s1Data = convertResult.value();
    ParticleImportWorkflowResult workflowResult;
    workflowResult.generatedVpcfFiles = s1Data.generatedVpcfPaths;
    workflowResult.totalConverted = s1Data.generatedVpcfPaths.size();

    // Step 2: Compile generated resources via ResourceCompilerTool
    auto compileResult = context.runStep(
        QCoreApplication::translate("ParticleImportWorkflow", "Compiling generated .vpcf resources"),
        1.0,
        [&]() -> Core::Result<Domain::Tool::ResourceCompilerToolResult> {
            Domain::Tool::ResourceCompilerOptions rcOptions;
            rcOptions.gameDir = Domain::Tool::Cs2PathLayout::gameDirectory(options.cs2BaseDir);
            rcOptions.inputFiles = workflowResult.generatedVpcfFiles;
            rcOptions.forceCompile = true;
            rcOptions.verbose = true;
            rcOptions.timeoutMs = toolTimeoutMs;
            rcOptions.cancellationToken = context.token();

            auto rcResult = Domain::Tool::ResourceCompilerTool::compileResources(
                options.resourceCompilerExe, rcOptions, context.loggingContext());
            if (rcResult.isFailure()) {
                return Core::Result<Domain::Tool::ResourceCompilerToolResult>::failure(
                    rcResult.error(),
                    QCoreApplication::translate("ParticleImportWorkflow", "Resource compilation failed: %1").arg(rcResult.message()));
            }

            return rcResult;
        });

    if (compileResult.isCancelled()) {
        cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
        return Core::Result<ParticleImportWorkflowResult>::cancelled(compileResult.message());
    }
    if (compileResult.isFailure()) {
        cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
        return Core::Result<ParticleImportWorkflowResult>::failure(compileResult.error(), compileResult.message());
    }

    const auto& rcData = compileResult.value();
    workflowResult.compiledVpcfCFiles = rcData.compiledVpcfCPaths;
    workflowResult.totalCompiled = !rcData.compiledVpcfCPaths.isEmpty()
        ? rcData.compiledVpcfCPaths.size()
        : rcData.compiledCount;

    return Core::Result<ParticleImportWorkflowResult>::success(
        workflowResult,
        QCoreApplication::translate("ParticleImportWorkflow", "Particle import and compilation completed successfully"));
}

} // namespace Workflow::Particle
