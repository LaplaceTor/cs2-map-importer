#pragma once

#include <QString>

namespace Core::Logging {

enum class LogSource {
    Application,
    Workflow,
    ExternalTool
};

inline QString logSourceToString(LogSource source)
{
    switch (source) {
    case LogSource::Application:
        return QStringLiteral("Application");
    case LogSource::Workflow:
        return QStringLiteral("Workflow");
    case LogSource::ExternalTool:
        return QStringLiteral("ExternalTool");
    }
    return QStringLiteral("Unknown");
}

} // namespace Core::Logging

