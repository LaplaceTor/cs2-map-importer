#pragma once

#include "TaskContext.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

namespace Core::Logging {

class LogManager {
public:
    static LogManager& instance();

    LogManager();
    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::shared_ptr<TaskContext> createTask(const QString& taskName);
    std::shared_ptr<TaskContext> findTask(quint64 taskId) const;

    void finishTask(quint64 taskId, const QString& message = QString());
    void failTask(quint64 taskId, const QString& message = QString());
    void cancelTask(quint64 taskId, const QString& message = QString());

    QVector<std::shared_ptr<TaskContext>> allTasks() const;

    quint64 nextSequence();
    void resetForTesting();

private:
    std::atomic<quint64> m_globalSequence{1};
    std::atomic<quint64> m_nextTaskId{1};

    mutable QMutex m_mutex;
    QHash<quint64, std::shared_ptr<TaskContext>> m_tasks;
};

} // namespace Core::Logging
