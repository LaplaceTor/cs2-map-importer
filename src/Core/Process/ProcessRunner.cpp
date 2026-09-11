#include "ProcessRunner.h"

#include <QProcess>
#include <QElapsedTimer>

namespace Core::Process {

ProcessResult ProcessRunner::run(const QString& executable, const QStringList& arguments, const ProcessOptions& options) {
    ProcessRunner runner;
    return runner.execute(executable, arguments, options);
}

ProcessResult ProcessRunner::run(const QString& executable, const ProcessOptions& options) {
    ProcessRunner runner;
    return runner.execute(executable, options);
}

ProcessResult ProcessRunner::execute(const QString& executable, const QStringList& arguments, const ProcessOptions& options) {
    ProcessOptions opts = options;
    opts.arguments = arguments;
    return execute(executable, opts);
}

ProcessResult ProcessRunner::execute(const QString& executable, const ProcessOptions& options) {
    ProcessResult result;

    if (executable.isEmpty()) {
        result.status = ProcessStatus::FailedToStart;
        result.exitCode = -1;
        result.errorMessage = QStringLiteral("Executable path is empty.");
        return result;
    }

    QProcess process;

    if (!options.workingDirectory.isEmpty()) {
        process.setWorkingDirectory(options.workingDirectory);
    }

    if (!options.environment.isEmpty()) {
        process.setProcessEnvironment(options.environment);
    }

    process.setProgram(executable);
    process.setArguments(options.arguments);

    QElapsedTimer timer;
    if (options.timeout >= 0) {
        timer.start();
    }

    process.start();

    if (!options.standardInput.isEmpty()) {
        process.write(options.standardInput);
        process.closeWriteChannel();
    }

    int startTimeout = options.timeout;
    if (!process.waitForStarted(startTimeout)) {
        result.exitCode = -1;
        if (process.error() == QProcess::Timedout) {
            result.status = ProcessStatus::TimedOut;
            result.errorMessage = QStringLiteral("Process startup timed out.");
        } else {
            result.status = ProcessStatus::FailedToStart;
            result.errorMessage = QString("Failed to start executable '%1': %2")
                                      .arg(executable, process.errorString());
        }
        return result;
    }

    QString fullStdOut;
    QString fullStdErr;
    QString pendingStdOutLine;
    QString pendingStdErrLine;

    auto drainStream = [](QProcess& proc, bool isError, QString& pendingLine, QString& fullOutput, const std::function<void(const QString&)>& lineCallback) {
        QByteArray data = isError ? proc.readAllStandardError() : proc.readAllStandardOutput();
        if (data.isEmpty()) {
            return;
        }
        QString text = QString::fromUtf8(data);
        fullOutput += text;

        if (lineCallback) {
            text = pendingLine + text;
            pendingLine.clear();

            int start = 0;
            int newlineIdx = -1;
            while ((newlineIdx = text.indexOf(QLatin1Char('\n'), start)) != -1) {
                QString line = text.mid(start, newlineIdx - start);
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
                lineCallback(line);
                start = newlineIdx + 1;
            }
            if (start < text.length()) {
                pendingLine = text.mid(start);
            }
        }
    };

    while (process.state() == QProcess::Running) {
        if (options.cancellationToken.isCancelled()) {
            process.kill();
            process.waitForFinished(1000);
            drainStream(process, false, pendingStdOutLine, fullStdOut, options.onStdOutLine);
            drainStream(process, true, pendingStdErrLine, fullStdErr, options.onStdErrLine);
            if (!pendingStdOutLine.isEmpty() && options.onStdOutLine) {
                options.onStdOutLine(pendingStdOutLine);
            }
            if (!pendingStdErrLine.isEmpty() && options.onStdErrLine) {
                options.onStdErrLine(pendingStdErrLine);
            }
            result.status = ProcessStatus::Cancelled;
            result.exitCode = -1;
            result.errorMessage = QStringLiteral("Process was cancelled by user.");
            result.stdOut = fullStdOut;
            result.stdErr = fullStdErr;
            return result;
        }

        if (options.timeout >= 0 && timer.elapsed() >= options.timeout) {
            process.kill();
            process.waitForFinished(1000);
            drainStream(process, false, pendingStdOutLine, fullStdOut, options.onStdOutLine);
            drainStream(process, true, pendingStdErrLine, fullStdErr, options.onStdErrLine);
            if (!pendingStdOutLine.isEmpty() && options.onStdOutLine) {
                options.onStdOutLine(pendingStdOutLine);
            }
            if (!pendingStdErrLine.isEmpty() && options.onStdErrLine) {
                options.onStdErrLine(pendingStdErrLine);
            }
            result.status = ProcessStatus::TimedOut;
            result.exitCode = -1;
            result.errorMessage = QString("Process execution timed out after %1 ms.").arg(options.timeout);
            result.stdOut = fullStdOut;
            result.stdErr = fullStdErr;
            return result;
        }

        process.waitForReadyRead(50);
        drainStream(process, false, pendingStdOutLine, fullStdOut, options.onStdOutLine);
        drainStream(process, true, pendingStdErrLine, fullStdErr, options.onStdErrLine);
    }

    drainStream(process, false, pendingStdOutLine, fullStdOut, options.onStdOutLine);
    drainStream(process, true, pendingStdErrLine, fullStdErr, options.onStdErrLine);
    if (!pendingStdOutLine.isEmpty() && options.onStdOutLine) {
        options.onStdOutLine(pendingStdOutLine);
    }
    if (!pendingStdErrLine.isEmpty() && options.onStdErrLine) {
        options.onStdErrLine(pendingStdErrLine);
    }

    if (options.cancellationToken.isCancelled()) {
        result.status = ProcessStatus::Cancelled;
        result.exitCode = -1;
        result.errorMessage = QStringLiteral("Process was cancelled by user.");
        result.stdOut = fullStdOut;
        result.stdErr = fullStdErr;
        return result;
    }

    result.stdOut = fullStdOut;
    result.stdErr = fullStdErr;
    result.exitCode = process.exitCode();

    if (process.exitStatus() == QProcess::CrashExit) {
        result.status = ProcessStatus::Crashed;
        result.errorMessage = QString("Process crashed with error: %1").arg(process.errorString());
    } else if (result.exitCode != 0) {
        result.status = ProcessStatus::NonZeroExit;
        result.errorMessage = QString("Process exited with non-zero code %1").arg(result.exitCode);
    } else {
        result.status = ProcessStatus::Success;
    }

    return result;
}

} // namespace Core::Process
