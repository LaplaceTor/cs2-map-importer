#include "ProcessRunner.h"

#include <QProcess>
#include <QElapsedTimer>

namespace Core::Process {

ProcessResult ProcessRunner::run(const QString& executable, const QStringList& arguments, const ProcessOptions& options) {
    ProcessRunner runner;
    return runner.execute(executable, arguments, options);
}

ProcessResult ProcessRunner::run(const QString& executable, const ProcessOptions& options) {
    return run(executable, options.arguments, options);
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

    QElapsedTimer timer;
    if (options.timeout >= 0) {
        timer.start();
    }

    process.start();

    int startTimeout = options.timeout;
    if (!process.waitForStarted(startTimeout)) {
        result.success = false;
        result.exitCode = -1;
        if (process.error() == QProcess::Timedout) {
            result.timedOut = true;
            result.errorInformation = QStringLiteral("Process startup timed out.");
        } else {
            result.errorInformation = QString("Failed to start executable '%1': %2")
                                          .arg(executable, process.errorString());
        }
        return result;
    }

    int remainingTimeout = -1;
    if (options.timeout >= 0) {
        qint64 elapsed = timer.elapsed();
        qint64 rem = static_cast<qint64>(options.timeout) - elapsed;
        if (rem < 0) {
            rem = 0;
        }
        remainingTimeout = static_cast<int>(rem);
    }

    if (!process.waitForFinished(remainingTimeout)) {
        if (process.error() == QProcess::Timedout) {
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
    result.success = (process.exitStatus() == QProcess::NormalExit && result.exitCode == 0);

    if (!result.success) {
        if (process.exitStatus() == QProcess::CrashExit) {
            result.errorInformation = QString("Process crashed with error: %1").arg(process.errorString());
        } else {
            result.errorInformation = QString("Process exited with code %1").arg(result.exitCode);
        }
    }

    return result;
}

} // namespace Core::Process
