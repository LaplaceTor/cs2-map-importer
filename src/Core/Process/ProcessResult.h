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
};

} // namespace Core::Process
