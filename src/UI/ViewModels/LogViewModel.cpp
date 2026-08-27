#include "UI/ViewModels/LogViewModel.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/LogFileManager.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>

namespace UI::ViewModels {

namespace {

class LogViewModelSinkAdapter : public Core::Logging::ILogSink {
public:
    explicit LogViewModelSinkAdapter(LogViewModel* target)
        : m_target(target)
    {
    }

    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override
    {
        if (!m_target) {
            return false;
        }
        const quint64 generation = m_target->viewGeneration();
        QMetaObject::invokeMethod(m_target.data(), [target = m_target, block, taskName, generation]() {
            if (!target) {
                return;
            }
            if (generation != target->viewGeneration()) {
                // Stale queued log block from a prior generation (e.g. before resetView was called)
                return;
            }
            target->processIncomingBlock(block, taskName);
        }, Qt::QueuedConnection);
        return true;
    }

    bool flush() override
    {
        return true;
    }

private:
    QPointer<LogViewModel> m_target;
};

} // namespace

LogViewModel::LogViewModel(QObject* parent)
    : LogTaskModel(0, parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

LogViewModel::~LogViewModel()
{
    unregisterFromLogManager();
}

void LogViewModel::registerWithLogManager()
{
    if (m_registeredSinkId != 0) {
        return;
    }
    auto sink = std::make_shared<LogViewModelSinkAdapter>(this);
    m_registeredSinkId = sink->sinkId();
    Core::Logging::LogManager::instance().addSink(sink);
}

void LogViewModel::unregisterFromLogManager()
{
    if (m_registeredSinkId != 0) {
        Core::Logging::LogManager::instance().removeSink(m_registeredSinkId);
        m_registeredSinkId = 0;
    }
}

int LogViewModel::totalMessageCount() const
{
    return m_totalMessages;
}

void LogViewModel::setAutoScroll(bool enabled)
{
    if (m_autoScroll != enabled) {
        m_autoScroll = enabled;
        for (const auto& entry : m_taskRegistry) {
            if (entry.subTasksModel) {
                entry.subTasksModel->setAutoScroll(enabled);
            }
        }
        emit autoScrollChanged();
    }
}

TaskRegistryEntry LogViewModel::ensureTaskRegistered(quint64 taskId, const QString& taskName)
{
    if (m_taskRegistry.contains(taskId)) {
        return m_taskRegistry.value(taskId);
    }

    // Look up context in LogManager
    auto context = Core::Logging::LogManager::instance().findTask(taskId);
    quint64 parentId = context ? context->parentTaskId() : 0;

    LogTaskModel* targetModel = this;
    int taskDepth = 0;

    if (parentId != 0) {
        auto parentCtx = Core::Logging::LogManager::instance().findTask(parentId);
        if (parentCtx) {
            TaskRegistryEntry parentEntry = ensureTaskRegistered(parentId, parentCtx->taskName());
            if (parentEntry.subTasksModel) {
                targetModel = parentEntry.subTasksModel.get();
                taskDepth = parentEntry.depth + 1;
            }
        } else {
            // Parent task does not exist in LogManager; fallback cleanly to root without phantom node creation
            parentId = 0;
            taskDepth = 0;
            targetModel = this;
        }
    }

    LogTaskItem newTask;
    newTask.taskId = taskId;
    newTask.parentTaskId = parentId;
    newTask.depth = taskDepth;
    newTask.taskName = taskName.trimmed().isEmpty() ? QStringLiteral("Task %1").arg(taskId) : taskName.trimmed();
    newTask.state = context ? context->state() : Core::Logging::TaskState::Running;
    newTask.progress = context ? context->progress() : 0.0;
    newTask.currentMessage = context ? context->currentMessage() : QString();
    newTask.expanded = (!m_autoScroll || newTask.state != Core::Logging::TaskState::Completed);
    newTask.messagesModel = std::make_shared<LogMessageListModel>();
    QQmlEngine::setObjectOwnership(newTask.messagesModel.get(), QQmlEngine::CppOwnership);
    newTask.subTasksModel = std::make_shared<LogTaskModel>(taskDepth + 1);
    newTask.subTasksModel->setAutoScroll(m_autoScroll);
    QQmlEngine::setObjectOwnership(newTask.subTasksModel.get(), QQmlEngine::CppOwnership);

    targetModel->appendTask(newTask);

    TaskRegistryEntry entry;
    entry.taskId = taskId;
    entry.parentTaskId = parentId;
    entry.depth = taskDepth;
    entry.owningModel = targetModel;
    entry.messagesModel = newTask.messagesModel;
    entry.subTasksModel = newTask.subTasksModel;

    m_taskRegistry.insert(taskId, entry);

    if (!m_taskLogFiles.contains(taskId)) {
        QString taskFilePath;
        if (context && !context->logFilePath().isEmpty()) {
            taskFilePath = context->logFilePath();
        } else {
            const qint64 startTimestamp = context ? context->startTimestamp() : 0;
            taskFilePath = Core::Logging::LogFileManager::generateTaskLogFilePath(newTask.taskName, startTimestamp, taskId);
        }
        m_taskLogFiles.insert(taskId, taskFilePath);
    }
    m_activeTaskId = taskId;
    m_lastTaskId = taskId;

    return entry;
}

void LogViewModel::processIncomingBlock(const Core::Logging::LogBlock& block, const QString& taskName)
{
    quint64 taskId = block.taskId();
    const auto& entries = block.entries();
    if (entries.isEmpty() && taskId == 0) {
        return;
    }

    TaskRegistryEntry entry = ensureTaskRegistered(taskId, taskName);

    if (!entries.isEmpty() && entry.messagesModel) {
        QVector<LogMessageItem> newItems;
        newItems.reserve(entries.size());
        for (const auto& logEntry : entries) {
            LogMessageItem msgItem;
            msgItem.sequence = logEntry.sequence;
            msgItem.timestamp = logEntry.timestamp;
            msgItem.level = logEntry.level;
            msgItem.message = logEntry.message;
            newItems.append(msgItem);
        }
        entry.messagesModel->appendEntries(newItems);

        m_totalMessages += entries.size();
    }

    // Refresh state from context
    auto context = Core::Logging::LogManager::instance().findTask(taskId);
    if (context && entry.owningModel) {
        int row = entry.owningModel->findRowByTaskId(taskId);
        if (row >= 0) {
            entry.owningModel->updateTaskMetadata(row, context->state(), context->progress(), context->currentMessage(), taskName);
        }
    }

    emit totalMessageCountChanged();
}

void LogViewModel::resetView()
{
    m_viewGeneration.fetch_add(1, std::memory_order_relaxed);

    LogTaskModel::clear();
    m_taskRegistry.clear();
    m_taskLogFiles.clear();
    m_activeTaskId = 0;
    m_lastTaskId = 0;
    m_totalMessages = 0;

    emit totalMessageCountChanged();
}

QString LogViewModel::getFullLogText() const
{
    return exportToPlainText(0);
}

QString LogViewModel::activeTaskLogFilePath() const
{
    if (m_activeTaskId != 0) {
        auto context = Core::Logging::LogManager::instance().findTask(m_activeTaskId);
        if (context && context->state() == Core::Logging::TaskState::Running) {
            return m_taskLogFiles.value(m_activeTaskId);
        }
    }
    return QString();
}

QString LogViewModel::lastTaskLogFilePath() const
{
    if (m_lastTaskId != 0) {
        return m_taskLogFiles.value(m_lastTaskId);
    }
    return QString();
}

bool LogViewModel::openLogFile()
{
    QString targetPath = activeTaskLogFilePath();
    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        targetPath = lastTaskLogFilePath();
    }
    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        targetPath = Core::Logging::ApplicationLogger::logFilePath();
    }
    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        targetPath = Core::Logging::LogFileManager::generateApplicationLogFilePath();
    }

    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(targetPath));
}

void LogViewModel::appendLog(const QString& message, int level)
{
    if (message.isEmpty()) {
        return;
    }
    Core::Logging::LogBlock block(0, 0);
    Core::Logging::LogEntry entry;
    entry.taskId = 0;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = static_cast<Core::Logging::LogLevel>(level);
    entry.message = message;
    block.append(entry);
    processIncomingBlock(block, QStringLiteral("General"));
}

} // namespace UI::ViewModels
