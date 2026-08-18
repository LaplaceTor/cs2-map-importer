#pragma once

#include <QString>
#include <QStringList>
#include <QProcessEnvironment>

namespace Core::Process {

struct ProcessOptions {
    int timeout = -1; // Timeout in milliseconds (-1 for infinite/no timeout)
    QString workingDirectory;
    QProcessEnvironment environment;
    QStringList arguments;
};

} // namespace Core::Process

namespace Core {
    using Process::ProcessOptions;
}

using Core::Process::ProcessOptions;
