#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Application::Logging {

/**
 * @brief UI-facing task lifecycle states.
 * Value order intentionally mirrors Core::Logging::TaskState; UI code must use
 * this enum (never the Core type) when rendering task state.
 */
enum class TaskState {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    Skipped
};

/**
 * @brief UI-facing log levels.
 * Value order intentionally mirrors Core::Logging::LogLevel.
 */
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

inline QString taskStateToString(TaskState state)
{
    switch (state) {
    case TaskState::Pending:
        return QStringLiteral("PENDING");
    case TaskState::Running:
        return QStringLiteral("RUNNING");
    case TaskState::Completed:
        return QStringLiteral("COMPLETED");
    case TaskState::Failed:
        return QStringLiteral("FAILED");
    case TaskState::Cancelled:
        return QStringLiteral("CANCELLED");
    case TaskState::Skipped:
        return QStringLiteral("SKIPPED");
    }
    return QStringLiteral("UNKNOWN");
}

inline QString logLevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("DEBUG");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    case LogLevel::Warning:
        return QStringLiteral("WARNING");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    case LogLevel::Critical:
        return QStringLiteral("CRITICAL");
    }
    return QStringLiteral("UNKNOWN");
}

/**
 * @brief Single log message delivered to the UI layer.
 */
struct TaskLogMessage {
    quint64 sequence = 0; // Task-local monotonic sequence number
    qint64 timestamp = 0; // UTC Unix epoch milliseconds
    LogLevel level = LogLevel::Info;
    QString message;
    quint64 toolTaskId = 0; // Non-zero when the entry originates from a hidden external tool task
};

/**
 * @brief Snapshot of a task's presentation metadata for UI rendering.
 */
struct TaskInfo {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    qint64 startTimestamp = 0; // UTC Unix epoch milliseconds
    QString taskName;
    TaskState state = TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    bool isToolTask = false;
    QString logFilePath;
    QString workflowDirectory;
    bool isValid = false; // false when the taskId does not exist in the logging system
};

} // namespace Application::Logging

Q_DECLARE_METATYPE(Application::Logging::TaskLogMessage)
Q_DECLARE_METATYPE(QVector<Application::Logging::TaskLogMessage>)
