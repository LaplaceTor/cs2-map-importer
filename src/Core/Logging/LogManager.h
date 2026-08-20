#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

#include "ILogSink.h"
#include "LogBlock.h"
#include "TaskLoggingContext.h"

namespace Core::Logging {

class LogManager {
public:
    struct SinkCursor {
        qsizetype committed = 0;
        qsizetype reserved = 0;
    };


    LogManager() = default;
    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    static LogManager& instance();

    std::shared_ptr<FaultBarrier> faultBarrier() const;
    LogSubmissionResult reportFault(quint64 taskId, const QString& message);
    bool beginFaultDraining();
    bool terminateAfterFault();

    std::shared_ptr<TaskLoggingContext> createTask(const QString& taskName = QString());

    /**
     * @brief Create a task with an explicitly provided taskId.
     * @return std::shared_ptr<TaskLoggingContext> if successful, or nullptr if taskId already exists.
     */
    std::shared_ptr<TaskLoggingContext> createTask(quint64 taskId, const QString& taskName = QString());

    std::shared_ptr<TaskLoggingContext> findTask(quint64 taskId) const;

    bool finishTask(quint64 taskId, const QString& message = QString());
    bool failTask(quint64 taskId, const QString& message = QString());
    bool cancelTask(quint64 taskId, const QString& message = QString());

    /**
     * @brief Sink Management APIs
     */
    void addSink(std::shared_ptr<ILogSink> sink);
    void removeSink(std::shared_ptr<ILogSink> sink);
    void clearSinks();

    /**
     * @brief Flushes all active blocks across all tasks and writes sealed blocks to registered sinks.
     */
    void flushAll();

    /**
     * @brief Get default block size threshold in bytes (estimated memory footprint).
     */
    qsizetype defaultBlockSizeThreshold() const;

    /**
     * @brief Set default block size threshold in bytes (0 means unlimited/no auto-seal).
     * Note: Size is calculated based on estimated memory footprint (estimatedByteSize).
     */
    void setDefaultBlockSizeThreshold(qsizetype bytes);

    /**
     * @brief Retrieves already sealed blocks for a task.
     */
    QVector<LogBlock> getSealedBlocks(quint64 taskId) const;

    /**
     * @brief Zero-copy inspection of a task's sealed log blocks.
     * Note: Reader callback is executed while holding the task lock. Keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    bool readSealedBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const;

    /**
     * @brief Retrieves all blocks (sealed and active) for a task.
     */
    QVector<LogBlock> getAllBlocks(quint64 taskId) const;

    /**
     * @brief Zero-copy inspection of all blocks (sealed and active) for a task.
     * Note: Reader callback is executed while holding the task lock. Keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    bool readAllBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const;

    /**
     * @brief Flushes active block for a task manually and outputs unwritten sealed blocks to sinks.
     */
    bool flushTask(quint64 taskId);

    /**
     * @brief Zero-copy inspection of a task's active log block.
     * Invokes the reader callback with a const reference to the task's active LogBlock while locked.
     * Returns true if task was found, false otherwise.
     * Note: Reader callback is executed while holding the task lock. Keep callback
     * operations lightweight and in-memory (avoid heavy I/O or long-running work).
     */
    bool readLogBlock(quint64 taskId, const std::function<void(const LogBlock&)>& reader) const;

    /**
     * @brief Retrieves an explicit read-only snapshot copy of the task's active log block.
     */
    LogBlock getLogBlockSnapshot(quint64 taskId) const;

    QVector<quint64> taskIds() const;
    qsizetype taskCount() const;

    /**
     * @brief Clears the LogManager registry and registered sinks.
     * Note: This removes task references from the LogManager registry. It does not force-kill
     * or alter external tasks that hold a std::shared_ptr<TaskLoggingContext> reference.
     */
    void clear();

private:
    mutable QMutex m_mutex;
    QHash<quint64, std::shared_ptr<TaskLoggingContext>> m_tasks;
    QVector<std::shared_ptr<ILogSink>> m_sinks;
    // Independent block cursors per sink ID per task ID: [sinkId -> [taskId -> SinkCursor]]
    QHash<quint64, QHash<quint64, SinkCursor>> m_sinkCursors;
    QHash<quint64, quint64> m_sinkGenerations;
    quint64 m_nextSinkGeneration = 1;
    std::shared_ptr<FaultBarrier> m_faultBarrier = std::make_shared<FaultBarrier>();
    quint64 m_nextTaskId = 1;
    qsizetype m_defaultBlockSizeThreshold = 0;
};

} // namespace Core::Logging
