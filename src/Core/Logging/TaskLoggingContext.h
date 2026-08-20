#pragma once

#include <QDateTime>
#include <QMutex>
#include <QString>
#include <QtGlobal>
#include <functional>

#include "LogBlock.h"
#include "LogLevel.h"
#include "TaskState.h"

namespace Core::Logging {

class TaskLoggingContext {
public:
    explicit TaskLoggingContext(quint64 taskId, QString taskName = QString());
    ~TaskLoggingContext() = default;

    TaskLoggingContext(const TaskLoggingContext&) = delete;
    TaskLoggingContext& operator=(const TaskLoggingContext&) = delete;
    TaskLoggingContext(TaskLoggingContext&&) noexcept = delete;
    TaskLoggingContext& operator=(TaskLoggingContext&&) noexcept = delete;

    quint64 taskId() const noexcept { return m_taskId; }

    QString taskName() const;
    void setTaskName(const QString& name);

    TaskState state() const noexcept;

    double progress() const noexcept;
    void updateProgress(double progress, const QString& message = QString());

    QString currentMessage() const;
    void updateCurrentMessage(const QString& message);

    // Zero-copy reader callback for log block inspection
    void withLogBlock(const std::function<void(const LogBlock&)>& reader) const;

    // Explicit read-only snapshot of the current log block
    LogBlock logBlockSnapshot() const;

    // Logging methods
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void log(LogLevel level, const QString& message);

    // Lifecycle methods with state machine enforcement:
    // Pending -> Running -> (Completed | Failed | Cancelled)
    bool start();
    bool complete(const QString& message = QString());
    bool fail(const QString& message = QString());
    bool cancel(const QString& message = QString());

    static bool isTerminalState(TaskState state) noexcept
    {
        return state == TaskState::Completed || state == TaskState::Failed || state == TaskState::Cancelled;
    }

private:
    quint64 m_taskId = 0;
    mutable QMutex m_mutex;
    QString m_taskName;
    TaskState m_state = TaskState::Pending;
    double m_progress = 0.0;
    QString m_currentMessage;
    LogBlock m_logBlock;
    quint64 m_nextSequence = 1; // Task-local log entry sequence number
};

} // namespace Core::Logging
