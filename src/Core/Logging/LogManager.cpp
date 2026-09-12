#include "LogManager.h"
#include "LogFileManager.h"
#include "TaskFileSink.h"
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <algorithm>
#include <limits>
#include <utility>

namespace Core::Logging {

namespace {

QString resolveLogPathForNewTask(
    const std::shared_ptr<TaskLoggingContext>& parentCtx,
    const QString& taskName,
    qint64 startTimestamp,
    quint64 taskId)
{
    if (parentCtx && !parentCtx->workflowDirectory().isEmpty()) {
        const QString parentDir = !parentCtx->taskDirectory().isEmpty()
            ? parentCtx->taskDirectory()
            : parentCtx->workflowDirectory();
        const QString assetBaseName = parentCtx->assetBaseName();
        // Tool command lines may quote the executable path (paths with spaces),
        // so parse the executable token quote-aware.
        QString toolToken;
        if (taskName.startsWith(QLatin1Char('"'))) {
            const qsizetype endIdx = taskName.indexOf(QLatin1Char('"'), 1);
            toolToken = (endIdx > 0) ? taskName.mid(1, endIdx - 1) : taskName.mid(1);
        } else {
            toolToken = taskName.section(QLatin1Char(' '), 0, 0);
        }
        toolToken = toolToken.trimmed();
        QString toolName = QFileInfo(toolToken).completeBaseName();
        if (toolName.isEmpty()) {
            toolName = QStringLiteral("tool");
        }
        return LogFileManager::generateToolLogFilePath(parentDir, assetBaseName, toolName, startTimestamp);
    }
    return LogFileManager::generateTaskLogFilePath(taskName, startTimestamp, taskId);
}

} // namespace

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

std::shared_ptr<TaskLoggingContext> LogManager::createWorkflowTask(const QString& workflowName, const QString& assetBaseName)
{
    std::shared_ptr<TaskLoggingContext> context;
    QVector<std::shared_ptr<ILogSink>> sinks;
    QString logPath;
    QString workflowDir;
    quint64 id = 0;
    {
        QMutexLocker locker(&m_mutex);
        while (m_tasks.contains(m_nextTaskId) || m_nextTaskId == 0) {
            m_nextTaskId++;
        }
        id = m_nextTaskId++;
        context = std::make_shared<TaskLoggingContext>(
            id, workflowName, m_defaultBlockSizeThreshold, m_faultBarrier, m_nextCreationSequence++, 0);
        workflowDir = LogFileManager::generateWorkflowDirectoryPath(workflowName, context->startTimestamp());
        logPath = LogFileManager::generateWorkflowLogFilePath(workflowDir);
        context->setWorkflowDirectory(workflowDir);
        context->setTaskDirectory(workflowDir);
        context->setAssetBaseName(assetBaseName);
        context->setIsWorkflow(true);
        context->setLogFilePath(logPath);
        m_tasks.insert(id, context);
        sinks = m_sinks;
    }

    bool logFileReady = false;
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskCreated(id, workflowName, context->startTimestamp(), logPath);
            if (auto fileSink = std::dynamic_pointer_cast<TaskFileSink>(sink)) {
                if (fileSink->hasTaskLogFile(id)) {
                    logFileReady = true;
                }
            }
        }
    }
    context->setLogFileReady(logFileReady);
    context->setFlushCallback([](quint64 tId) {
        LogManager::instance().flushTask(tId);
    });
    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(const QString& taskName, quint64 parentTaskId)
{
    std::shared_ptr<TaskLoggingContext> context;
    QVector<std::shared_ptr<ILogSink>> sinks;
    QString logPath;
    quint64 id = 0;
    {
        QMutexLocker locker(&m_mutex);
        std::shared_ptr<TaskLoggingContext> parentCtx;
        if (parentTaskId != 0) {
            if (!m_tasks.contains(parentTaskId)) {
                return nullptr;
            }
            parentCtx = m_tasks.value(parentTaskId);
        }

        while (m_tasks.contains(m_nextTaskId) || m_nextTaskId == 0) {
            m_nextTaskId++;
        }
        id = m_nextTaskId++;
        context = std::make_shared<TaskLoggingContext>(
            id, taskName, m_defaultBlockSizeThreshold, m_faultBarrier, m_nextCreationSequence++, parentTaskId);
        if (parentCtx && !parentCtx->workflowDirectory().isEmpty()) {
            context->setWorkflowDirectory(parentCtx->workflowDirectory());
            context->setAssetBaseName(parentCtx->assetBaseName());
            const QString parentDir = !parentCtx->taskDirectory().isEmpty()
                ? parentCtx->taskDirectory()
                : parentCtx->workflowDirectory();
            const QString sanitizedTaskName = LogFileManager::sanitizeFileName(taskName);
            context->setTaskDirectory(QDir(parentDir).filePath(sanitizedTaskName));
        }
        logPath = resolveLogPathForNewTask(parentCtx, taskName, context->startTimestamp(), id);
        context->setLogFilePath(logPath);
        m_tasks.insert(id, context);
        if (parentTaskId != 0) {
            m_childTaskIds.insert(parentTaskId, id);
        }
        sinks = m_sinks;
    }

    bool logFileReady = false;
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskCreated(id, taskName, context->startTimestamp(), logPath);
            if (auto fileSink = std::dynamic_pointer_cast<TaskFileSink>(sink)) {
                if (fileSink->hasTaskLogFile(id)) {
                    logFileReady = true;
                }
            }
        }
    }
    context->setLogFileReady(logFileReady);
    context->setFlushCallback([](quint64 tId) {
        LogManager::instance().flushTask(tId);
    });
    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createTask(quint64 taskId, const QString& taskName, quint64 parentTaskId)
{
    if (taskId == 0 || taskId == parentTaskId) {
        return nullptr;
    }

    std::shared_ptr<TaskLoggingContext> context;
    QVector<std::shared_ptr<ILogSink>> sinks;
    QString logPath;
    {
        QMutexLocker locker(&m_mutex);
        if (m_tasks.contains(taskId)) {
            return nullptr;
        }
        std::shared_ptr<TaskLoggingContext> parentCtx;
        if (parentTaskId != 0) {
            if (!m_tasks.contains(parentTaskId)) {
                return nullptr;
            }
            parentCtx = m_tasks.value(parentTaskId);
        }

        context = std::make_shared<TaskLoggingContext>(
            taskId, taskName, m_defaultBlockSizeThreshold, m_faultBarrier, m_nextCreationSequence++, parentTaskId);
        if (parentCtx && !parentCtx->workflowDirectory().isEmpty()) {
            context->setWorkflowDirectory(parentCtx->workflowDirectory());
            context->setAssetBaseName(parentCtx->assetBaseName());
            const QString parentDir = !parentCtx->taskDirectory().isEmpty()
                ? parentCtx->taskDirectory()
                : parentCtx->workflowDirectory();
            const QString sanitizedTaskName = LogFileManager::sanitizeFileName(taskName);
            context->setTaskDirectory(QDir(parentDir).filePath(sanitizedTaskName));
        }
        logPath = resolveLogPathForNewTask(parentCtx, taskName, context->startTimestamp(), taskId);
        context->setLogFilePath(logPath);
        m_tasks.insert(taskId, context);
        if (parentTaskId != 0) {
            m_childTaskIds.insert(parentTaskId, taskId);
        }

        if (taskId >= m_nextTaskId && taskId != (std::numeric_limits<quint64>::max)()) {
            m_nextTaskId = taskId + 1;
        }
        sinks = m_sinks;
    }

    bool logFileReady = false;
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskCreated(taskId, taskName, context->startTimestamp(), logPath);
            if (auto fileSink = std::dynamic_pointer_cast<TaskFileSink>(sink)) {
                if (fileSink->hasTaskLogFile(taskId)) {
                    logFileReady = true;
                }
            }
        }
    }
    context->setLogFileReady(logFileReady);
    context->setFlushCallback([](quint64 tId) {
        LogManager::instance().flushTask(tId);
    });

    return context;
}

