#include <QCoreApplication>
#include "ParticleImportWorkflow.h"

#include "Domain/Tool/Source1ImportTool.h"
#include "Domain/Tool/ResourceCompilerTool.h"
#include "Domain/Tool/Cs2PathLayout.h"
#include "Domain/Tool/ToolErrors.h"
#include "Core/Error/ErrorCode.h"
#include "Core/Temp/TempFile.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

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
    if (options.sourcePcfPaths.empty()) {
        return Core::Result<ParticleImportWorkflowResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("ParticleImportWorkflow", "No source PCF files specified"));
    }

    const QString trimmedAddon = options.addonName.trimmed();
    const int toolTimeoutMs = options.toolTimeoutMs > 0 ? options.toolTimeoutMs : 120000;
    // Deterministic working directory anchor: relative artifact paths reported by the
    // tools on stdout are resolved against the CS2 game directory.
    const auto toolWorkingDirectory = Domain::Tool::Cs2PathLayout::gameDirectory(options.cs2BaseDir);

    const Core::Path::FilesystemPath s1ParticlesPath = options.s1GameInfoDir / QStringLiteral("particles");
    const QDir s1ParticlesDir(s1ParticlesPath.toString());
    const Core::Path::FilesystemPath baseParticlesPath = !options.source1GameDir.isEmpty()
        ? options.source1GameDir / QStringLiteral("particles")
        : Core::Path::FilesystemPath{};

    ParticleImportWorkflowResult workflowResult;
    const int totalPcfCount = static_cast<int>(options.sourcePcfPaths.size());

    // Step 1: Iterate through all PCF files and convert them via Source1ImportTool
    for (int i = 0; i < totalPcfCount; ++i) {
        if (context.isCancelled()) {
            cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
            return Core::Result<ParticleImportWorkflowResult>::cancelled(
                QCoreApplication::translate("ParticleImportWorkflow", "Particle import was cancelled"));
        }

        const auto& pcfPath = options.sourcePcfPaths[static_cast<size_t>(i)];
        const QString pcfFileName = QFileInfo(pcfPath.toString()).fileName();

        bool alreadyInS1Particles = pcfPath.isSubpathOf(s1ParticlesPath);
        if (!alreadyInS1Particles && !baseParticlesPath.isEmpty()) {
            alreadyInS1Particles = pcfPath.isSubpathOf(baseParticlesPath);
        }

        Core::Path::FilesystemPath effectivePcfPath = pcfPath;
        QString stagedTempFilePath;

        if (!alreadyInS1Particles) {
            if (!s1ParticlesDir.exists() && !s1ParticlesDir.mkpath(QStringLiteral("."))) {
                context.warning(QCoreApplication::translate("ParticleImportWorkflow", "Failed to create Source 1 particles directory: %1")
                    .arg(s1ParticlesDir.absolutePath()));
                workflowResult.failedPcfFiles.append(pcfPath.toString());
                workflowResult.totalFailed += 1;
                continue;
            }

            const QString targetPcfPathStr = s1ParticlesDir.filePath(pcfFileName);
            if (QFile::exists(targetPcfPathStr)) {
                QFile::remove(targetPcfPathStr);
            }
            if (!QFile::copy(pcfPath.toString(), targetPcfPathStr)) {
                context.warning(QCoreApplication::translate("ParticleImportWorkflow", "Failed to copy PCF file to Source 1 particles folder: %1")
                    .arg(targetPcfPathStr));
                workflowResult.failedPcfFiles.append(pcfPath.toString());
                workflowResult.totalFailed += 1;
                continue;
            }
            effectivePcfPath = Core::Path::FilesystemPath(targetPcfPathStr);
            stagedTempFilePath = targetPcfPathStr;
            context.info(QCoreApplication::translate("ParticleImportWorkflow", "Staged PCF file to Source 1 particles folder: %1").arg(targetPcfPathStr));
        } else {
            context.info(QCoreApplication::translate("ParticleImportWorkflow", "PCF file is already inside Source 1 particles folder: %1")
                .arg(pcfPath.toString()));
        }

        // Run conversion step for this PCF
        const double progress = 0.6 * (static_cast<double>(i + 1) / totalPcfCount);
        const QString stepName = totalPcfCount > 1
            ? QCoreApplication::translate("ParticleImportWorkflow", "Converting PCF (%1/%2): %3")
                .arg(i + 1).arg(totalPcfCount).arg(pcfFileName)
            : QCoreApplication::translate("ParticleImportWorkflow", "Converting PCF with source1import: %1")
                .arg(pcfFileName);

        auto convertResult = context.runStep(
            stepName,
            progress,
            [&]() -> Core::Result<Domain::Tool::Source1ImportToolResult> {
                Domain::Tool::Source1ImportOptions s1Options;
                s1Options.source1GameInfoDir = options.s1GameInfoDir;
                s1Options.addonName = trimmedAddon;
                s1Options.inputFilePath = effectivePcfPath;
                s1Options.allowDepthBlend = options.allowDepthBlend;
                s1Options.disableDiffuse = options.disableDiffuse;
                s1Options.isCsgo = options.isCsgo;
                s1Options.timeoutMs = toolTimeoutMs;
                s1Options.workingDirectory = toolWorkingDirectory;
                s1Options.cancellationToken = context.token();

                return Domain::Tool::Source1ImportTool::importAsset(
                    options.source1ImportExe, s1Options, context.loggingContext());
            });

        // Immediately clean up staged temporary PCF
        if (!stagedTempFilePath.isEmpty()) {
            if (QFile::exists(stagedTempFilePath)) {
                QFile::remove(stagedTempFilePath);
            }
        }

        if (convertResult.isCancelled()) {
            cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
            return Core::Result<ParticleImportWorkflowResult>::cancelled(convertResult.message());
        }

        if (convertResult.isSuccess()) {
            const auto& s1Data = convertResult.value();
            if (s1Data.generatedVpcfPaths.isEmpty()) {
                workflowResult.failedPcfFiles.append(pcfPath.toString());
                workflowResult.totalFailed += s1Data.failedCount > 0 ? s1Data.failedCount : 1;
                context.warning(QCoreApplication::translate("ParticleImportWorkflow", "No .vpcf files generated from PCF: %1").arg(pcfFileName));
            } else {
                for (const QString& vpcf : s1Data.generatedVpcfPaths) {
                    if (!workflowResult.generatedVpcfFiles.contains(vpcf)) {
                        workflowResult.generatedVpcfFiles.append(vpcf);
                    }
                }
                const int convertedCount = s1Data.generatedVpcfPaths.size();
                workflowResult.totalConverted += convertedCount;
                workflowResult.totalFailed += s1Data.failedCount;

                if (s1Data.failedCount > 0) {
                    context.warning(QCoreApplication::translate("ParticleImportWorkflow", "PCF '%1': %2 .vpcf generated, %3 failed")
                        .arg(pcfFileName).arg(convertedCount).arg(s1Data.failedCount));
                } else {
                    context.info(QCoreApplication::translate("ParticleImportWorkflow", "PCF '%1': %2 .vpcf generated")
                        .arg(pcfFileName).arg(convertedCount));
                }
            }
        } else {
            workflowResult.failedPcfFiles.append(pcfPath.toString());
            const auto& s1Data = convertResult.valueOr(Domain::Tool::Source1ImportToolResult{});
            workflowResult.totalFailed += s1Data.failedCount > 0 ? s1Data.failedCount : 1;
            context.warning(QCoreApplication::translate("ParticleImportWorkflow", "PCF conversion failed for '%1': %2")
                .arg(pcfFileName, convertResult.message()));
        }
    }

    if (context.isCancelled()) {
        cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
        return Core::Result<ParticleImportWorkflowResult>::cancelled(
            QCoreApplication::translate("ParticleImportWorkflow", "Particle import was cancelled"));
    }

    if (workflowResult.generatedVpcfFiles.isEmpty()) {
        return Core::Result<ParticleImportWorkflowResult>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("ParticleImportWorkflow", "No .vpcf files were generated from the selected PCF files"),
            workflowResult);
    }

    // Step 2: Compile all generated resources via ResourceCompilerTool in a single batch
    auto compileResult = context.runStep(
        QCoreApplication::translate("ParticleImportWorkflow", "Compiling %1 generated .vpcf resource(s)")
            .arg(workflowResult.generatedVpcfFiles.size()),
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
        return Core::Result<ParticleImportWorkflowResult>::failure(compileResult.error(), compileResult.message(), workflowResult);
    }

    const auto& rcData = compileResult.value();
    workflowResult.compiledVpcfCFiles = rcData.compiledVpcfCPaths;
    workflowResult.totalCompiled = !rcData.compiledVpcfCPaths.isEmpty()
        ? rcData.compiledVpcfCPaths.size()
        : rcData.compiledCount;
    workflowResult.totalFailed += rcData.failedCount;

    if (workflowResult.totalCompiled == 0) {
        cleanupGeneratedArtifacts(workflowResult.generatedVpcfFiles, context);
        return Core::Result<ParticleImportWorkflowResult>::failure(
            Core::Error::ErrorCode::OperationFailed,
            QCoreApplication::translate("ParticleImportWorkflow", "No particle resources were successfully compiled"),
            workflowResult);
    }

    return Core::Result<ParticleImportWorkflowResult>::success(
        workflowResult,
        QCoreApplication::translate("ParticleImportWorkflow", "Particle import and compilation completed successfully"));
}

} // namespace Workflow::Particle
