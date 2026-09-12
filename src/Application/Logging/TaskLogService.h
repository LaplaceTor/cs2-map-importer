#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

#include "Application/Logging/TaskLogDTOs.h"

namespace Core::Logging {
class ILogSink;
}

namespace Application::Logging {

/**
 * @brief Application-layer facade exposing the Core task logging system to the UI layer.
 *
 * Owns the Core::Logging::ILogSink bridge that converts sealed Core log blocks into
 * Application-owned DTOs (TaskLogMessage) and delivers them via queued signals.
 * UI code must consume logging exclusively through this facade and its DTOs,
 * never through Core/Logging headers, to keep the Presentation → Application
 * dependency contract intact.
 */
class TaskLogService : public QObject {
    Q_OBJECT

public:
    explicit TaskLogService(QObject* parent = nullptr);
    ~TaskLogService() override;

    TaskLogService(const TaskLogService&) = delete;
    TaskLogService& operator=(const TaskLogService&) = delete;
    TaskLogService(TaskLogService&&) = delete;
    TaskLogService& operator=(TaskLogService&&) = delete;

    /**
     * @brief Starts delivering log batches to the (single) active subscriber.
     * @return Subscription id that tags subsequent logBatchReceived deliveries.
     * A newer subscribe() supersedes older subscriptions: batches only carry the
     * most recent id, so superseded receivers drop deliveries by id comparison.
     */
    quint64 subscribe();

    /**
     * @brief Stops log delivery for the given subscription. Unknown/expired ids are ignored.
     */
    void unsubscribe(quint64 subscriptionId);

    /**
     * @brief Snapshot of a task's presentation metadata.
     * Returns an invalid TaskInfo (isValid == false) for unknown task ids.
     */
    TaskInfo taskInfo(quint64 taskId) const;

    /**
     * @brief Path of the current application log file (empty when logging is not initialized).
     */
    QString applicationLogFilePath() const;

    /**
     * @brief Predicted application log file path for the current session (may not exist yet).
     */
    QString expectedApplicationLogFilePath() const;

    /**
     * @brief Root directory that holds application and workflow logs.
     */
    QString logsDirectory() const;

    /**
     * @brief Creates the logs directory when missing.
     * @return true if the directory exists or was created successfully.
     */
    bool ensureLogsDirectory() const;

    /**
     * @brief Deterministic fallback path for a task log when the task context carries none.
     */
    QString fallbackTaskLogFilePath(const QString& taskName, qint64 startTimestamp, quint64 taskId) const;

signals:
    /**
     * @brief Emitted (possibly from worker threads) for each published Core log block.
     * @param subscriptionId Subscription that produced this batch; receivers drop stale ids.
     * @param taskId Task the block belongs to (0 for unattached UI-side messages).
     * @param taskName Display name of the task (may be empty).
     * @param messages Converted log messages in submission order.
     */
    void logBatchReceived(quint64 subscriptionId, quint64 taskId, const QString& taskName,
                          const QVector<Application::Logging::TaskLogMessage>& messages);

private:
    class SinkBridge;

    void publishBatch(quint64 subscriptionId, quint64 taskId, const QString& taskName,
                      QVector<TaskLogMessage> messages);

    std::shared_ptr<Core::Logging::ILogSink> m_sink;
    std::atomic<quint64> m_subscriptionId{0};
};

} // namespace Application::Logging
