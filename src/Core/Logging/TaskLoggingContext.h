#pragma once

#include <QDateTime>
#include <QMutex>
#include <QRecursiveMutex>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>

#include "FaultBarrier.h"
#include "LogBlock.h"
#include "LogLevel.h"
#include "TaskSnapshot.h"

namespace Core::Logging {

class TaskLoggingContext {
public:
    explicit TaskLoggingContext(quint64 taskId, QString taskName = QString(), qsizetype blockSizeThreshold = 0,
                                std::shared_ptr<FaultBarrier> faultBarrier = nullptr,
                                quint64 creationSequence = 0,
                                quint64 parentTaskId = 0);
    ~TaskLoggingContext() = default;

    TaskLoggingContext(const TaskLoggingContext&) = delete;
    TaskLoggingContext& operator=(const TaskLoggingContext&) = delete;
    TaskLoggingContext(TaskLoggingContext&&) noexcept = delete;
    TaskLoggingContext& operator=(TaskLoggingContext&&) noexcept = delete;

    quint64 taskId() const noexcept { return m_taskId; }
    quint64 parentTaskId() const noexcept { return m_parentTaskId; }

    TaskSnapshot snapshot() const;

    void setFaultBarrier(std::shared_ptr<FaultBarrier> barrier);
    std::shared_ptr<FaultBarrier> faultBarrier() const;

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
     * @brief Returns sealed blocks whose absolute block index is at least firstBlockIndex.
     * This is the preferred incremental-read API for sinks and future UI consumers.
     */
    QVector<LogBlock> sealedBlocksFrom(quint64 firstBlockIndex) const;

    /**
     * @brief Releases a prefix of pending sealed blocks after all sinks committed them.
     * @param exclusiveBlockIndex Blocks with an index below this value are released.
     * @return Number of released blocks.
     *
     * Blocks are released in block-index order. This is intended for LogManager's
     * delivery acknowledgement and must not be used while a reader callback is active.
     */
    qsizetype releaseSealedBlocksBefore(quint64 exclusiveBlockIndex);

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
    LogSubmissionResult reportFault(const QString& message);

    /**
     * @brief Lifecycle state transitions:
     * Pending -> Running
     * Running -> Completed | Failed | Cancelled
     * Completed | Failed | Cancelled -> Terminal (non-transitionable)
     */
    bool start();
    bool complete(const QString& message = QString());
    bool fail(const QString& message = QString());
    bool cancel(const QString& message = QString());
    bool skip(const QString& message = QString());
    bool forceTerminalState(TaskState newState, const QString& message = QString());

    /**
     * @brief Disables this context when its owning LogManager session is reset.
     * Existing external shared pointers remain valid, but no longer accept state
     * transitions, ordinary logs, or fault reports.
     */
    void invalidateSession();

    quint64 errorCount() const noexcept;
    bool hasErrors() const noexcept;

    static bool isTerminalState(TaskState state) noexcept
    {
        return state == TaskState::Completed || state == TaskState::Failed ||
               state == TaskState::Cancelled || state == TaskState::Skipped;
    }

private:
    void checkAndFlushActiveBlockLocked();
    void flushActiveBlockLocked();
    LogSubmissionResult submitNormalLocked();
    bool appendLifecycleEntryLocked(LogLevel level, const QString& message);

    quint64 m_taskId = 0;
    quint64 m_parentTaskId = 0;
    quint64 m_creationSequence = 0;
    mutable QRecursiveMutex m_mutex;
    std::shared_ptr<FaultBarrier> m_faultBarrier;
    mutable QMutex m_flushMutex;
    QString m_taskName;
    TaskState m_state = TaskState::Pending;
    bool m_sessionValid = true;
    double m_progress = 0.0;
    QString m_currentMessage;
    QVector<LogBlock> m_sealedBlocks;
    LogBlock m_activeBlock;
    quint64 m_nextBlockIndex = 1; // Monotonic block identity, independent of retained blocks
    qsizetype m_blockSizeThreshold = 0; // 0 means unlimited
    quint64 m_nextSequence = 1; // Task-local log entry sequence number
    quint64 m_logCount = 0;
    quint64 m_errorCount = 0;
};

} // namespace Core::Logging
