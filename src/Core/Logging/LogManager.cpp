#include "LogManager.h"
#include <QMutexLocker>
#include <algorithm>
#include <limits>
#include <utility>

namespace Core::Logging {

LogManager& LogManager::instance()
{
    static LogManager s_instance;
    return s_instance;
}

std::shared_ptr<FaultBarrier> LogManager::faultBarrier() const
{
    QMutexLocker locker(&m_mutex);
    return m_faultBarrier;
}

LogSubmissionResult LogManager::reportFault(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return {LogSubmissionStatus::RejectedAfterTermination, 0};
    }
    return task->reportFault(message);
}

bool LogManager::beginFaultDraining()
{
    const auto barrier = faultBarrier();
    return barrier && barrier->beginDraining();
}

bool LogManager::terminateAfterFault()
{
    const auto barrier = faultBarrier();
    if (!barrier || barrier->state() != FaultBarrierState::Draining) {
        return false;
    }

    bool flushed = true;
    for (const quint64 taskId : taskIds()) {
        flushed = flushTask(taskId) && flushed;
    }
    return flushed && barrier->terminate();
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(const QString& taskName, quint64 parentTaskId)
{
    QMutexLocker locker(&m_mutex);
    if (parentTaskId != 0 && !m_tasks.contains(parentTaskId)) {
        return nullptr;
    }

    while (m_tasks.contains(m_nextTaskId) || m_nextTaskId == 0) {
        m_nextTaskId++;
    }
    quint64 id = m_nextTaskId++;
    auto context = std::make_shared<TaskLoggingContext>(
        id, taskName, m_defaultBlockSizeThreshold, m_faultBarrier, m_nextCreationSequence++, parentTaskId);
    m_tasks.insert(id, context);
    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(quint64 taskId, const QString& taskName, quint64 parentTaskId)
{
    if (taskId == 0 || taskId == parentTaskId) {
        return nullptr;
    }

    QMutexLocker locker(&m_mutex);
    if (m_tasks.contains(taskId)) {
        return nullptr;
    }
    if (parentTaskId != 0 && !m_tasks.contains(parentTaskId)) {
        return nullptr;
    }

    auto context = std::make_shared<TaskLoggingContext>(
        taskId, taskName, m_defaultBlockSizeThreshold, m_faultBarrier, m_nextCreationSequence++, parentTaskId);
    m_tasks.insert(taskId, context);

    if (taskId >= m_nextTaskId && taskId != (std::numeric_limits<quint64>::max)()) {
        m_nextTaskId = taskId + 1;
    }

    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createChildTask(quint64 parentTaskId, const QString& taskName)
{
    if (parentTaskId == 0) {
        return nullptr;
    }
    return createTask(taskName, parentTaskId);
}

std::shared_ptr<TaskLoggingContext> LogManager::findTask(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, nullptr);
}

bool LogManager::finishTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    bool result = task->complete(message);
    bool flushOk = flushTask(taskId);
    return result && flushOk;
}

bool LogManager::failTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    bool result = task->fail(message);
    bool flushOk = flushTask(taskId);
    return result && flushOk;
}

bool LogManager::cancelTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    bool result = task->cancel(message);
    bool flushOk = flushTask(taskId);
    return result && flushOk;
}

bool LogManager::skipTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    bool result = task->skip(message);
    bool flushOk = flushTask(taskId);
    return result && flushOk;
}

bool LogManager::forceTaskState(quint64 taskId, TaskState state, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    bool result = task->forceTerminalState(state, message);
    bool flushOk = flushTask(taskId);
    return result && flushOk;
}

void LogManager::addSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink) {
        return;
    }
    QMutexLocker locker(&m_mutex);
    if (!m_sinks.contains(sink)) {
        m_sinks.append(sink);
        m_sinkCursors.insert(sink->sinkId(), QHash<quint64, SinkCursor>());
        m_sinkGenerations.insert(sink->sinkId(), m_nextSinkGeneration++);
    }
}

void LogManager::removeSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink) {
        return;
    }
    removeSink(sink->sinkId());
}

