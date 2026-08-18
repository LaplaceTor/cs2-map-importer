#pragma once

#include <QString>

namespace Core::Process {

enum class ProcessStatus {
    Success,
    FailedToStart,
    Crashed,
    TimedOut,
    NonZeroExit
};

struct ProcessResult {
    ProcessStatus status = ProcessStatus::FailedToStart;
    int exitCode = -1;
    QString stdOut;
    QString stdErr;
    QString errorMessage;

    bool isSuccess() const {
        return status == ProcessStatus::Success;
    }
};

} // namespace Core::Process
