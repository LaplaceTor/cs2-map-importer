#pragma once

#include <QString>

namespace Core::Logging {

class Logger {
public:
    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);
};

} // namespace Core::Logging
