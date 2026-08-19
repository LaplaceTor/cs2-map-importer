#pragma once

namespace Core::Logging {

enum class TaskState {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

} // namespace Core::Logging
