#include "TaskLoggingContext.h"
#include <algorithm>
#include <QMutexLocker>

namespace Core::Logging {

TaskLoggingContext::TaskLoggingContext(quint64 taskId, QString taskName)
    : m_taskId(taskId)
    , m_taskName(std::move(taskName))
    , m_logBlock(taskId)
{
}

QString TaskLoggingContext::taskName() const
{
    QMutexLocker locker(&m_mutex);
    return m_taskName;
}

void TaskLoggingContext::setTaskName(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    m_taskName = name;
}

TaskState TaskLoggingContext::state() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

double TaskLoggingContext::progress() const noexcept
{
    QMutexLocker locker(&m_mutex);
    return m_progress;
}

void TaskLoggingContext::updateProgress(double progress, const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_progress = std::clamp(progress, 0.0, 1.0);
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

QString TaskLoggingContext::currentMessage() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentMessage;
}

void TaskLoggingContext::updateCurrentMessage(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_currentMessage = message;
}

LogBlock TaskLoggingContext::logBlock() const
{
    QMutexLocker locker(&m_mutex);
    return m_logBlock;
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
    QMutexLocker locker(&m_mutex);
    LogEntry entry;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.message = message;

    m_logBlock.append(std::move(entry));
}

void TaskLoggingContext::start()
{
    QMutexLocker locker(&m_mutex);
    m_state = TaskState::Running;
}

void TaskLoggingContext::complete(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Info;
        entry.message = message;
        m_logBlock.append(std::move(entry));
    }
    m_state = TaskState::Completed;
}

void TaskLoggingContext::fail(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Error;
        entry.message = message;
        m_logBlock.append(std::move(entry));
    }
    m_state = TaskState::Failed;
}

void TaskLoggingContext::cancel(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Warning;
        entry.message = message;
        m_logBlock.append(std::move(entry));
    }
    m_state = TaskState::Cancelled;
}

} // namespace Core::Logging
