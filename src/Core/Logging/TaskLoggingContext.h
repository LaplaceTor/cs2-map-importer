#pragma once

#include <QDateTime>
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

    // Move construct / assign allowed; copy disabled because LogBlock is non-copyable
    TaskLoggingContext(const TaskLoggingContext&) = delete;
    TaskLoggingContext& operator=(const TaskLoggingContext&) = delete;
    TaskLoggingContext(TaskLoggingContext&&) noexcept = default;
    TaskLoggingContext& operator=(TaskLoggingContext&&) noexcept = default;

    quint64 taskId() const noexcept { return m_taskId; }

    QString taskName() const { return m_taskName; }
    void setTaskName(const QString& name) { m_taskName = name; }

    TaskState state() const noexcept { return m_state; }
    void setState(TaskState state) noexcept { m_state = state; }

    double progress() const noexcept { return m_progress; }
    void setProgress(double progress) noexcept { m_progress = progress; }
    void updateProgress(double progress, const QString& message = QString());

    QString currentMessage() const { return m_currentMessage; }
    void setCurrentMessage(const QString& message) { m_currentMessage = message; }
    void updateCurrentMessage(const QString& message) { m_currentMessage = message; }

    const LogBlock& logBlock() const noexcept { return m_logBlock; }
    LogBlock& logBlock() noexcept { return m_logBlock; }

    // Logging methods
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void log(LogLevel level, const QString& message);

    // Lifecycle methods
    void start();
    void startTask();

    void complete(const QString& message = QString());
    void completeTask(const QString& message = QString());

    void fail(const QString& message = QString());
    void failTask(const QString& message = QString());

    void cancel(const QString& message = QString());
    void cancelTask(const QString& message = QString());

private:
    quint64 m_taskId = 0;
    QString m_taskName;
    TaskState m_state = TaskState::Pending;
    double m_progress = 0.0;
    QString m_currentMessage;
    LogBlock m_logBlock;
    quint64 m_nextSequence = 1;
};

} // namespace Core::Logging
