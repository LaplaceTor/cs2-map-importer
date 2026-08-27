#pragma once

#include <QString>
#include <QtGlobal>
#include "LogLevel.h"
#include "LogSource.h"

namespace Core::Logging {

/**
 * @brief Represents a single log entry.
 * Note: sequence is a task-local sequence number starting from 1 for each task.
 * taskId and sequence together identify an entry within the logging system.
 */
struct LogEntry {
    quint64 taskId = 0; // Stable task identifier
    quint64 submissionSequence = 0; // Global submission order, when a FaultBarrier is attached
    quint64 sequence = 0; // Monotonically increasing sequence number within its Task
    qint64 timestamp = 0; // UTC Unix epoch milliseconds
    LogLevel level = LogLevel::Info;
    LogSource source = LogSource::Workflow;
    QString message;

    // Estimated memory footprint in bytes for size management / flush decisions
    qsizetype estimatedByteSize() const noexcept
    {
        return static_cast<qsizetype>(sizeof(LogEntry)) + message.capacity() * static_cast<qsizetype>(sizeof(QChar));
    }
};

} // namespace Core::Logging
