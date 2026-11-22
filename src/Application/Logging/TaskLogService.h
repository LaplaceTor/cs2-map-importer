#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>
#include <memory>

#include "Application/Logging/TaskLogDTOs.h"

namespace Core::Logging {
class ILogSink;
}

namespace Application::Logging {

/**
 * @brief Application-layer facade exposing the Core task logging system to the UI layer.
 *
 * Owns the Core::Logging::ILogSink bridge that buffers task lifecycle, progress, and sealed
 * log blocks per task and delivers coalesced frame updates (TaskLogUpdate) on the UI thread.
 * UI code must consume logging exclusively through this facade and its DTOs,
 * never through Core/Logging headers, to keep the Presentation -> Application
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
     * @brief Starts delivering log updates to the (single) active subscriber.
     * @return Subscription id that tags subsequent taskUpdatesReceived deliveries.
     * A newer subscribe() supersedes older subscriptions: updates only carry the
     * most recent id, so superseded receivers drop deliveries by id comparison.
     */
    quint64 subscribe();

    /**
     * @brief Stops log delivery for the given subscription. Unknown/expired ids are ignored.
     */
    void unsubscribe(quint64 subscriptionId);

    /**
     * @brief Single-lock snapshot of a task's presentation metadata.
     * Returns an invalid TaskInfo (isValid == false) for unknown task ids.
     */
    TaskInfo taskInfo(quint64 taskId) const;

    /**
     * @brief Sets the frame coalescing interval in milliseconds (clamped to >= 0).
     * Default is 33 ms (~30 Hz). Setting 0 disables coalescing delay.
     */
    void setCoalesceIntervalMs(int intervalMs);

    /**
     * @brief Current frame coalescing interval in milliseconds.
     */
    int coalesceIntervalMs() const noexcept;

    /**
     * @brief Immediately drains and emits any pending buffered updates on the UI thread.
     */
    void flushPendingDeliveries();

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
     * @brief Emitted on the UI thread with coalesced per-task updates.
     * @param subscriptionId Subscription that produced this batch; receivers drop stale ids.
     * @param updates Coalesced per-task updates ordered by first dirty arrival.
     */
    void taskUpdatesReceived(quint64 subscriptionId,
                             const QVector<Application::Logging::TaskLogUpdate>& updates);

private slots:
    void scheduleCoalescedFlush();
    void drainPendingUpdates();

private:
    class SinkBridge;

    std::shared_ptr<SinkBridge> m_sinkBridge;
    QTimer m_coalesceTimer;
    QElapsedTimer m_lastDrainTimer;
    int m_coalesceIntervalMs = 33;
};

} // namespace Application::Logging
