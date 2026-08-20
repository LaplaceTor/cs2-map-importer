#include "TaskLoggingContext.h"
#include <algorithm>

namespace Core::Logging {

TaskLoggingContext::TaskLoggingContext(quint64 taskId, QString taskName)
    : m_taskId(taskId)
    , m_taskName(std::move(taskName))
    , m_logBlock(taskId)
{
}

void TaskLoggingContext::updateProgress(double progress, const QString& message)
{
    m_progress = std::clamp(progress, 0.0, 1.0);
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

void TaskLoggingContext::debug(const QString& message)
{
    log(LogLevel::Debug, message);
}

void TaskLoggingContext::info(const QString& message)
{
    log(LogLevel::Info, message);
}

void TaskLoggingContext::warning(const QString& message)
{
    log(LogLevel::Warning, message);
}

void TaskLoggingContext::error(const QString& message)
{
    log(LogLevel::Error, message);
}

void TaskLoggingContext::log(LogLevel level, const QString& message)
{
    LogEntry entry;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.message = message;

    m_logBlock.append(std::move(entry));
}

void TaskLoggingContext::start()
{
    m_state = TaskState::Running;
}

void TaskLoggingContext::complete(const QString& message)
{
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        info(message);
    }
    m_state = TaskState::Completed;
}

void TaskLoggingContext::fail(const QString& message)
{
    if (!message.isEmpty()) {
        m_currentMessage = message;
        error(message);
    }
    m_state = TaskState::Failed;
}

void TaskLoggingContext::cancel(const QString& message)
{
    if (!message.isEmpty()) {
        m_currentMessage = message;
        warning(message);
    }
    m_state = TaskState::Cancelled;
}

} // namespace Core::Logging
