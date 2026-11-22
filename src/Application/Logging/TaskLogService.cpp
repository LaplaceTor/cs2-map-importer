#include "Application/Logging/TaskLogService.h"

#include <QHash>
#include <QMetaObject>
#include <algorithm>
#include <atomic>
#include <mutex>

#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogBlock.h"
#include "Core/Logging/LogFileManager.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Logging/TaskSnapshot.h"

namespace Application::Logging {

namespace {

TaskState mapState(Core::Logging::TaskState state)
{
    switch (state) {
    case Core::Logging::TaskState::Pending:
        return TaskState::Pending;
    case Core::Logging::TaskState::Running:
        return TaskState::Running;
    case Core::Logging::TaskState::Completed:
        return TaskState::Completed;
    case Core::Logging::TaskState::Failed:
        return TaskState::Failed;
    case Core::Logging::TaskState::Cancelled:
        return TaskState::Cancelled;
    case Core::Logging::TaskState::Skipped:
        return TaskState::Skipped;
    }
    return TaskState::Pending;
}

LogLevel mapLevel(Core::Logging::LogLevel level)
{
    switch (level) {
    case Core::Logging::LogLevel::Debug:
        return LogLevel::Debug;
    case Core::Logging::LogLevel::Info:
        return LogLevel::Info;
    case Core::Logging::LogLevel::Warning:
        return LogLevel::Warning;
    case Core::Logging::LogLevel::Error:
        return LogLevel::Error;
    case Core::Logging::LogLevel::Critical:
        return LogLevel::Critical;
    }
    return LogLevel::Info;
}

} // namespace

/**
 * @brief Bridges Core::Logging::ILogSink into TaskLogService frame coalescing.
 * Sink callbacks run on worker threads, merge updates per taskId under m_mutex,
 * and post at most one queued wakeup per drain cycle onto the UI thread.
 */
class TaskLogService::SinkBridge final : public Core::Logging::ILogSink {
public:
    struct PendingTaskSlot {
        QString taskName;
        qint64 startTimestamp = 0;
        QString logFilePath;
        QVector<TaskLogMessage> messages;
        bool created = false;
        bool terminated = false;
        TaskState terminalState = TaskState::Pending;
        bool hasProgressUpdate = false;
        double progress = 0.0;
        QString currentMessage;
    };

    explicit SinkBridge(TaskLogService* service)
        : m_service(service)
    {
    }

    void detach()
    {
        // 1. Release store to signal any incoming calls to bail out immediately
        m_detached.store(true, std::memory_order_release);
        m_subscriptionId.store(0, std::memory_order_release);

        // 2. Wait for any in-flight critical section to finish, then clear m_service and pending data
        std::lock_guard<std::mutex> lock(m_mutex);
        m_service = nullptr;
        clearPendingLocked();
    }

    quint64 subscribe()
    {
        if (m_detached.load(std::memory_order_acquire)) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_detached.load(std::memory_order_relaxed)) {
            return 0;
        }
        clearPendingLocked();
        return m_subscriptionId.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    bool unsubscribe(quint64 subscriptionId)
    {
        if (subscriptionId == 0) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        quint64 expected = subscriptionId;
        if (m_subscriptionId.compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
            clearPendingLocked();
            return true;
        }
        return false;
    }

    bool hasPending(bool* immediateOut = nullptr) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (immediateOut) {
            *immediateOut = m_immediateRequested;
        }
        return !m_detached.load(std::memory_order_relaxed)
            && m_subscriptionId.load(std::memory_order_relaxed) != 0
            && !m_dirtyOrder.isEmpty();
    }

