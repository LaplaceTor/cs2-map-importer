#include "ResourceCompilerTool.h"

#include "ResourceCompilerLogParser.h"
#include "ToolErrors.h"
#include "Core/Process/ProcessRunner.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Process/ProcessResult.h"
#include "Core/Logging/LogManager.h"
#include <QDateTime>
#include <QFileInfo>

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
    QString fullCommandLine = QStringLiteral("%1 %2").arg(toolBinaryPath.toString(), args.join(QLatin1Char(' ')));

    std::shared_ptr<Core::Logging::TaskLoggingContext> childTask;
    if (taskCtx && taskCtx->taskId() != 0) {
        childTask = Core::Logging::LogManager::instance().createToolTask(taskCtx->taskId(), fullCommandLine);
        if (childTask) {
            childTask->start();
        }
    }

    Core::Process::ProcessOptions procOptions;
    procOptions.arguments = args;
    procOptions.timeout = options.timeoutMs > 0 ? options.timeoutMs : 120000;
    procOptions.cancellationToken = options.cancellationToken;

    qint64 lastFlushTime = QDateTime::currentMSecsSinceEpoch();
    int pendingLinesCount = 0;

    if (childTask) {
        procOptions.onStdOutLine = [childTask, &lastFlushTime, &pendingLinesCount](const QString& line) {
            childTask->logExternalToolOutput(line, Core::Logging::LogLevel::Info);
            pendingLinesCount++;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - lastFlushTime >= 50 || pendingLinesCount >= 20) {
                childTask->flush();
                lastFlushTime = now;
                pendingLinesCount = 0;
            }
        };
        procOptions.onStdErrLine = [childTask, &lastFlushTime, &pendingLinesCount](const QString& line) {
            childTask->logExternalToolOutput(line, Core::Logging::LogLevel::Warning);
            pendingLinesCount++;
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - lastFlushTime >= 50 || pendingLinesCount >= 20) {
                childTask->flush();
                lastFlushTime = now;
                pendingLinesCount = 0;
            }
        };
    }

    const QString toolLogFileName = childTask ? QFileInfo(childTask->logFilePath()).fileName() : QString();
    if (taskCtx) {
        if (!toolLogFileName.isEmpty()) {
            taskCtx->info(QStringLiteral("[External Tool] Executing: resourcecompiler (Log: %1)").arg(toolLogFileName));
        } else {
            taskCtx->info(QStringLiteral("Executing resourcecompiler: %1").arg(fullCommandLine));
        }
    }

    auto procResult = Core::Process::ProcessRunner::run(toolBinaryPath.toString(), args, procOptions);

    if (procResult.isCancelled() || options.cancellationToken.isCancelled()) {
        if (childTask) {
            Core::Logging::LogManager::instance().cancelTask(childTask->taskId(), QStringLiteral("resourcecompiler cancelled"));
        }
        if (taskCtx) {
            taskCtx->warning(QStringLiteral("resourcecompiler was cancelled"));
        }
        return Core::Result<ResourceCompilerToolResult>::cancelled(QStringLiteral("资源编译器已被取消"));
    }

    if (procResult.status == Core::Process::ProcessStatus::Crashed) {
        if (childTask) {
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), procResult.errorMessage);
        }
        if (taskCtx) {
            taskCtx->error(QStringLiteral("resourcecompiler crashed: %1").arg(procResult.errorMessage));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::crashed(QStringLiteral("resourcecompiler.exe"), procResult.errorMessage),
            QStringLiteral("资源编译器崩溃"));
    }

    if (procResult.status == Core::Process::ProcessStatus::TimedOut) {
        if (childTask) {
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), QStringLiteral("Timed out"));
        }
        if (taskCtx) {
            taskCtx->error(QStringLiteral("resourcecompiler timed out after %1 ms").arg(procOptions.timeout));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::timeout(QStringLiteral("resourcecompiler.exe"), procResult.errorMessage),
            QStringLiteral("资源编译器执行超时"));
    }

    if (procResult.status == Core::Process::ProcessStatus::FailedToStart) {
        if (childTask) {
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), procResult.errorMessage);
        }
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
        if (childTask) {
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), failureReason);
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::compilationFailed(failureReason, procResult.stdOut),
            QStringLiteral("资源编译失败"),
            toolResult);
    }

    if (childTask) {
        Core::Logging::LogManager::instance().finishTask(childTask->taskId(), QStringLiteral("Compiled %1 asset(s)").arg(toolResult.compiledCount));
    }

    if (taskCtx && !toolLogFileName.isEmpty()) {
        taskCtx->info(QStringLiteral("[External Tool] Finished: resourcecompiler (Exit code: %1, Log: %2)")
            .arg(QString::number(procResult.exitCode), toolLogFileName));
    }

    return Core::Result<ResourceCompilerToolResult>::success(
        toolResult,
        QStringLiteral("成功编译 %1 个资源").arg(toolResult.compiledCount));
}

} // namespace Domain::Tool
