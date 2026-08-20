#include "TaskLoggingContext.h"
#include <algorithm>
#include <QRecursiveMutex>

namespace Core::Logging {

TaskLoggingContext::TaskLoggingContext(quint64 taskId, QString taskName, qsizetype blockSizeThreshold)
    : m_taskId(taskId)
    , m_taskName(std::move(taskName))
    , m_activeBlock(taskId, 0)
    , m_blockSizeThreshold(std::max<qsizetype>(0, blockSizeThreshold))
{
}

QString TaskLoggingContext::taskName() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_taskName;
}

void TaskLoggingContext::setTaskName(const QString& name)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    m_taskName = name;
}

TaskState TaskLoggingContext::state() const noexcept
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_state;
}

double TaskLoggingContext::progress() const noexcept
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_progress;
}

void TaskLoggingContext::updateProgress(double progress, const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    m_progress = std::clamp(progress, 0.0, 1.0);
    if (!message.isEmpty()) {
        m_currentMessage = message;
    }
}

QString TaskLoggingContext::currentMessage() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_currentMessage;
}

void TaskLoggingContext::updateCurrentMessage(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    m_currentMessage = message;
}

qsizetype TaskLoggingContext::blockSizeThreshold() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_blockSizeThreshold;
}

void TaskLoggingContext::setBlockSizeThreshold(qsizetype bytes)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    m_blockSizeThreshold = std::max<qsizetype>(0, bytes);
    checkAndFlushActiveBlockLocked();
}

void TaskLoggingContext::flushActiveBlock()
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    flushActiveBlockLocked();
}

QVector<LogBlock> TaskLoggingContext::sealedBlocks() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_sealedBlocks;
}

void TaskLoggingContext::withSealedBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const
{
    if (!reader) {
        return;
    }
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    reader(m_sealedBlocks);
}

QVector<LogBlock> TaskLoggingContext::allBlocks() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    QVector<LogBlock> result = m_sealedBlocks;
    if (m_activeBlock.entryCount() > 0 || result.isEmpty()) {
        result.append(m_activeBlock);
    }
    return result;
}

void TaskLoggingContext::withAllBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const
{
    if (!reader) {
        return;
    }
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    QVector<LogBlock> result = m_sealedBlocks;
    if (m_activeBlock.entryCount() > 0 || result.isEmpty()) {
        result.append(m_activeBlock);
    }
    reader(result);
}

qsizetype TaskLoggingContext::sealedBlockCount() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_sealedBlocks.size();
}

qsizetype TaskLoggingContext::totalBlockCount() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (m_activeBlock.entryCount() > 0 || m_sealedBlocks.isEmpty()) {
        return m_sealedBlocks.size() + 1;
    }
    return m_sealedBlocks.size();
}

void TaskLoggingContext::withLogBlock(const std::function<void(const LogBlock&)>& reader) const
{
    if (!reader) {
        return;
    }
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    reader(m_activeBlock);
}

LogBlock TaskLoggingContext::logBlockSnapshot() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_activeBlock;
}

bool TaskLoggingContext::debug(const QString& message)
{
    return log(LogLevel::Debug, message);
}

bool TaskLoggingContext::info(const QString& message)
{
    return log(LogLevel::Info, message);
}

bool TaskLoggingContext::warning(const QString& message)
{
    return log(LogLevel::Warning, message);
}

bool TaskLoggingContext::error(const QString& message)
{
    return log(LogLevel::Error, message);
}

bool TaskLoggingContext::log(LogLevel level, const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (isTerminalState(m_state)) {
        return false;
    }

    LogEntry entry;
    entry.taskId = m_taskId;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.message = message;

    if (!m_activeBlock.append(std::move(entry))) {
        return false;
    }
    checkAndFlushActiveBlockLocked();
    return true;
}

bool TaskLoggingContext::start()
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (isTerminalState(m_state)) {
        return false;
    }
    m_state = TaskState::Running;
    return true;
}

bool TaskLoggingContext::complete(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (isTerminalState(m_state)) {
        return false;
    }
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.taskId = m_taskId;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Info;
        entry.message = message;
        m_activeBlock.append(std::move(entry));
    }
    flushActiveBlockLocked();
    m_state = TaskState::Completed;
    return true;
}

bool TaskLoggingContext::fail(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (isTerminalState(m_state)) {
        return false;
    }
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.taskId = m_taskId;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Error;
        entry.message = message;
        m_activeBlock.append(std::move(entry));
    }
    flushActiveBlockLocked();
    m_state = TaskState::Failed;
    return true;
}

bool TaskLoggingContext::cancel(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (isTerminalState(m_state)) {
        return false;
    }
    if (!message.isEmpty()) {
        m_currentMessage = message;
        LogEntry entry;
        entry.taskId = m_taskId;
        entry.sequence = m_nextSequence++;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.level = LogLevel::Warning;
        entry.message = message;
        m_activeBlock.append(std::move(entry));
    }
    flushActiveBlockLocked();
    m_state = TaskState::Cancelled;
    return true;
}

void TaskLoggingContext::checkAndFlushActiveBlockLocked()
{
    if (m_blockSizeThreshold > 0 && m_activeBlock.size() >= m_blockSizeThreshold) {
        flushActiveBlockLocked();
    }
}

void TaskLoggingContext::flushActiveBlockLocked()
{
    if (m_activeBlock.entryCount() == 0) {
        return;
    }
    m_activeBlock.seal();
    m_sealedBlocks.append(std::move(m_activeBlock));
    quint64 nextBlockIndex = static_cast<quint64>(m_sealedBlocks.size());
    m_activeBlock = LogBlock(m_taskId, nextBlockIndex);
}

} // namespace Core::Logging
