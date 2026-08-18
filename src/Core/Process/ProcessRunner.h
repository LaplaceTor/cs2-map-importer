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

    static ProcessResult Run(const QString& executable, const QStringList& arguments = {}, const ProcessOptions& options = ProcessOptions());
    static ProcessResult Run(const QString& executable, const ProcessOptions& options);

    ProcessResult execute(const QString& executable, const QStringList& arguments = {}, const ProcessOptions& options = ProcessOptions());
    ProcessResult Execute(const QString& executable, const QStringList& arguments = {}, const ProcessOptions& options = ProcessOptions()) {
        return execute(executable, arguments, options);
    }
};

} // namespace Core::Process

namespace Core {
    using Process::ProcessRunner;
}

using Core::Process::ProcessRunner;
