#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

#include "LogBlock.h"
#include "TaskLoggingContext.h"

namespace Core::Logging {

class LogManager {
public:
    LogManager() = default;
    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    static LogManager& instance();

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

    qsizetype defaultBlockSizeThreshold() const;
    void setDefaultBlockSizeThreshold(qsizetype bytes);

    /**
     * @brief Retrieves already sealed blocks for a task.
     */
    QVector<LogBlock> getSealedBlocks(quint64 taskId) const;

    /**
     * @brief Zero-copy inspection of a task's sealed log blocks.
     */
    bool readSealedBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const;

    /**
     * @brief Retrieves all blocks (sealed and active) for a task.
     */
    QVector<LogBlock> getAllBlocks(quint64 taskId) const;

    /**
     * @brief Zero-copy inspection of all blocks (sealed and active) for a task.
     */
    bool readAllBlocks(quint64 taskId, const std::function<void(const QVector<LogBlock>&)>& reader) const;

    /**
     * @brief Flushes active block for a task manually.
     */
    bool flushTask(quint64 taskId);

    /**
     * @brief Zero-copy inspection of a task's active log block.
     * Invokes the reader callback with a const reference to the task's LogBlock while locked.
     * Returns true if task was found, false otherwise.
     */
    bool readLogBlock(quint64 taskId, const std::function<void(const LogBlock&)>& reader) const;

    /**
     * @brief Retrieves an explicit read-only snapshot copy of the task's log block.
     */
    LogBlock getLogBlockSnapshot(quint64 taskId) const;

    QVector<quint64> taskIds() const;
    qsizetype taskCount() const;

    /**
     * @brief Clears the LogManager registry.
     * Note: This removes task references from the LogManager registry. It does not force-kill
     * or alter external tasks that hold a std::shared_ptr<TaskLoggingContext> reference.
     */
    void clear();

private:
    mutable QMutex m_mutex;
    QHash<quint64, std::shared_ptr<TaskLoggingContext>> m_tasks;
    quint64 m_nextTaskId = 1;
    qsizetype m_defaultBlockSizeThreshold = 0;
};

} // namespace Core::Logging
