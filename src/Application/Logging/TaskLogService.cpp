#include "Application/Logging/TaskLogService.h"

#include <mutex>

#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogBlock.h"
#include "Core/Logging/LogFileManager.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskLoggingContext.h"

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
 * @brief Bridges Core::Logging::ILogSink into TaskLogService::publishBatch.
 * writeBlock is invoked on worker threads; the emitted signal is auto-queued
 * to receivers living on the UI thread.
 */
class TaskLogService::SinkBridge final : public Core::Logging::ILogSink {
public:
    explicit SinkBridge(TaskLogService* service)
        : m_service(service)
    {
    }

    void detach()
    {
        // 1. Release store to signal any incoming calls to bail out immediately
        m_detached.store(true, std::memory_order_release);
        m_subscriptionId.store(0, std::memory_order_release);

        // 2. Wait for any in-flight writeBlock critical section to finish, then clear m_service
        std::lock_guard<std::mutex> lock(m_mutex);
        m_service = nullptr;
    }

    quint64 subscribe()
    {
        if (m_detached.load(std::memory_order_acquire)) {
            return 0;
        }
        return m_subscriptionId.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    void unsubscribe(quint64 subscriptionId)
    {
        quint64 expected = subscriptionId;
        if (expected != 0) {
            m_subscriptionId.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
        }
    }

    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override
    {
        // Step 1: Fast lock-free bailout if detached
        if (m_detached.load(std::memory_order_acquire)) {
            return true;
        }

        // Step 2: Fast lock-free bailout if no active subscriber
        const quint64 subscriptionId = m_subscriptionId.load(std::memory_order_acquire);
        if (subscriptionId == 0) {
            return true; // No active subscriber: skip DTO conversion entirely
        }

        // Step 3: Convert entries to TaskLogMessage DTOs lock-free
        QVector<TaskLogMessage> messages;
        const auto& entries = block.entries();
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

        // Step 4: Thread-safe delivery to TaskLogService under lock
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_service || m_detached.load(std::memory_order_relaxed)) {
            return true;
        }

        const quint64 activeSubId = m_subscriptionId.load(std::memory_order_relaxed);
        if (activeSubId == 0) {
            return true;
        }

        m_service->publishBatch(activeSubId, block.taskId(), taskName, std::move(messages));
        return true;
    }

    bool flush() override
    {
        return true;
    }

private:
    std::mutex m_mutex;
    TaskLogService* m_service = nullptr;
    std::atomic<bool> m_detached{false};
    std::atomic<quint64> m_subscriptionId{0};
};

TaskLogService::TaskLogService(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<TaskLogMessage>("Application::Logging::TaskLogMessage");
    qRegisterMetaType<QVector<TaskLogMessage>>("QVector<Application::Logging::TaskLogMessage>");

    m_sinkBridge = std::make_shared<SinkBridge>(this);
    Core::Logging::LogManager::instance().addSink(m_sinkBridge);
}

TaskLogService::~TaskLogService()
{
    if (m_sinkBridge) {
        m_sinkBridge->detach();
        Core::Logging::LogManager::instance().removeSink(m_sinkBridge);
    }
}

quint64 TaskLogService::subscribe()
{
    if (m_sinkBridge) {
        return m_sinkBridge->subscribe();
    }
    return 0;
}

void TaskLogService::unsubscribe(quint64 subscriptionId)
{
    if (m_sinkBridge) {
        m_sinkBridge->unsubscribe(subscriptionId);
    }
}

TaskInfo TaskLogService::taskInfo(quint64 taskId) const
{
    TaskInfo info;
    const auto context = Core::Logging::LogManager::instance().findTask(taskId);
    if (!context) {
        return info;
    }

    info.isValid = true;
    info.taskId = taskId;
    info.parentTaskId = context->parentTaskId();
    info.startTimestamp = context->startTimestamp();
    info.taskName = context->taskName();
    info.state = mapState(context->state());
    info.progress = context->progress();
    info.currentMessage = context->currentMessage();
    info.isToolTask = context->isToolTask();
    info.logFilePath = context->logFilePath();
    info.workflowDirectory = context->workflowDirectory();
    return info;
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

void TaskLogService::publishBatch(quint64 subscriptionId, quint64 taskId, const QString& taskName,
                                  QVector<TaskLogMessage> messages)
{
    emit logBatchReceived(subscriptionId, taskId, taskName, messages);
}

} // namespace Application::Logging
