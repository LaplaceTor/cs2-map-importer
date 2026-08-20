#pragma once

#include <QDateTime>
#include <QMutex>
#include <QString>
#include <QtGlobal>

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

    LogBlock logBlock() const;

    // Logging methods
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void log(LogLevel level, const QString& message);

    // Lifecycle methods
    void start();
    void complete(const QString& message = QString());
    void fail(const QString& message = QString());
    void cancel(const QString& message = QString());

private:
    quint64 m_taskId = 0;
    mutable QMutex m_mutex;
    QString m_taskName;
    TaskState m_state = TaskState::Pending;
    double m_progress = 0.0;
    QString m_currentMessage;
    LogBlock m_logBlock;
    quint64 m_nextSequence = 1;
};

} // namespace Core::Logging
