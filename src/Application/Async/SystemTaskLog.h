#pragma once

#include <QString>
#include <utility>

#include "Core/Logging/ApplicationLogger.h"

namespace Application::Async {

/**
 * @brief Lightweight logging context for system (non-workflow) background tasks.
 *
 * System tasks deliberately have no TaskLoggingContext and no LogManager presence:
 * their entries are merged into the application log (application_<timestamp>.log)
 * under a "[TaskName]" prefix instead of being routed to the UI task tree.
 * All methods are thread-safe (ApplicationLogger serializes file access), so the
 * context can be used freely from the worker thread and passed down into callees.
 */
class SystemTaskLog {
public:
    explicit SystemTaskLog(QString taskName)
        : m_taskName(std::move(taskName))
    {
    }

    const QString& taskName() const noexcept { return m_taskName; }

    void debug(const QString& message) const
    {
        Core::Logging::ApplicationLogger::debug(prefixed(message));
    }

    void info(const QString& message) const
    {
        Core::Logging::ApplicationLogger::info(prefixed(message));
    }

    void warning(const QString& message) const
    {
        Core::Logging::ApplicationLogger::warning(prefixed(message));
    }

    void error(const QString& message) const
    {
        Core::Logging::ApplicationLogger::error(prefixed(message));
    }

private:
    QString prefixed(const QString& message) const
    {
        return QStringLiteral("[%1] %2").arg(m_taskName, message);
    }

    QString m_taskName;
};

} // namespace Application::Async
