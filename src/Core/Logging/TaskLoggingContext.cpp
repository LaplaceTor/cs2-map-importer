#include "TaskLoggingContext.h"

namespace Core::Logging {

TaskLoggingContext::TaskLoggingContext(quint64 taskId, QString taskName)
    : m_taskId(taskId)
    , m_taskName(std::move(taskName))
    , m_logBlock(taskId)
{
}

void TaskLoggingContext::updateProgress(double progress, const QString& message)
{
    m_progress = progress;
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

void TaskLoggingContext::startTask()
{
    start();
}

void TaskLoggingContext::complete(const QString& message)
{
    m_state = TaskState::Completed;
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        info(message);
    }
}

void TaskLoggingContext::completeTask(const QString& message)
{
    complete(message);
}

void TaskLoggingContext::fail(const QString& message)
{
    m_state = TaskState::Failed;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        error(message);
    }
}

void TaskLoggingContext::failTask(const QString& message)
{
    fail(message);
}

void TaskLoggingContext::cancel(const QString& message)
{
    m_state = TaskState::Cancelled;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        warning(message);
    }
}

void TaskLoggingContext::cancelTask(const QString& message)
{
    cancel(message);
}

} // namespace Core::Logging
