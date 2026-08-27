#pragma once

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QTextStream>
#include <memory>

#include "ILogSink.h"
#include "LogBlock.h"
#include "LogSource.h"

namespace Core::Logging {

/**
 * @brief Log sink that writes sealed log blocks to task-specific log files.
 * Automatically manages task file creation, naming (<task_name>_<start_timestamp>.log),
 * continuous appending, and reliable flushing upon task completion/failure/cancellation.
 */
class TaskFileSink : public ILogSink {
public:
    explicit TaskFileSink();
    ~TaskFileSink() override;

    TaskFileSink(const TaskFileSink&) = delete;
    TaskFileSink& operator=(const TaskFileSink&) = delete;
    TaskFileSink(TaskFileSink&&) = delete;
    TaskFileSink& operator=(TaskFileSink&&) = delete;

    /**
     * @brief Creates and opens the log file immediately upon task creation.
     */
    bool onTaskCreated(quint64 taskId, const QString& taskName, qint64 startTimestamp, const QString& logFilePath) override;

    /**
     * @brief Checks if the log file handle for a given task is open and ready.
     */
    bool isTaskFileOpen(quint64 taskId) const;

    /**
     * @brief Flushes and closes the task log file upon task completion/failure/cancellation.
     */
    void onTaskTerminated(quint64 taskId, TaskState state) override;

    /**
     * @brief Writes a sealed LogBlock to the task's individual log file.
     */
    bool writeBlock(const LogBlock& block, const QString& taskName) override;

    /**
     * @brief Flushes all currently open task log files to disk.
     */
    bool flush() override;

    /**
     * @brief Flushes and closes the log file for a specific task.
     */
    void closeTask(quint64 taskId);

    /**
     * @brief Closes all open task log files.
     */
    void closeAll();

    /**
     * @brief Retrieves the file path for a specific task's log.
     */
    QString taskLogFilePath(quint64 taskId) const;

    /**
     * @brief Retrieves the log file path of the most recent or active task.
     */
    QString lastTaskLogFilePath() const;

    /**
     * @brief Sets or registers a task's log file path explicitly (e.g. at task initialization).
     */
    void registerTaskPath(quint64 taskId, const QString& path);

    /**
     * @brief Formats a single log entry line.
     * Format: [2026-08-27 21:30:01.012] [Task 42 - Map Import] [Block 1] [Seq 1] [Source: Workflow] [INFO] message
     */
    static QString formatEntry(qint64 timestamp, quint64 taskId, const QString& taskName,
                               quint64 blockIndex, quint64 sequence, LogSource source,
                               LogLevel level, const QString& message);

private:
    struct TaskFileHandle {
        QString filePath;
        std::unique_ptr<QFile> file;
        std::unique_ptr<QTextStream> stream;
    };

    bool ensureTaskFileOpenLocked(quint64 taskId, const QString& taskName, qint64 startTimestamp);

    mutable QMutex m_mutex;
    QHash<quint64, std::shared_ptr<TaskFileHandle>> m_taskFiles;
    QHash<quint64, QString> m_taskFilePaths;
    QString m_lastTaskLogFilePath;
};

} // namespace Core::Logging