void LogManager::removeSink(quint64 sinkId)
{
    if (sinkId == 0) {
        return;
    }

    bool hasRemainingSinks = false;
    {
        QMutexLocker locker(&m_mutex);
        auto it = std::find_if(m_sinks.begin(), m_sinks.end(), [sinkId](const std::shared_ptr<ILogSink>& s) {
            return s && s->sinkId() == sinkId;
        });
        if (it == m_sinks.end()) {
            return;
        }
        m_sinks.erase(it);
        m_sinkCursors.remove(sinkId);
        m_sinkGenerations.remove(sinkId);
        hasRemainingSinks = !m_sinks.isEmpty();
    }

    // A removed sink no longer owns delivery responsibility. Re-run delivery
    // and reclamation against the remaining sinks; if none remain, all pending
    // blocks can be released because no sink can consume them anymore.
    if (hasRemainingSinks) {
        flushAll();
        return;
    }

    for (const quint64 taskId : taskIds()) {
        const auto task = findTask(taskId);
        if (task) {
            task->releaseSealedBlocksBefore((std::numeric_limits<quint64>::max)());
        }
    }
}

void LogManager::clearSinks()
{
    QVector<std::shared_ptr<TaskLoggingContext>> tasks;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
            tasks.append(it.value());
        }
        m_sinks.clear();
        m_sinkCursors.clear();
        m_sinkGenerations.clear();
    }

    // clearSinks ends all outstanding sink responsibilities. Release pending
    // blocks now rather than retaining them for a sink that no longer exists.
    for (const auto& task : tasks) {
        task->releaseSealedBlocksBefore((std::numeric_limits<quint64>::max)());
    }
}

void LogManager::flushAll()
{
    QVector<quint64> ids = taskIds();
    for (quint64 id : ids) {
        flushTask(id);
    }
}

qsizetype LogManager::defaultBlockSizeThreshold() const
{
    QMutexLocker locker(&m_mutex);
    return m_defaultBlockSizeThreshold;
}

void LogManager::setDefaultBlockSizeThreshold(qsizetype bytes)
{
    QMutexLocker locker(&m_mutex);
    m_defaultBlockSizeThreshold = std::max<qsizetype>(0, bytes);
}

QVector<LogBlock> LogManager::getSealedBlocks(quint64 taskId) const
{
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return {};
    }
    return task->sealedBlocks();
}

QVector<LogBlock> LogManager::getSealedBlocksFrom(quint64 taskId, quint64 firstBlockIndex) const
{
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return {};
    }
    return task->sealedBlocksFrom(firstBlockIndex);
}

bool LogManager::readSealedBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const
{
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return false;
    }
    task->withSealedBlocks(reader);
    return true;
}

QVector<LogBlock> LogManager::getAllBlocks(quint64 taskId) const
{
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return {};
    }
    return task->allBlocks();
}

bool LogManager::readAllBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const
{
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return false;
    }
    task->withAllBlocks(reader);
    return true;
}