    quint64 takePending(QVector<quint64>& dirtyOrderOut,
                        QHash<quint64, PendingTaskSlot>& pendingByTaskOut)
    {
        dirtyOrderOut.clear();
        pendingByTaskOut.clear();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_wakeupPosted = false;
        m_immediateRequested = false;

        if (m_detached.load(std::memory_order_relaxed)) {
            clearPendingLocked();
            return 0;
        }

        const quint64 activeSubId = m_subscriptionId.load(std::memory_order_relaxed);
        if (activeSubId == 0) {
            clearPendingLocked();
            return 0;
        }

        dirtyOrderOut.swap(m_dirtyOrder);
        pendingByTaskOut.swap(m_pendingByTask);
        return activeSubId;
    }

    bool onTaskCreated(quint64 taskId, const QString& taskName, qint64 startTimestamp,
                       const QString& logFilePath) override
    {
        if (shouldBailOutLockFree()) {
            return true;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!canAcceptLocked()) {
            return true;
        }

        PendingTaskSlot& slot = ensureSlotLocked(taskId);
        slot.created = true;
        if (!taskName.isEmpty()) {
            slot.taskName = taskName;
        }
        if (startTimestamp != 0) {
            slot.startTimestamp = startTimestamp;
        }
        if (!logFilePath.isEmpty()) {
            slot.logFilePath = logFilePath;
        }
        postWakeupLocked(/*immediate=*/true);
        return true;
    }

    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override
    {
        if (shouldBailOutLockFree()) {
            return true;
        }

        const auto& entries = block.entries();
        if (entries.isEmpty()) {
            return true;
        }

        // Convert entries to TaskLogMessage DTOs outside m_mutex
        QVector<TaskLogMessage> messages;
        messages.reserve(entries.size());
        for (const auto& entry : entries) {
            TaskLogMessage message;
            message.sequence = entry.sequence;
            message.timestamp = entry.timestamp;
            message.level = mapLevel(entry.level);
            message.message = entry.message;
            message.toolTaskId = entry.toolTaskId;
            messages.append(std::move(message));
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!canAcceptLocked()) {
            return true;
        }

        PendingTaskSlot& slot = ensureSlotLocked(block.taskId());
        if (!taskName.isEmpty()) {
            slot.taskName = taskName;
        }
        if (slot.messages.isEmpty()) {
            slot.messages = std::move(messages);
        } else {
            slot.messages.reserve(slot.messages.size() + messages.size());
            for (auto& msg : messages) {
                slot.messages.append(std::move(msg));
            }
        }
        postWakeupLocked(/*immediate=*/false);
        return true;
    }

    void onTaskProgressUpdated(quint64 taskId, double progress, const QString& currentMessage) override
    {
        if (shouldBailOutLockFree()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!canAcceptLocked()) {
            return;
        }

        PendingTaskSlot& slot = ensureSlotLocked(taskId);
        slot.hasProgressUpdate = true;
        slot.progress = progress;
        slot.currentMessage = currentMessage;
        postWakeupLocked(/*immediate=*/false);
    }

    void onTaskTerminated(quint64 taskId, Core::Logging::TaskState state) override
    {
        if (shouldBailOutLockFree()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!canAcceptLocked()) {
            return;
        }

        PendingTaskSlot& slot = ensureSlotLocked(taskId);
        slot.terminated = true;
        slot.terminalState = mapState(state);
        postWakeupLocked(/*immediate=*/true);
    }

    bool flush() override
    {
        return true;
    }

    bool flushTask(quint64 taskId) override
    {
        Q_UNUSED(taskId);
        return true;
    }

private:
    bool shouldBailOutLockFree() const noexcept
    {
        return m_detached.load(std::memory_order_acquire)
            || m_subscriptionId.load(std::memory_order_acquire) == 0;
    }

    bool canAcceptLocked() const noexcept
    {
        return m_service != nullptr
            && !m_detached.load(std::memory_order_relaxed)
            && m_subscriptionId.load(std::memory_order_relaxed) != 0;
    }

    PendingTaskSlot& ensureSlotLocked(quint64 taskId)
    {
        auto it = m_pendingByTask.find(taskId);
        if (it == m_pendingByTask.end()) {
            m_dirtyOrder.append(taskId);
            it = m_pendingByTask.insert(taskId, PendingTaskSlot{});
        }
        return it.value();
    }

