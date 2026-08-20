#pragma once

#include <QDateTime>
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

    QString taskName() const;
    void setTaskName(const QString& name);

    TaskState state() const noexcept;

    double progress() const noexcept;
    void updateProgress(double progress, const QString& message = QString());

    QString currentMessage() const;
    void updateCurrentMessage(const QString& message);

    qsizetype blockSizeThreshold() const;
    void setBlockSizeThreshold(qsizetype bytes);

    void flushActiveBlock();

    QVector<LogBlock> sealedBlocks() const;
    void withSealedBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const;

    QVector<LogBlock> allBlocks() const;
    void withAllBlocks(const std::function<void(const QVector<LogBlock>&)>& reader) const;

    qsizetype sealedBlockCount() const;
    qsizetype totalBlockCount() const;

    /**
     * @brief Zero-copy reader callback for active log block inspection.
     * Note: Executed while holding the task lock. For lightweight/in-memory inspections,
     * this avoids copying. For long-running or IO operations, prefer logBlockSnapshot().
     */
    void withLogBlock(const std::function<void(const LogBlock&)>& reader) const;

    // Explicit read-only merged snapshot of all entries across blocks
    LogBlock logBlockSnapshot() const;

    // Logging methods
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void log(LogLevel level, const QString& message);

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