bool LogManager::flushTask(quint64 taskId)
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }

    // Serialize flush operations per task across threads
    QMutexLocker taskFlushLocker(&task->flushMutex());

    // Seal active block in task (Task-level lock, no LogManager global lock)
    task->flushActiveBlock();

    struct SinkWork {
        std::shared_ptr<ILogSink> sink;
        quint64 sinkId = 0;
        quint64 generation = 0;
        QVector<LogBlock> blocks;
        QString taskName;
    };
    QVector<SinkWork> pendingWork;

    // Phase 1: Reserve unflushed blocks under LogManager lock
    {
        QMutexLocker locker(&m_mutex);
        if (m_sinks.isEmpty()) {
            return true;
        }

        QString taskName = task->taskName();
        const QVector<LogBlock> sealedBlocks = task->sealedBlocks();

        // FaultBarrier already prevents rejected ordinary entries from being
        // appended. Do not rebuild or filter sealed blocks here: a sealed block
        // is an immutable delivery unit and sink cursors use its absolute
        // blockIndex. This also preserves the accepted-before-fault contract.
        for (const auto& sink : m_sinks) {
            quint64 sId = sink->sinkId();
            SinkCursor& cursor = m_sinkCursors[sId][taskId];

            QVector<LogBlock> blocksToDispatch;
            for (const auto& block : sealedBlocks) {
                if (block.blockIndex() >= cursor.reserved) {
                    blocksToDispatch.append(block);
                }
            }

            if (!blocksToDispatch.isEmpty()) {
                cursor.reserved = blocksToDispatch.constLast().blockIndex() + 1;
                pendingWork.append(SinkWork{sink, sId, m_sinkGenerations.value(sId), std::move(blocksToDispatch), taskName});
            }
        }
    } // Unlock LogManager::m_mutex

    if (pendingWork.isEmpty()) {
        return true;
    }

    bool overallSuccess = true;

    // Phase 2: Execute Sink I/O outside of LogManager lock

    for (const auto& work : pendingWork) {
        qsizetype successCount = 0;
        bool writeAllOk = true;

        for (const auto& block : work.blocks) {
            if (work.sink->writeBlock(block, work.taskName)) {
                successCount++;
            } else {
                writeAllOk = false;
                break;
            }
        }

        bool flushOk = true;
        if (successCount > 0) {
            if (!work.sink->flush()) {
                flushOk = false;
            }
        }

        if (!writeAllOk || !flushOk) {
            overallSuccess = false;
        }

        // Phase 3: Commit the sink cursor under the LogManager lock.
        // Delivery is at-least-once: a failed batch may be retried and can be
        // duplicated by a sink that has already persisted part of the batch.
        QMutexLocker locker(&m_mutex);
        if (!m_sinkCursors.contains(work.sinkId)
            || m_sinkGenerations.value(work.sinkId) != work.generation) {
            // Sink was removed or replaced during I/O; ignore state update safely
            continue;
        }

        SinkCursor& cursor = m_sinkCursors[work.sinkId][taskId];
        if (writeAllOk && flushOk) {
            cursor.committed = cursor.reserved;
        } else {
            cursor.reserved = cursor.committed;
        }
    }

    // A sealed block can be released from the Task only after every currently
    // registered sink has committed past it. Sink cursors are absolute block
    // indices, so releasing the vector prefix does not change their position.
    {
        QMutexLocker locker(&m_mutex);
        if (!m_sinks.isEmpty()) {
            quint64 releaseBefore = (std::numeric_limits<quint64>::max)();
            for (const auto& sink : m_sinks) {
                releaseBefore = (std::min)(releaseBefore, m_sinkCursors[sink->sinkId()][taskId].committed);
            }
            if (releaseBefore != (std::numeric_limits<quint64>::max)()) {
                task->releaseSealedBlocksBefore(releaseBefore);
            }
        }
    }

    return overallSuccess;
}

bool LogManager::readLogBlock(quint64 taskId, const std::function<void(const LogBlock&)>& reader) const
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return false;
    }
    task->withLogBlock(reader);
    return true;
}

LogBlock LogManager::getLogBlockSnapshot(quint64 taskId) const
{
    std::shared_ptr<TaskLoggingContext> task;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
    }
    if (!task) {
        return LogBlock(taskId);
    }
    return task->logBlockSnapshot();
}

QVector<quint64> LogManager::taskIds() const
{
    QMutexLocker locker(&m_mutex);
    QVector<quint64> keys;
    keys.reserve(m_tasks.size());
    for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
        keys.append(it.key());
    }
    return keys;
}

QVector<TaskSnapshot> LogManager::taskSnapshots() const
{
    QMutexLocker locker(&m_mutex);
    QVector<TaskSnapshot> snapshots;
    snapshots.reserve(m_tasks.size());
    for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
        snapshots.append(it.value()->snapshot());
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const TaskSnapshot& left, const TaskSnapshot& right) {
        return left.creationSequence < right.creationSequence;
    });
    return snapshots;
}

qsizetype LogManager::taskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

void LogManager::clear()
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
        it.value()->invalidateSession();
    }

    m_tasks.clear();
    m_sinks.clear();
    m_sinkCursors.clear();
    m_sinkGenerations.clear();
    m_faultBarrier = std::make_shared<FaultBarrier>();
    m_nextTaskId = 1;
    m_nextCreationSequence = 1;
}

} // namespace Core::Logging
