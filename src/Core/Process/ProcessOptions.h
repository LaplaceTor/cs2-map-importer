#pragma once

#include <QString>
#include <QStringList>
#include <QProcessEnvironment>
#include <QByteArray>
#include <functional>
#include "Core/Async/CancellationToken.h"

namespace Core::Process {

struct ProcessOptions {
    int timeout = 30000; // Timeout in milliseconds (-1 for infinite/no timeout)
    QString workingDirectory;
    QProcessEnvironment environment;
    QStringList arguments;
    QByteArray standardInput;

    std::function<void(const QString& line)> onStdOutLine;
    std::function<void(const QString& line)> onStdErrLine;
    Core::Async::CancellationToken cancellationToken;
};

} // namespace Core::Process
