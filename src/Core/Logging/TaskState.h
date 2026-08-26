#pragma once

#include <QString>

namespace Core::Logging {

enum class TaskState {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    Skipped
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

} // namespace Core::Logging