    void clearPendingLocked()
    {
        m_dirtyOrder.clear();
        m_pendingByTask.clear();
        m_wakeupPosted = false;
        m_immediateRequested = false;
    }

    void postWakeupLocked(bool immediate)
    {
        // Post once per drain cycle, plus one immediate wakeup if immediate transitions false -> true
        const bool needWakeup = !m_wakeupPosted || (immediate && !m_immediateRequested);
        if (immediate) {
            m_immediateRequested = true;
        }
        if (needWakeup) {
            m_wakeupPosted = true;
            QMetaObject::invokeMethod(m_service, &TaskLogService::scheduleCoalescedFlush,
                                      Qt::QueuedConnection);
        }
    }

    mutable std::mutex m_mutex;
    TaskLogService* m_service = nullptr;
    std::atomic<bool> m_detached{false};
    std::atomic<quint64> m_subscriptionId{0};
    QVector<quint64> m_dirtyOrder;
    QHash<quint64, PendingTaskSlot> m_pendingByTask;
    bool m_wakeupPosted = false;
    bool m_immediateRequested = false;
};

TaskLogService::TaskLogService(QObject* parent)
    : QObject(parent)
    , m_coalesceTimer(this)
{
    qRegisterMetaType<TaskLogMessage>("Application::Logging::TaskLogMessage");
    qRegisterMetaType<QVector<TaskLogMessage>>("QVector<Application::Logging::TaskLogMessage>");
    qRegisterMetaType<TaskLogUpdate>("Application::Logging::TaskLogUpdate");
    qRegisterMetaType<QVector<TaskLogUpdate>>("QVector<Application::Logging::TaskLogUpdate>");

    m_coalesceTimer.setSingleShot(true);
    connect(&m_coalesceTimer, &QTimer::timeout, this, &TaskLogService::drainPendingUpdates);

    m_sinkBridge = std::make_shared<SinkBridge>(this);
    Core::Logging::LogManager::instance().addSink(m_sinkBridge);
}

TaskLogService::~TaskLogService()
{
    m_coalesceTimer.stop();
    if (m_sinkBridge) {
        m_sinkBridge->detach();
        Core::Logging::LogManager::instance().removeSink(m_sinkBridge);
    }
}

quint64 TaskLogService::subscribe()
{
    if (m_coalesceTimer.isActive()) {
        m_coalesceTimer.stop();
    }
    if (m_sinkBridge) {
        return m_sinkBridge->subscribe();
    }
    return 0;
}

void TaskLogService::unsubscribe(quint64 subscriptionId)
{
    if (m_sinkBridge && m_sinkBridge->unsubscribe(subscriptionId)) {
        if (m_coalesceTimer.isActive()) {
            m_coalesceTimer.stop();
        }
    }
}

TaskInfo TaskLogService::taskInfo(quint64 taskId) const
{
    TaskInfo info;
    const auto context = Core::Logging::LogManager::instance().findTask(taskId);
    if (!context) {
        return info;
    }

    const Core::Logging::TaskSnapshot snap = context->snapshot();
    info.isValid = true;
    info.taskId = snap.taskId;
    info.parentTaskId = snap.parentTaskId;
    info.startTimestamp = snap.startTimestamp;
    info.taskName = snap.taskName;
    info.state = mapState(snap.state);
    info.progress = snap.progress;
    info.currentMessage = snap.currentMessage;
    info.isToolTask = snap.isToolTask;
    info.logFilePath = snap.logFilePath;
    info.workflowDirectory = snap.workflowDirectory;
    return info;
}

void TaskLogService::setCoalesceIntervalMs(int intervalMs)
{
    m_coalesceIntervalMs = std::max(0, intervalMs);
    if (m_coalesceIntervalMs == 0 && m_coalesceTimer.isActive()) {
        m_coalesceTimer.stop();
        drainPendingUpdates();
    }
}

