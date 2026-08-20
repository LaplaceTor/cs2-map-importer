#include "LogManager.h"
#include <QMutexLocker>
#include <limits>

namespace Core::Logging {

LogManager& LogManager::instance()
{
    static LogManager s_instance;
    return s_instance;
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(const QString& taskName)
{
    QMutexLocker locker(&m_mutex);
    while (m_tasks.contains(m_nextTaskId) || m_nextTaskId == 0) {
        m_nextTaskId++;
    }
    quint64 id = m_nextTaskId++;
    auto context = std::make_shared<TaskLoggingContext>(id, taskName, m_defaultBlockSizeThreshold);
    m_tasks.insert(id, context);
    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(quint64 taskId, const QString& taskName)
{
    QMutexLocker locker(&m_mutex);
    if (m_tasks.contains(taskId)) {
        return nullptr;
    }

    auto context = std::make_shared<TaskLoggingContext>(taskId, taskName, m_defaultBlockSizeThreshold);
    m_tasks.insert(taskId, context);

    if (taskId >= m_nextTaskId && taskId != std::numeric_limits<quint64>::max()) {
        m_nextTaskId = taskId + 1;
    }

    return context;
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
    flushTask(taskId);
    return result;
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
    flushTask(taskId);
    return result;
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
    flushTask(taskId);
    return result;
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
    }
}

void LogManager::removeSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink) {
        return;
    }
    QMutexLocker locker(&m_mutex);
    m_sinks.removeOne(sink);
    m_sinkCursors.remove(sink->sinkId());
}

void LogManager::clearSinks()
{
    QMutexLocker locker(&m_mutex);
    m_sinks.clear();
    m_sinkCursors.clear();
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

    // Seal active block in task (Task-level lock, no LogManager global lock)
    task->flushActiveBlock();

    struct SinkWork {
        std::shared_ptr<ILogSink> sink;
        quint64 sinkId = 0;
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
        QVector<LogBlock> sealedBlocks = task->sealedBlocks();
        qsizetype totalSealedCount = sealedBlocks.size();

        for (const auto& sink : m_sinks) {
            quint64 sId = sink->sinkId();
            SinkCursor& cursor = m_sinkCursors[sId][taskId];

            if (cursor.reserved < totalSealedCount) {
                QVector<LogBlock> blocksToDispatch;
                blocksToDispatch.reserve(totalSealedCount - cursor.reserved);

                for (qsizetype i = cursor.reserved; i < totalSealedCount; ++i) {
                    blocksToDispatch.append(sealedBlocks[i]);
                }

                // Advance reserved cursor
                cursor.reserved = totalSealedCount;

                pendingWork.append(SinkWork{sink, sId, std::move(blocksToDispatch), taskName});
            }
        }
    } // Unlock LogManager::m_mutex

    if (pendingWork.isEmpty()) {
        return true;
    }

    // Phase 2: Execute Sink I/O outside of LogManager lock
    for (const auto& work : pendingWork) {
        qsizetype successCount = 0;
        bool flushOk = true;

        for (const auto& block : work.blocks) {
            if (work.sink->writeBlock(block, work.taskName)) {
                successCount++;
            } else {
                flushOk = false;
                break;
            }
        }

        if (successCount > 0 && flushOk) {
            if (!work.sink->flush()) {
                flushOk = false;
            }
        }

        // Phase 3: Transactional Commit / Rollback under LogManager lock
        QMutexLocker locker(&m_mutex);
        if (!m_sinkCursors.contains(work.sinkId)) {
            // Sink was removed during I/O; ignore state update safely
            continue;
        }

        SinkCursor& cursor = m_sinkCursors[work.sinkId][taskId];
        cursor.committed += successCount;

        if (!flushOk || successCount < work.blocks.size()) {
            // Roll back reserved cursor to committed cursor so unwritten blocks can be retried
            cursor.reserved = cursor.committed;
        }
    }

    return true;
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

qsizetype LogManager::taskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

void LogManager::clear()
{
    QMutexLocker locker(&m_mutex);
    m_tasks.clear();
    m_sinks.clear();
    m_sinkCursors.clear();
}

} // namespace Core::Logging