std::shared_ptr<TaskLoggingContext> LogManager::createChildTask(quint64 parentTaskId, const QString& taskName)
{
    if (parentTaskId == 0) {
        return nullptr;
    }
    return createTask(taskName, parentTaskId);
}

std::shared_ptr<TaskLoggingContext> LogManager::createToolTask(
    quint64 parentTaskId, const QString& commandLine, const QString& assetBaseName)
{
    if (parentTaskId == 0) {
        return nullptr;
    }

    auto toolTask = createTask(commandLine, parentTaskId);
    if (!toolTask) {
        return nullptr;
    }

    toolTask->setIsToolTask(true);
    if (!assetBaseName.isEmpty()) {
        toolTask->setAssetBaseName(assetBaseName);
    }

    // Automatically log command execution in parent task
    if (auto parent = findTask(parentTaskId)) {
        parent->command(commandLine, toolTask->taskId());
        flushTask(parentTaskId);
    }

    return toolTask;
}

std::shared_ptr<TaskLoggingContext> LogManager::findTask(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, nullptr);
}

bool LogManager::finishTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
        sinks = m_sinks;
    }
    if (!task) {
        return false;
    }
    if (!task->complete(message)) {
        return false; // Illegal transition: no flush, no termination notification
    }
    bool flushOk = flushTask(taskId);
    {
        QMutexLocker locker(&m_mutex);
        dropTaskCursorsLocked(taskId);
    }
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskTerminated(taskId, TaskState::Completed);
        }
    }
    return flushOk;
}

