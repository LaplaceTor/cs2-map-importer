#pragma once

#include <QString>

namespace Core::Logging {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

inline QString logLevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("DEBUG");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    case LogLevel::Warning:
        return QStringLiteral("WARNING");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("UNKNOWN");
}

} // namespace Core::Logging
