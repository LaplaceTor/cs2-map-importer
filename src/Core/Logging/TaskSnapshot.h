#pragma once

#include <QString>
#include <QtGlobal>

#include "TaskState.h"

namespace Core::Logging {

/**
 * @brief Immutable value snapshot of task metadata for non-owning consumers.
 *
 * The snapshot contains no synchronization primitives and does not expose the
 * task's mutable logging state. Consumers such as a future UI may retain it
 * and refresh it by requesting another snapshot.
 */
struct TaskSnapshot {
    quint64 taskId = 0;
    quint64 creationSequence = 0;
    QString taskName;
    TaskState state = TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    quint64 logCount = 0;
    qsizetype sealedBlockCount = 0;
};

} // namespace Core::Logging
