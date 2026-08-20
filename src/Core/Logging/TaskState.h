#pragma once

#include <QString>

namespace Core::Logging {

enum class TaskState {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
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
    }
    return QStringLiteral("UNKNOWN");
}

} // namespace Core::Logging