bool LogManager::failTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
        sinks = m_sinks;
    }
    if (!task) {
        return false;
    }
    if (!task->fail(message)) {
        return false; // Illegal transition: no flush, no termination notification
    }
    bool flushOk = flushTask(taskId);
    {
        QMutexLocker locker(&m_mutex);
        dropTaskCursorsLocked(taskId);
    }
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskTerminated(taskId, TaskState::Failed);
        }
    }
    return flushOk;
}

bool LogManager::cancelTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
        sinks = m_sinks;
    }
    if (!task) {
        return false;
    }
    if (!task->cancel(message)) {
        return false; // Illegal transition: no flush, no termination notification
    }
    bool flushOk = flushTask(taskId);
    {
        QMutexLocker locker(&m_mutex);
        dropTaskCursorsLocked(taskId);
    }

    // Cascade cancellation to descendant tasks (e.g. hidden tool tasks) so the log
    // tree cannot show a cancelled parent with still-running children.
    QVector<quint64> descendants;
    {
        QMutexLocker locker(&m_mutex);
        collectDescendantsLocked(taskId, descendants);
    }
    for (const quint64 childId : descendants) {
        cancelTask(childId, message);
    }

    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskTerminated(taskId, TaskState::Cancelled);
        }
    }
    return flushOk;
}

bool LogManager::skipTask(quint64 taskId, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
        sinks = m_sinks;
    }
    if (!task) {
        return false;
    }
    if (!task->skip(message)) {
        return false; // Illegal transition: no flush, no termination notification
    }
    bool flushOk = flushTask(taskId);
    {
        QMutexLocker locker(&m_mutex);
        dropTaskCursorsLocked(taskId);
    }
    for (const auto& sink : sinks) {
        if (sink) {
            sink->onTaskTerminated(taskId, TaskState::Skipped);
        }
    }
    return flushOk;
}

bool LogManager::forceTaskState(quint64 taskId, TaskState state, const QString& message)
{
    std::shared_ptr<TaskLoggingContext> task;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        task = m_tasks.value(taskId, nullptr);
        sinks = m_sinks;
    }
    if (!task) {
        return false;
    }
    const TaskState previousState = task->state();
    if (!task->forceTerminalState(state, message)) {
        return false;
    }
    // Idempotent re-force (same terminal state): state and message are already
    // recorded; skip repeated flush/termination notifications.
    if (previousState == task->state()) {
        return true;
    }
    bool flushOk = flushTask(taskId);
    {
        QMutexLocker locker(&m_mutex);
        dropTaskCursorsLocked(taskId);
    }
    if (TaskLoggingContext::isTerminalState(state)) {
        for (const auto& sink : sinks) {
            if (sink) {
                sink->onTaskTerminated(taskId, state);
            }
        }
    }
    return flushOk;
}

