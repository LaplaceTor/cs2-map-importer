#pragma once

#include <QDateTime>
#include <QMutex>
#include <QRecursiveMutex>
#include <QString>
#include <QtGlobal>
#include <functional>

#include "LogBlock.h"
#include "LogLevel.h"
#include "TaskState.h"

namespace Core::Logging {

class TaskLoggingContext {
public:
    explicit TaskLoggingContext(quint64 taskId, QString taskName = QString(), qsizetype blockSizeThreshold = 0);
    ~TaskLoggingContext() = default;

    TaskLoggingContext(const TaskLoggingContext&) = delete;
    TaskLoggingContext& operator=(const TaskLoggingContext&) = delete;
    TaskLoggingContext(TaskLoggingContext&&) noexcept = delete;
    TaskLoggingContext& operator=(TaskLoggingContext&&) noexcept = delete;

    quint64 taskId() const noexcept { return m_taskId; }

    QMutex& flushMutex() const noexcept { return m_flushMutex; }

    QString taskName() const;
    void setTaskName(const QString& name);

    TaskState state() const noexcept;

    double progress() const noexcept;
    void updateProgress(double progress, const QString& message = QString());

    QString currentMessage() const;
    void updateCurrentMessage(const QString& message);

    /**
     * @brief Get the block size threshold in bytes (estimated memory footprint).
     */
    qsizetype blockSizeThreshold() const;

    /**
     * @brief Set the block size threshold in bytes (0 means unlimited/no auto-seal).
     * Note: Size is calculated based on estimated memory footprint (estimatedByteSize).
     */
    void setBlockSizeThreshold(qsizetype bytes);

    void flushActiveBlock();

    QVector<LogBlock> sealedBlocks() const;

    /**
     * @brief Callback inspection for sealed blocks.
     * Note: Reader is executed while holding the task lock. Callers must keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    void withSealedBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const;

    QVector<LogBlock> allBlocks() const;

    /**
     * @brief Callback inspection for all blocks.
     * Note: Reader is executed while holding the task lock. Callers must keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    void withAllBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const;

    qsizetype sealedBlockCount() const;
    qsizetype totalBlockCount() const;

    /**
     * @brief Zero-copy reader callback for active log block inspection.
     * Note: Executed while holding the task lock. Callers must keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    void withLogBlock(const std::function<void(const LogBlock&)>& reader) const;

    // Explicit read-only snapshot of the current active log block
    LogBlock logBlockSnapshot() const;

    // Logging methods
    bool debug(const QString& message);
    bool info(const QString& message);
    bool warning(const QString& message);
    bool error(const QString& message);
    bool log(LogLevel level, const QString& message);

    /**
     * @brief Lifecycle state transitions:
     * Pending -> Running | Completed | Failed | Cancelled
     * Running -> Completed | Failed | Cancelled
     * Completed | Failed | Cancelled -> Terminal (non-transitionable)
     */
    bool start();
    bool complete(const QString& message = QString());
    bool fail(const QString& message = QString());
    bool cancel(const QString& message = QString());

    static bool isTerminalState(TaskState state) noexcept
    {
        return state == TaskState::Completed || state == TaskState::Failed || state == TaskState::Cancelled;
    }

private:
    void checkAndFlushActiveBlockLocked();
    void flushActiveBlockLocked();

    quint64 m_taskId = 0;
    mutable QRecursiveMutex m_mutex;
    mutable QMutex m_flushMutex;
    QString m_taskName;
    TaskState m_state = TaskState::Pending;
    double m_progress = 0.0;
    QString m_currentMessage;
    QVector<LogBlock> m_sealedBlocks;
    LogBlock m_activeBlock;
    qsizetype m_blockSizeThreshold = 0; // 0 means unlimited
    quint64 m_nextSequence = 1; // Task-local log entry sequence number
};

} // namespace Core::Logging
