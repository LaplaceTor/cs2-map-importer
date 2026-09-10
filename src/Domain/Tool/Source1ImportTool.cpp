#include "Source1ImportTool.h"

#include "Source1ImportLogParser.h"
#include "ToolErrors.h"
#include "Core/Process/ProcessRunner.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Process/ProcessResult.h"

namespace Domain::Tool {

QStringList Source1ImportTool::buildArguments(const Source1ImportOptions& options)
{
    QStringList args;
    args << QStringLiteral("-retail");
    args << QStringLiteral("-nop4");
    args << QStringLiteral("-nop4sync");
    args << QStringLiteral("-src1gameinfodir") << options.source1GameInfoDir.toString();
    args << QStringLiteral("-s2addon") << options.addonName;
    args << QStringLiteral("-game") << QStringLiteral("csgo");
    if (options.allowDepthBlend) {
        args << QStringLiteral("-particle_allow_depth_blend");
    }
    if (options.disableDiffuse) {
        args << QStringLiteral("-particle_disable_diffuse");
    }
    args << options.inputFilePath.toString();
    return args;
}

Core::Result<Source1ImportToolResult> Source1ImportTool::importAsset(
    const Core::Path::FilesystemPath& toolBinaryPath,
    const Source1ImportOptions& options,
    Core::Logging::TaskLoggingContext* taskCtx)
{
    if (toolBinaryPath.isEmpty() || !toolBinaryPath.isValid()) {
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::executableNotFound(toolBinaryPath.toString()),
            QStringLiteral("Source 1 导入工具路径无效"));
    }
    if (!toolBinaryPath.exists()) {
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::executableNotFound(toolBinaryPath.toString()),
            QStringLiteral("Source 1 导入工具不存在"));
    }

    QStringList args = buildArguments(options);

    Core::Process::ProcessOptions procOptions;
    procOptions.arguments = args;
    procOptions.timeout = options.timeoutMs > 0 ? options.timeoutMs : 120000;
    if (!options.isCsgo) {
        procOptions.standardInput = "y\n";
    }

    if (taskCtx) {
        taskCtx->info(QStringLiteral("Executing source1import: %1 %2")
                          .arg(toolBinaryPath.toString(), args.join(QLatin1Char(' '))));
    }

    auto procResult = Core::Process::ProcessRunner::run(toolBinaryPath.toString(), args, procOptions);

    if (procResult.status == Core::Process::ProcessStatus::Crashed) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("source1import crashed: %1").arg(procResult.errorMessage));
        }
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::crashed(QStringLiteral("source1import.exe"), procResult.errorMessage),
            QStringLiteral("Source 1 导入工具崩溃"));
    }

    if (procResult.status == Core::Process::ProcessStatus::TimedOut) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("source1import timed out after %1 ms").arg(procOptions.timeout));
        }
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::timeout(QStringLiteral("source1import.exe"), procResult.errorMessage),
            QStringLiteral("Source 1 导入工具执行超时"));
    }

    if (procResult.status == Core::Process::ProcessStatus::FailedToStart) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("source1import failed to start: %1").arg(procResult.errorMessage));
        }
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::executionFailed(QStringLiteral("source1import.exe"), procResult.exitCode, procResult.errorMessage),
            QStringLiteral("Source 1 导入工具启动失败"));
    }

    auto logResult = Source1ImportLogParser::parse(procResult.stdOut, procResult.stdErr, procResult.exitCode);

    Source1ImportToolResult toolResult;
    toolResult.success = logResult.success;
    toolResult.importedCount = logResult.importedCount;
    toolResult.failedCount = logResult.failedCount;
    toolResult.skippedCount = logResult.skippedCount;
    toolResult.generatedVpcfPaths = logResult.generatedVpcfPaths;
    toolResult.rawOutput = logResult.rawOutput;

    if (taskCtx) {
        for (const auto& w : logResult.warnings) {
            taskCtx->warning(w);
        }
        for (const auto& e : logResult.errorMessages) {
            taskCtx->error(e);
        }
        for (const auto& p : toolResult.generatedVpcfPaths) {
            taskCtx->info(QStringLiteral("Generated VPCF: %1").arg(p));
        }
    }

    if (logResult.hasNoMatchingFiles) {
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::noMatchingFiles(options.inputFilePath.toString(), procResult.stdOut),
            QStringLiteral("未找到与规格匹配的文件"),
            toolResult);
    }

    if (!toolResult.success) {
        QString failureReason = logResult.errorMessages.isEmpty()
            ? QStringLiteral("source1import returned failure with exit code %1").arg(procResult.exitCode)
            : logResult.errorMessages.join(QStringLiteral("; "));
        if (taskCtx) {
            taskCtx->error(QStringLiteral("source1import failed: %1").arg(failureReason));
        }
        return Core::Result<Source1ImportToolResult>::failure(
            ToolErrors::importFailed(failureReason, procResult.stdOut),
            QStringLiteral("资源导入失败: %1").arg(failureReason),
            toolResult);
    }

    return Core::Result<Source1ImportToolResult>::success(
        toolResult,
        QStringLiteral("成功导入 %1 个资产").arg(toolResult.importedCount));
}

} // namespace Domain::Tool
