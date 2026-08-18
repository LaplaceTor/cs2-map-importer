#pragma once

#include <QString>
#include <QStringList>

#include "ProcessOptions.h"
#include "ProcessResult.h"

namespace Core::Process {

class ProcessRunner {
public:
    ProcessRunner() = default;
    ~ProcessRunner() = default;

    static ProcessResult run(const QString& executable, const QStringList& arguments, const ProcessOptions& options = ProcessOptions());
    static ProcessResult run(const QString& executable, const ProcessOptions& options = ProcessOptions());

    ProcessResult execute(const QString& executable, const ProcessOptions& options = ProcessOptions());
    ProcessResult execute(const QString& executable, const QStringList& arguments, const ProcessOptions& options = ProcessOptions());
};

} // namespace Core::Process
