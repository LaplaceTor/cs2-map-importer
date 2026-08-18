#include "ProcessRunner.h"

#include <QProcess>

namespace Core::Process {

ProcessResult ProcessRunner::Run(const QString& executable, const QStringList& arguments, const ProcessOptions& options) {
    ProcessRunner runner;
    return runner.execute(executable, arguments, options);
}

ProcessResult ProcessRunner::Run(const QString& executable, const ProcessOptions& options) {
    return Run(executable, options.arguments, options);
}

ProcessResult ProcessRunner::execute(const QString& executable, const QStringList& arguments, const ProcessOptions& options) {
    ProcessResult result;

    if (executable.isEmpty()) {
        result.success = false;
        result.exitCode = -1;
        result.errorInformation = QStringLiteral("Executable path is empty.");
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

    QStringList finalArgs = arguments.isEmpty() ? options.arguments : arguments;
    process.setArguments(finalArgs);

    process.start();

    int startTimeout = (options.timeout > 0 && options.timeout < 5000) ? options.timeout : 30000;
    if (!process.waitForStarted(startTimeout)) {
        result.success = false;
        result.exitCode = -1;
        if (process.error() == QProcess::ProcessError::TimedOut) {
            result.timedOut = true;
            result.errorInformation = QStringLiteral("Process startup timed out.");
        } else {
            result.errorInformation = QString("Failed to start executable '%1': %2")
                                          .arg(executable, process.errorString());
        }
        return result;
    }

    int finishTimeout = options.timeout > 0 ? options.timeout : -1;
    if (!process.waitForFinished(finishTimeout)) {
        if (process.error() == QProcess::ProcessError::TimedOut) {
            result.timedOut = true;
            result.errorInformation = QString("Process execution timed out after %1 ms.").arg(options.timeout);
        } else {
            result.errorInformation = QString("Process execution failed: %1").arg(process.errorString());
        }

        if (process.state() == QProcess::Running) {
            process.kill();
            process.waitForFinished(1000);
        }

        result.success = false;
        result.exitCode = -1;
        result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
        result.stdErr = QString::fromUtf8(process.readAllStandardError());
        return result;
    }

    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    result.success = (process.exitStatus() == QProcess::ExitStatus::NormalExit && result.exitCode == 0);

    if (!result.success) {
        if (process.exitStatus() == QProcess::ExitStatus::CrashExit) {
            result.errorInformation = QString("Process crashed with error: %1").arg(process.errorString());
        } else {
            result.errorInformation = QString("Process exited with code %1").arg(result.exitCode);
        }
    }

    return result;
}

} // namespace Core::Process
