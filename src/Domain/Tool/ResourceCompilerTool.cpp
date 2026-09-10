#include "ResourceCompilerTool.h"

#include "ResourceCompilerLogParser.h"
#include "ToolErrors.h"
#include "Core/Process/ProcessRunner.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Process/ProcessResult.h"

namespace Domain::Tool {

QStringList ResourceCompilerTool::buildArguments(const ResourceCompilerOptions& options)
{
    QStringList args;
    args << QStringLiteral("-retail");
    args << QStringLiteral("-nop4");
    if (options.forceCompile) {
        args << QStringLiteral("-f");
    }
    if (options.verbose) {
        args << QStringLiteral("-v");
    }
    if (!options.gameDir.isEmpty()) {
        args << QStringLiteral("-game");
        args << options.gameDir.toString();
    }
    for (const QString& file : options.inputFiles) {
        if (!file.isEmpty()) {
            args << file;
        }
    }
    return args;
}

Core::Result<ResourceCompilerToolResult> ResourceCompilerTool::compileResources(
    const Core::Path::FilesystemPath& toolBinaryPath,
    const ResourceCompilerOptions& options,
    Core::Logging::TaskLoggingContext* taskCtx)
{
    if (toolBinaryPath.isEmpty() || !toolBinaryPath.isValid()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::executableNotFound(toolBinaryPath.toString()),
            QStringLiteral("资源编译器路径无效"));
    }
    if (!toolBinaryPath.exists()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::executableNotFound(toolBinaryPath.toString()),
            QStringLiteral("资源编译器不存在"));
    }
    if (options.gameDir.isEmpty()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("CS2 游戏目录不能为空"));
    }
    if (options.inputFiles.isEmpty()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QStringLiteral("待编译资源列表不能为空"));
    }

    QStringList args = buildArguments(options);

    Core::Process::ProcessOptions procOptions;
    procOptions.arguments = args;
    procOptions.timeout = options.timeoutMs > 0 ? options.timeoutMs : 120000;

    if (taskCtx) {
        taskCtx->info(QStringLiteral("Executing resourcecompiler: %1 %2")
                          .arg(toolBinaryPath.toString(), args.join(QLatin1Char(' '))));
    }

    auto procResult = Core::Process::ProcessRunner::run(toolBinaryPath.toString(), args, procOptions);

    if (procResult.status == Core::Process::ProcessStatus::Crashed) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("resourcecompiler crashed: %1").arg(procResult.errorMessage));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::crashed(QStringLiteral("resourcecompiler.exe"), procResult.errorMessage),
            QStringLiteral("资源编译器崩溃"));
    }

    if (procResult.status == Core::Process::ProcessStatus::TimedOut) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("resourcecompiler timed out after %1 ms").arg(procOptions.timeout));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::timeout(QStringLiteral("resourcecompiler.exe"), procResult.errorMessage),
            QStringLiteral("资源编译器执行超时"));
    }

    if (procResult.status == Core::Process::ProcessStatus::FailedToStart) {
        if (taskCtx) {
            taskCtx->error(QStringLiteral("resourcecompiler failed to start: %1").arg(procResult.errorMessage));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::executionFailed(QStringLiteral("resourcecompiler.exe"), procResult.exitCode, procResult.errorMessage),
            QStringLiteral("资源编译器启动失败"));
    }

    auto logResult = ResourceCompilerLogParser::parse(procResult.stdOut, procResult.stdErr, procResult.exitCode);

    ResourceCompilerToolResult toolResult;
    toolResult.success = logResult.success;
    toolResult.compiledCount = logResult.compiledCount;
    toolResult.failedCount = logResult.failedCount;
    toolResult.skippedCount = logResult.skippedCount;
    toolResult.compiledVpcfCPaths = logResult.compiledVpcfCPaths;
    toolResult.compileErrors = logResult.compileErrors;
    toolResult.rawOutput = logResult.rawOutput;

    if (taskCtx) {
        for (const auto& w : logResult.warnings) {
            taskCtx->warning(w);
        }
        for (const auto& e : logResult.compileErrors) {
            taskCtx->error(e);
        }
        for (const auto& p : toolResult.compiledVpcfCPaths) {
            taskCtx->info(QStringLiteral("Compiled VPCF_C: %1").arg(p));
        }
    }

    if (!toolResult.success) {
        QString failureReason = logResult.compileErrors.isEmpty()
            ? QStringLiteral("resourcecompiler returned failure with exit code %1").arg(procResult.exitCode)
            : logResult.compileErrors.join(QStringLiteral("; "));
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::compilationFailed(failureReason, procResult.stdOut),
            QStringLiteral("资源编译失败"),
            toolResult);
    }

    return Core::Result<ResourceCompilerToolResult>::success(
        toolResult,
        QStringLiteral("成功编译 %1 个资源").arg(toolResult.compiledCount));
}

} // namespace Domain::Tool
