#pragma once

#include <QString>

namespace Core::Process {

struct ProcessResult {
    bool success = false;
    int exitCode = -1;
    QString stdOut;
    QString stdErr;
    QString errorInformation;
    bool timedOut = false;

    bool Success() const { return success; }
    int ExitCode() const { return exitCode; }
    QString StdOut() const { return stdOut; }
    QString StdErr() const { return stdErr; }
    QString ErrorInformation() const { return errorInformation; }
    bool TimedOut() const { return timedOut; }
};

} // namespace Core::Process

namespace Core {
    using Process::ProcessResult;
}

using Core::Process::ProcessResult;
