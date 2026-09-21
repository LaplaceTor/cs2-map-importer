#include <QCoreApplication>
#include "ResourceCompilerTool.h"

#include "ResourceCompilerLogParser.h"
#include "ToolErrors.h"
#include "Core/Process/ProcessRunner.h"
#include "Core/Process/ProcessOptions.h"
#include "Core/Process/ProcessResult.h"
#include "Core/Logging/LogManager.h"
#include "Core/Temp/TempFile.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace Domain::Tool {

namespace {

Core::Result<Core::Temp::TempFile> writeFileList(const QStringList& files)
{
    try {
        auto tempFile = Core::Temp::TempFile::create(QStringLiteral("rc_filelist_XXXXXX.txt"));
        QFile file(tempFile.path());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return Core::Result<Core::Temp::TempFile>::failure(
                Core::Error::ErrorCode::WriteFailed,
                QCoreApplication::translate("ResourceCompilerTool", "Failed to open temporary filelist: %1").arg(file.errorString()));
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        for (const QString& f : files) {
            const QString trimmed = f.trimmed();
            if (!trimmed.isEmpty()) {
                out << QDir::toNativeSeparators(trimmed) << "\n";
            }
        }
        out.flush();
        file.close();
        return Core::Result<Core::Temp::TempFile>::success(std::move(tempFile));
    } catch (const Core::Error::Exception& ex) {
        return Core::Result<Core::Temp::TempFile>::failure(ex.error());
    } catch (const std::exception& ex) {
        return Core::Result<Core::Temp::TempFile>::failure(
            Core::Error::ErrorCode::WriteFailed,
            QString::fromUtf8(ex.what()));
    }
}

Core::Result<ResourceCompilerToolResult> compileInternal(
    const Core::Path::FilesystemPath& toolBinaryPath,
    const ResourceCompilerOptions& options,
    Core::Logging::TaskLoggingContext* taskCtx)
{
    if (options.cancellationToken.isCancelled()) {
        return Core::Result<ResourceCompilerToolResult>::cancelled(
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler was cancelled"));
    }

    Core::Temp::TempFile tempFile;
    QString fileListPath;
    if (options.inputFiles.size() > 1) {
        auto fileListRes = writeFileList(options.inputFiles);
        if (!fileListRes.isSuccess()) {
            return Core::Result<ResourceCompilerToolResult>::failure(fileListRes.error(), fileListRes.message());
        }
        tempFile = std::move(fileListRes.value());
        fileListPath = tempFile.path();
    }

    QStringList args = ResourceCompilerTool::buildArguments(options, fileListPath);
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
    if (!options.gameDir.isEmpty()) {
        procOptions.workingDirectory = options.gameDir.toString();
    }

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
            Core::Logging::LogManager::instance().cancelTask(childTask->taskId(), QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler cancelled"));
        }
        if (taskCtx) {
            taskCtx->warning(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler was cancelled"));
        }
        return Core::Result<ResourceCompilerToolResult>::cancelled(
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler was cancelled"));
    }

    if (procResult.status == Core::Process::ProcessStatus::Crashed) {
        if (childTask) {
            childTask->error(procResult.errorMessage);
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), procResult.errorMessage);
        }
        if (taskCtx) {
            taskCtx->warning(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler crashed: %1").arg(procResult.errorMessage));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::crashed(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler.exe"), procResult.errorMessage),
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler crashed"));
    }

    if (procResult.status == Core::Process::ProcessStatus::TimedOut) {
        if (childTask) {
            childTask->error(QCoreApplication::translate("ResourceCompilerTool", "Timed out"));
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), QCoreApplication::translate("ResourceCompilerTool", "Timed out"));
        }
        if (taskCtx) {
            taskCtx->warning(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler timed out after %1 ms").arg(procOptions.timeout));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::timeout(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler.exe"), procResult.errorMessage),
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler timed out"));
    }

    if (procResult.status == Core::Process::ProcessStatus::FailedToStart) {
        if (childTask) {
            childTask->error(procResult.errorMessage);
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), procResult.errorMessage);
        }
        if (taskCtx) {
            taskCtx->warning(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler failed to start: %1").arg(procResult.errorMessage));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::executionFailed(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler.exe"), procResult.exitCode, procResult.errorMessage),
            QCoreApplication::translate("ResourceCompilerTool", "Failed to start resource compiler"));
    }

    auto logResult = ResourceCompilerLogParser::parse(
        procResult.stdOut, procResult.stdErr, procResult.exitCode, procOptions.workingDirectory);

    ResourceCompilerToolResult toolResult;
    toolResult.success = logResult.success;
    toolResult.compiledCount = logResult.compiledCount;
    toolResult.failedCount = logResult.failedCount;
    toolResult.skippedCount = logResult.skippedCount;
    toolResult.compiledVpcfCPaths = logResult.compiledVpcfCPaths;
    toolResult.compileErrors = logResult.compileErrors;
    toolResult.rawOutput = logResult.rawOutput;

    if (childTask) {
        for (const auto& w : logResult.warnings) {
            childTask->warning(w);
        }
        for (const auto& e : logResult.compileErrors) {
            childTask->error(e);
        }
    }

    if (taskCtx) {
        for (const auto& w : logResult.warnings) {
            taskCtx->warning(w);
        }
        for (const auto& e : logResult.compileErrors) {
            taskCtx->warning(e);
        }
    }

    if (!toolResult.success) {
        QString failureReason = logResult.compileErrors.isEmpty()
            ? QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler returned failure with exit code %1").arg(procResult.exitCode)
            : logResult.compileErrors.join(QCoreApplication::translate("ResourceCompilerTool", "; "));
        if (childTask) {
            childTask->error(failureReason);
            Core::Logging::LogManager::instance().failTask(childTask->taskId(), failureReason);
        }
        if (taskCtx) {
            taskCtx->warning(QCoreApplication::translate("ResourceCompilerTool", "resourcecompiler failed: %1").arg(failureReason));
        }
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::compilationFailed(failureReason, procResult.stdOut),
            QCoreApplication::translate("ResourceCompilerTool", "Resource compilation failed"),
            toolResult);
    }

    if (childTask) {
        Core::Logging::LogManager::instance().finishTask(childTask->taskId(), QCoreApplication::translate("ResourceCompilerTool", "Compiled %1 asset(s)").arg(toolResult.compiledCount));
    }

    if (taskCtx && !toolLogFileName.isEmpty()) {
        taskCtx->info(QStringLiteral("[External Tool] Finished: resourcecompiler (Exit code: %1, Log: %2)")
            .arg(QString::number(procResult.exitCode), toolLogFileName));
    }

    return Core::Result<ResourceCompilerToolResult>::success(
        toolResult,
        QCoreApplication::translate("ResourceCompilerTool", "Successfully compiled %1 resource(s)").arg(toolResult.compiledCount));
}

} // namespace

QStringList ResourceCompilerTool::buildArguments(const ResourceCompilerOptions& options, const QString& fileListPath)
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
    if (!fileListPath.isEmpty()) {
        args << QStringLiteral("-filelist");
        args << fileListPath;
    } else {
        for (const QString& file : options.inputFiles) {
            if (!file.isEmpty()) {
                args << file;
            }
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
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler path is invalid"));
    }
    if (!toolBinaryPath.exists()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            ToolErrors::executableNotFound(toolBinaryPath.toString()),
            QCoreApplication::translate("ResourceCompilerTool", "Resource compiler does not exist"));
    }
    if (options.gameDir.isEmpty()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("ResourceCompilerTool", "CS2 game directory cannot be empty"));
    }
    if (options.inputFiles.isEmpty()) {
        return Core::Result<ResourceCompilerToolResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("ResourceCompilerTool", "Resource list to compile cannot be empty"));
    }

    return compileInternal(toolBinaryPath, options, taskCtx);
}

} // namespace Domain::Tool
