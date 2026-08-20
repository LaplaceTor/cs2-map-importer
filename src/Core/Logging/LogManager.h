#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

#include "LogBlock.h"
#include "TaskLoggingContext.h"

namespace Core::Logging {

class LogManager {
public:
    LogManager() = default;
    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    static LogManager& instance();

    std::shared_ptr<TaskLoggingContext> createTask(const QString& taskName = QString());
    std::shared_ptr<TaskLoggingContext> createTask(quint64 taskId, const QString& taskName = QString());

    std::shared_ptr<TaskLoggingContext> findTask(quint64 taskId) const;

    bool finishTask(quint64 taskId, const QString& message = QString());
    bool failTask(quint64 taskId, const QString& message = QString());
    bool cancelTask(quint64 taskId, const QString& message = QString());

    LogBlock getLogBlock(quint64 taskId) const;
    QVector<quint64> taskIds() const;
    qsizetype taskCount() const;
    void clear();

private:
    mutable QMutex m_mutex;
    QHash<quint64, std::shared_ptr<TaskLoggingContext>> m_tasks;
    std::atomic<quint64> m_nextTaskId{1};
};

} // namespace Core::Logging
