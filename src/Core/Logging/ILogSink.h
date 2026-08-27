#pragma once

#include <QString>
#include <QtGlobal>
#include <atomic>
#include "LogBlock.h"
#include "TaskState.h"

namespace Core::Logging {

/**
 * @brief Abstract interface for logging sinks.
 * Sinks process sealed LogBlocks produced by tasks and receive task lifecycle notifications.
 */
class ILogSink {
public:
    explicit ILogSink(quint64 sinkId = 0)
        : m_sinkId(sinkId != 0 ? sinkId : generateNextSinkId())
    {
    }

    virtual ~ILogSink() = default;

    /**
     * @brief Unique identifier for this sink instance.
     */
    quint64 sinkId() const noexcept { return m_sinkId; }

    /**
     * @brief Lifecycle callback invoked when a new task is created.
     */
    virtual void onTaskCreated(quint64 taskId, const QString& taskName, qint64 startTimestamp, const QString& logFilePath)
    {
        Q_UNUSED(taskId);
        Q_UNUSED(taskName);
        Q_UNUSED(startTimestamp);
        Q_UNUSED(logFilePath);
    }

    /**
     * @brief Lifecycle callback invoked when a task reaches a terminal state (Completed, Failed, Cancelled, Skipped).
     */
    virtual void onTaskTerminated(quint64 taskId, TaskState state)
    {
        Q_UNUSED(taskId);
        Q_UNUSED(state);
    }

    /**
     * @brief Writes a sealed LogBlock to the sink.
     * @param block The sealed LogBlock containing entries.
     * @param taskName The name of the task associated with the block.
     * @return true if block was written successfully, false on I/O error.
     */
    virtual bool writeBlock(const LogBlock& block, const QString& taskName) = 0;

    /**
     * @brief Flushes any buffered output in the sink.
     * @return true if flushed successfully, false on I/O error.
     */
    virtual bool flush() = 0;

private:
    static quint64 generateNextSinkId() noexcept
    {
        static std::atomic<quint64> s_nextSinkId{1};
        return s_nextSinkId.fetch_add(1, std::memory_order_relaxed);
    }

    quint64 m_sinkId = 0;
};

} // namespace Core::Logging
