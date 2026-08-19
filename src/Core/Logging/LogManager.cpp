#include "LogManager.h"

namespace Core::Logging {

LogManager& LogManager::instance()
{
    static LogManager instance;
    return instance;
}

LogManager::LogManager() = default;

std::shared_ptr<TaskContext> LogManager::createTask(const QString& taskName)
{
    quint64 taskId = m_nextTaskId.fetch_add(1, std::memory_order_relaxed);

    auto task = std::make_shared<TaskContext>(
        taskId, taskName, [this]() { return nextSequence(); });

    {
        QMutexLocker locker(&m_mutex);
        m_tasks.insert(taskId, task);
    }

    return task;
}

std::shared_ptr<TaskContext> LogManager::findTask(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, nullptr);
}

void LogManager::finishTask(quint64 taskId, const QString& message)
{
    auto task = findTask(taskId);
    if (task) {
        task->complete(message);
    }
}

void LogManager::failTask(quint64 taskId, const QString& message)
{
    auto task = findTask(taskId);
    if (task) {
        task->fail(message);
    }
}

void LogManager::cancelTask(quint64 taskId, const QString& message)
{
    auto task = findTask(taskId);
    if (task) {
        task->cancel(message);
    }
}

QVector<std::shared_ptr<TaskContext>> LogManager::allTasks() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.values();
}

quint64 LogManager::nextSequence()
{
    return m_globalSequence.fetch_add(1, std::memory_order_relaxed);
}

void LogManager::resetForTesting()
{
    QMutexLocker locker(&m_mutex);
    m_tasks.clear();
    m_globalSequence.store(1, std::memory_order_relaxed);
    m_nextTaskId.store(1, std::memory_order_relaxed);
}

} // namespace Core::Logging
