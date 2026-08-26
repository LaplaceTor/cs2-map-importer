#include "TaskLoggingContext.h"
#include <algorithm>
#include <QRecursiveMutex>
#include <utility>

namespace Core::Logging {

TaskLoggingContext::TaskLoggingContext(quint64 taskId, QString taskName, qsizetype blockSizeThreshold,
                                       std::shared_ptr<FaultBarrier> faultBarrier, quint64 creationSequence,
                                       quint64 parentTaskId)
    : m_taskId(taskId)
    , m_parentTaskId(parentTaskId)
    , m_creationSequence(creationSequence)
    , m_faultBarrier(std::move(faultBarrier))
    , m_taskName(std::move(taskName))
    , m_activeBlock(taskId, 0)
    , m_blockSizeThreshold(std::max<qsizetype>(0, blockSizeThreshold))
{
}

void TaskLoggingContext::setFaultBarrier(std::shared_ptr<FaultBarrier> barrier)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (m_faultBarrier || m_state != TaskState::Pending || m_nextSequence != 1
        || !m_sealedBlocks.isEmpty() || m_activeBlock.entryCount() != 0) {
        return;
    }
    m_faultBarrier = std::move(barrier);
}

std::shared_ptr<FaultBarrier> TaskLoggingContext::faultBarrier() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_faultBarrier;
}

TaskSnapshot TaskLoggingContext::snapshot() const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    TaskSnapshot result;
    result.taskId = m_taskId;
    result.parentTaskId = m_parentTaskId;
    result.creationSequence = m_creationSequence;
    result.taskName = m_taskName;
    result.state = m_state;
    result.progress = m_progress;
    result.currentMessage = m_currentMessage;
    result.logCount = m_logCount;
    result.sealedBlockCount = m_sealedBlocks.size();
    return result;
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

QVector<LogBlock> TaskLoggingContext::sealedBlocksFrom(quint64 firstBlockIndex) const
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    QVector<LogBlock> result;
    for (const auto& block : m_sealedBlocks) {
        if (block.blockIndex() >= firstBlockIndex) {
            result.append(block);
        }
    }
    return result;
}

qsizetype TaskLoggingContext::releaseSealedBlocksBefore(quint64 exclusiveBlockIndex)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    qsizetype releaseCount = 0;
    while (releaseCount < m_sealedBlocks.size()
           && m_sealedBlocks.at(releaseCount).blockIndex() < exclusiveBlockIndex) {
        ++releaseCount;
    }
    if (releaseCount > 0) {
        m_sealedBlocks.remove(0, releaseCount);
    }
    return releaseCount;
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
    if (!m_sessionValid || isTerminalState(m_state)) {
        return false;
    }

    const LogSubmissionResult submission = submitNormalLocked();
    if (!submission.accepted()) {
        return false;
    }

    LogEntry entry;
    entry.taskId = m_taskId;
    entry.submissionSequence = submission.submissionSequence;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.message = message;

    if (!m_activeBlock.append(std::move(entry))) {
        return false;
    }
    ++m_logCount;
    if (level == LogLevel::Error || level == LogLevel::Critical) {
        ++m_errorCount;
    }
    checkAndFlushActiveBlockLocked();
    return true;
}

LogSubmissionResult TaskLoggingContext::reportFault(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || isTerminalState(m_state)) {
        return {LogSubmissionStatus::RejectedAfterTermination, 0};
    }
    if (!m_faultBarrier) {
        return {LogSubmissionStatus::RejectedAfterFault, 0};
    }

    const LogSubmissionResult result = m_faultBarrier->reportFault(
        m_taskId, QDateTime::currentMSecsSinceEpoch(), message);
    if (!result.accepted()) {
        return result;
    }

    LogEntry entry;
    entry.taskId = m_taskId;
    entry.submissionSequence = result.submissionSequence;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = LogLevel::Critical;
    entry.message = message;
    if (!m_activeBlock.append(std::move(entry))) {
        return {LogSubmissionStatus::RejectedAfterFault, result.submissionSequence};
    }
    ++m_logCount;
    ++m_errorCount;
    flushActiveBlockLocked();
    return result;
}

bool TaskLoggingContext::start()
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || m_state != TaskState::Pending) {
        return false;
    }
    m_state = TaskState::Running;
    return true;
}

bool TaskLoggingContext::complete(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || m_state != TaskState::Running) {
        return false;
    }
    m_progress = 1.0;
    if (!message.isEmpty()) {
        m_currentMessage = message;
        if (!appendLifecycleEntryLocked(LogLevel::Info, message)) {
            return false;
        }
    }
    flushActiveBlockLocked();
    m_state = TaskState::Completed;
    return true;
}

bool TaskLoggingContext::fail(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || m_state != TaskState::Running) {
        return false;
    }
    if (!message.isEmpty()) {
        m_currentMessage = message;
        if (!appendLifecycleEntryLocked(LogLevel::Error, message)) {
            return false;
        }
    }
    flushActiveBlockLocked();
    m_state = TaskState::Failed;
    return true;
}

bool TaskLoggingContext::cancel(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || m_state != TaskState::Running) {
        return false;
    }
    if (!message.isEmpty()) {
        m_currentMessage = message;
        if (!appendLifecycleEntryLocked(LogLevel::Warning, message)) {
            return false;
        }
    }
    flushActiveBlockLocked();
    m_state = TaskState::Cancelled;
    return true;
}

bool TaskLoggingContext::skip(const QString& message)
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    if (!m_sessionValid || m_state != TaskState::Running) {
        return false;
    }
    if (!message.isEmpty()) {
        m_currentMessage = message;
        if (!appendLifecycleEntryLocked(LogLevel::Info, message)) {
            return false;
        }
    }
    flushActiveBlockLocked();
    m_state = TaskState::Skipped;
    return true;
}

LogSubmissionResult TaskLoggingContext::submitNormalLocked()
{
    if (!m_faultBarrier) {
        return {LogSubmissionStatus::Accepted, 0};
    }
    return m_faultBarrier->submitNormal();
}

void TaskLoggingContext::invalidateSession()
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    m_sessionValid = false;
}

bool TaskLoggingContext::appendLifecycleEntryLocked(LogLevel level, const QString& message)
{
    const LogSubmissionResult submission = submitNormalLocked();
    if (!submission.accepted()) {
        return false;
    }

    LogEntry entry;
    entry.taskId = m_taskId;
    entry.submissionSequence = submission.submissionSequence;
    entry.sequence = m_nextSequence++;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.message = message;
    if (!m_activeBlock.append(std::move(entry))) {
        return false;
    }
    ++m_logCount;
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
    m_activeBlock = LogBlock(m_taskId, m_nextBlockIndex++);
}

quint64 TaskLoggingContext::errorCount() const noexcept
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_errorCount;
}

bool TaskLoggingContext::hasErrors() const noexcept
{
    QMutexLocker<QRecursiveMutex> locker(&m_mutex);
    return m_errorCount > 0;
}

} // namespace Core::Logging
