#include "Application/Logging/TaskLogService.h"

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

    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override
    {
        if (!m_service) {
            return false;
        }
        const quint64 subscriptionId = m_service->m_subscriptionId.load(std::memory_order_relaxed);
        if (subscriptionId == 0) {
            return true; // No active subscriber: skip DTO conversion entirely
        }

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

        m_service->publishBatch(subscriptionId, block.taskId(), taskName, std::move(messages));
        return true;
    }

    bool flush() override
    {
        return true;
    }

private:
    TaskLogService* m_service = nullptr;
};

TaskLogService::TaskLogService(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<TaskLogMessage>("Application::Logging::TaskLogMessage");
    qRegisterMetaType<QVector<TaskLogMessage>>("QVector<Application::Logging::TaskLogMessage>");

    m_sink = std::make_shared<SinkBridge>(this);
    Core::Logging::LogManager::instance().addSink(m_sink);
}

TaskLogService::~TaskLogService()
{
    Core::Logging::LogManager::instance().removeSink(m_sink);
}

quint64 TaskLogService::subscribe()
{
    // Start from 1 so 0 can represent "no subscription"
    return m_subscriptionId.fetch_add(1, std::memory_order_relaxed) + 1;
}

void TaskLogService::unsubscribe(quint64 subscriptionId)
{
    quint64 expected = subscriptionId;
    if (expected != 0) {
        m_subscriptionId.compare_exchange_strong(expected, 0, std::memory_order_relaxed);
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
