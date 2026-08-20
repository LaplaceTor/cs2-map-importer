#include "LogManager.h"
#include <QMutexLocker>

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
    task->complete(message);
    return true;
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
    task->fail(message);
    return true;
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
    task->cancel(message);
    return true;
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
    std::shared_ptr<TaskLoggingContext> task = findTask(taskId);
    if (!task) {
        return false;
    }
    task->flushActiveBlock();
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
    m_defaultBlockSizeThreshold = 0;
}

} // namespace Core::Logging