int TaskLogService::coalesceIntervalMs() const noexcept
{
    return m_coalesceIntervalMs;
}

void TaskLogService::flushPendingDeliveries()
{
    if (m_coalesceTimer.isActive()) {
        m_coalesceTimer.stop();
    }
    drainPendingUpdates();
}

QString TaskLogService::applicationLogFilePath() const
{
    return Core::Logging::ApplicationLogger::logFilePath();
}

QString TaskLogService::expectedApplicationLogFilePath() const
{
    return Core::Logging::LogFileManager::generateApplicationLogFilePath();
}

QString TaskLogService::logsDirectory() const
{
    return Core::Logging::LogFileManager::logsDirectory();
}

bool TaskLogService::ensureLogsDirectory() const
{
    return Core::Logging::LogFileManager::ensureLogsDirectoryExists();
}

QString TaskLogService::fallbackTaskLogFilePath(const QString& taskName, qint64 startTimestamp, quint64 taskId) const
{
    return Core::Logging::LogFileManager::generateTaskLogFilePath(taskName, startTimestamp, taskId);
}

void TaskLogService::scheduleCoalescedFlush()
{
    bool immediate = false;
    if (!m_sinkBridge || !m_sinkBridge->hasPending(&immediate)) {
        return;
    }

    if (immediate || m_coalesceIntervalMs <= 0 || !m_lastDrainTimer.isValid()
        || m_lastDrainTimer.elapsed() >= m_coalesceIntervalMs) {
        if (m_coalesceTimer.isActive()) {
            m_coalesceTimer.stop();
        }
        drainPendingUpdates();
        return;
    }

    if (!m_coalesceTimer.isActive()) {
        const int remainingMs = std::max<int>(
            1, m_coalesceIntervalMs - static_cast<int>(m_lastDrainTimer.elapsed()));
        m_coalesceTimer.start(remainingMs);
    }
}

void TaskLogService::drainPendingUpdates()
{
    QVector<quint64> dirtyOrder;
    QHash<quint64, SinkBridge::PendingTaskSlot> pendingByTask;
    const quint64 subId = m_sinkBridge
        ? m_sinkBridge->takePending(dirtyOrder, pendingByTask)
        : 0;
    if (subId == 0 || dirtyOrder.isEmpty()) {
        return;
    }

    m_lastDrainTimer.restart();

    QVector<TaskLogUpdate> updates;
    updates.reserve(dirtyOrder.size());

    for (const quint64 taskId : dirtyOrder) {
        auto it = pendingByTask.find(taskId);
        if (it == pendingByTask.end()) {
            continue;
        }
        auto& slot = it.value();

        TaskInfo info = taskInfo(taskId);
        if (!info.isValid) {
            info.taskId = taskId;
            info.taskName = slot.taskName;
            info.startTimestamp = slot.startTimestamp;
            info.logFilePath = slot.logFilePath;
            info.state = slot.terminated ? slot.terminalState : TaskState::Running;
            if (slot.hasProgressUpdate) {
                info.progress = slot.progress;
                info.currentMessage = slot.currentMessage;
            }
        } else {
            if (info.taskName.isEmpty() && !slot.taskName.isEmpty()) {
                info.taskName = slot.taskName;
            }
            if (info.startTimestamp == 0 && slot.startTimestamp != 0) {
                info.startTimestamp = slot.startTimestamp;
            }
            if (info.logFilePath.isEmpty() && !slot.logFilePath.isEmpty()) {
                info.logFilePath = slot.logFilePath;
            }
            if (slot.terminated && info.state == TaskState::Running) {
                info.state = slot.terminalState;
            }
        }

        TaskLogUpdate update;
        update.taskId = taskId;
        update.taskInfo = std::move(info);
        update.messages = std::move(slot.messages);
        update.created = slot.created;
        update.terminated = slot.terminated;
        updates.append(std::move(update));
    }

    if (!updates.isEmpty()) {
        emit taskUpdatesReceived(subId, updates);
    }
}

} // namespace Application::Logging
