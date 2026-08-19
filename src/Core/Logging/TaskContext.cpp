#include "TaskContext.h"

#include <algorithm>

namespace Core::Logging {

TaskContext::TaskContext(quint64 taskId, QString taskName, SequenceGenerator sequenceGenerator)
    : m_taskId(taskId)
    , m_taskName(std::move(taskName))
    , m_sequenceGenerator(std::move(sequenceGenerator))
{
}

quint64 TaskContext::taskId() const
{
    QMutexLocker locker(&m_mutex);
    return m_taskId;
}

QString TaskContext::taskName() const
{
    QMutexLocker locker(&m_mutex);
    return m_taskName;
}

TaskState TaskContext::state() const
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

double TaskContext::progress() const
{
    QMutexLocker locker(&m_mutex);
    return m_progress;
}

QString TaskContext::currentMessage() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentMessage;
}

bool TaskContext::isFinished() const
{
    QMutexLocker locker(&m_mutex);
    return m_state == TaskState::Completed || m_state == TaskState::Failed || m_state == TaskState::Cancelled;
}

void TaskContext::setRotationThresholds(qsizetype maxEntries, qsizetype maxBytes)
{
    QMutexLocker locker(&m_mutex);
    m_maxEntries = maxEntries;
    m_maxBytes = maxBytes;
}

qsizetype TaskContext::maxEntries() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxEntries;
}

qsizetype TaskContext::maxBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxBytes;
}

void TaskContext::setProgress(double progress)
{
    QMutexLocker locker(&m_mutex);
    m_progress = std::clamp(progress, 0.0, 1.0);
}

void TaskContext::setCurrentMessage(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_currentMessage = message;
}

void TaskContext::setState(TaskState state)
{
    QMutexLocker locker(&m_mutex);
    m_state = state;
}

void TaskContext::complete(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_state = TaskState::Completed;
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

void TaskContext::fail(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_state = TaskState::Failed;
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

void TaskContext::cancel(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    m_state = TaskState::Cancelled;
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

void TaskContext::debug(const QString& message)
{
    log(LogLevel::Debug, message);
}

void TaskContext::info(const QString& message)
{
    log(LogLevel::Info, message);
}

void TaskContext::warning(const QString& message)
{
    log(LogLevel::Warning, message);
}

void TaskContext::error(const QString& message)
{
    log(LogLevel::Error, message);
}

void TaskContext::critical(const QString& message)
{
    log(LogLevel::Critical, message);
}

void TaskContext::log(LogLevel level, const QString& message)
{
    QMutexLocker locker(&m_mutex);

    LogEntry entry;
    entry.sequence = nextSequence();
    entry.timestamp = QDateTime::currentDateTimeUtc();
    entry.level = level;
    entry.taskId = m_taskId;
    entry.message = message;

    if (m_blocks.isEmpty()) {
        m_blocks.append(LogBlock());
    }

    LogBlock* activeBlock = &m_blocks.last();
    if (activeBlock->isSealed() || activeBlock->entryCount() >= m_maxEntries || activeBlock->size() >= m_maxBytes) {
        activeBlock->seal();
        m_blocks.append(LogBlock());
        activeBlock = &m_blocks.last();
    }

    activeBlock->append(entry);
}

QVector<LogBlock> TaskContext::blocks() const
{
    QMutexLocker locker(&m_mutex);
    return m_blocks;
}

QVector<LogEntry> TaskContext::allEntries() const
{
    QMutexLocker locker(&m_mutex);
    QVector<LogEntry> entries;
    for (const auto& block : m_blocks) {
        entries.append(block.entries());
    }
    return entries;
}

qsizetype TaskContext::totalEntryCount() const
{
    QMutexLocker locker(&m_mutex);
    qsizetype count = 0;
    for (const auto& block : m_blocks) {
        count += block.entryCount();
    }
    return count;
}

quint64 TaskContext::nextSequence()
{
    if (m_sequenceGenerator) {
        return m_sequenceGenerator();
    }
    return m_fallbackSequence++;
}

} // namespace Core::Logging