void LogManager::addSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink) {
        return;
    }
    QVector<std::shared_ptr<TaskLoggingContext>> tasks;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_sinks.contains(sink)) {
            m_sinks.append(sink);
            m_sinkCursors.insert(sink->sinkId(), QHash<quint64, SinkCursor>());
            m_sinkGenerations.insert(sink->sinkId(), m_nextSinkGeneration++);

            tasks.reserve(m_tasks.size());
            for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
                tasks.append(it.value());
            }
        }
    }

    for (const auto& task : tasks) {
        if (task && !TaskLoggingContext::isTerminalState(task->state())) {
            if (sink->onTaskCreated(task->taskId(), task->taskName(), task->startTimestamp(), task->logFilePath())) {
                if (auto fileSink = std::dynamic_pointer_cast<TaskFileSink>(sink)) {
                    if (fileSink->hasTaskLogFile(task->taskId())) {
                        task->setLogFileReady(true);
                    }
                }
            }
        }
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

    // Update log file readiness across remaining sinks
    for (const quint64 taskId : taskIds()) {
        const auto task = findTask(taskId);
        if (task) {
            bool ready = false;
            {
                QMutexLocker locker(&m_mutex);
                for (const auto& s : m_sinks) {
                    if (auto fileSink = std::dynamic_pointer_cast<TaskFileSink>(s)) {
                        if (fileSink->hasTaskLogFile(taskId)) {
                            ready = true;
                            break;
                        }
                    }
                }
            }
            task->setLogFileReady(ready);
        }
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
        if (task) {
            task->setLogFileReady(false);
            task->releaseSealedBlocksBefore((std::numeric_limits<quint64>::max)());
        }
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
            // Sealed blocks are intentionally RETAINED for inspection APIs
            // (getSealedBlocks/getAllBlocks must keep serving the task history even
            // without sinks). Retention stays bounded in practice: terminal flushes
            // release committed blocks and removing the last sink releases the rest.
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
    // 1. Snapshot sinks and tasks under lock
    QVector<std::shared_ptr<TaskLoggingContext>> tasks;
    QVector<std::shared_ptr<ILogSink>> sinks;
    {
        QMutexLocker locker(&m_mutex);
        tasks.reserve(m_tasks.size());
        for (auto it = m_tasks.constBegin(); it != m_tasks.constEnd(); ++it) {
            tasks.append(it.value());
        }
        sinks = m_sinks;
    }

    // 2. Transition any unfinished (Pending or Running) tasks to Cancelled
    for (const auto& task : tasks) {
        if (task && !TaskLoggingContext::isTerminalState(task->state())) {
            task->forceTerminalState(TaskState::Cancelled, QStringLiteral("Logging session reset"));
        }
    }

    // 3. Flush all tasks so all sealed blocks reach sinks
    flushAll();

    // 4. Notify sinks of task termination outside of mutex lock
    for (const auto& task : tasks) {
        if (task) {
            for (const auto& sink : sinks) {
                if (sink) {
                    sink->onTaskTerminated(task->taskId(), task->state());
                }
            }
        }
    }

    // 5. Invalidate sessions and clear internal state under lock
    {
        QMutexLocker locker(&m_mutex);
        for (const auto& task : tasks) {
            if (task) {
                task->invalidateSession();
            }
        }

        m_tasks.clear();
        m_childTaskIds.clear();
        m_sinks.clear();
        m_sinkCursors.clear();
        m_sinkGenerations.clear();
        m_faultBarrier = std::make_shared<FaultBarrier>();
        // m_nextTaskId is intentionally NOT reset: external holders may still carry
        // task ids from the previous session, and reusing them would misroute their
        // late reports onto newly created tasks.
        m_nextCreationSequence = 1;
    }
}

void LogManager::dropTaskCursorsLocked(quint64 taskId)
{
    // Caller must hold m_mutex. Terminal tasks can no longer produce blocks, so
    // their per-sink cursors are dropped to keep the registry bounded across a
    // long-running session.
    for (auto it = m_sinkCursors.begin(); it != m_sinkCursors.end(); ++it) {
        it.value().remove(taskId);
    }
}

void LogManager::collectDescendantsLocked(quint64 taskId, QVector<quint64>& out) const
{
    // Caller must hold m_mutex. Breadth-first traversal of the parent -> child
    // registry; the out-contains guard also protects against registry cycles.
    QVector<quint64> frontier{taskId};
    while (!frontier.isEmpty()) {
        QVector<quint64> next;
        for (const quint64 id : frontier) {
            const QList<quint64> children = m_childTaskIds.values(id);
            for (const quint64 child : children) {
                if (!out.contains(child)) {
                    out.append(child);
                    next.append(child);
                }
            }
        }
        frontier = std::move(next);
    }
}

} // namespace Core::Logging
