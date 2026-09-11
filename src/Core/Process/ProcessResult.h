#pragma once

#include <QString>
#include "Core/Error/Error.h"
#include "Core/Error/ErrorCode.h"

namespace Core::Process {

enum class ProcessStatus {
    Success,
    FailedToStart,
    Crashed,
    TimedOut,
    NonZeroExit,
    Cancelled
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

    bool isCancelled() const {
        return status == ProcessStatus::Cancelled;
    }

    Core::Error::ErrorCode toErrorCode() const {
        switch (status) {
            case ProcessStatus::Success:
                return Core::Error::ErrorCode::Success;
            case ProcessStatus::TimedOut:
                return Core::Error::ErrorCode::ProcessTimeout;
            case ProcessStatus::Crashed:
                return Core::Error::ErrorCode::ProcessCrashed;
            case ProcessStatus::Cancelled:
                return Core::Error::ErrorCode::Cancelled;
            case ProcessStatus::FailedToStart:
            case ProcessStatus::NonZeroExit:
            default:
                return Core::Error::ErrorCode::ProcessFailed;
        }
    }

    Core::Error::Error toError() const {
        if (isSuccess()) {
            return Core::Error::Error::success();
        }
        QString msg = errorMessage.isEmpty() ? QStringLiteral("Process execution failed with exit code %1").arg(exitCode) : errorMessage;
        QString details = stdErr.trimmed().isEmpty() ? stdOut.trimmed() : stdErr.trimmed();
        return Core::Error::Error(toErrorCode(), msg, details);
    }
};

} // namespace Core::Process
