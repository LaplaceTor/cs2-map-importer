#include "UI/ViewModels/LogViewModel.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogManager.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>

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
        QMetaObject::invokeMethod(m_target.data(), [target = m_target, block, taskName]() {
            if (target) {
                target->processIncomingBlock(block, taskName);
            }
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
    newTask.expanded = true;
    newTask.messagesModel = std::make_shared<LogMessageListModel>();
    newTask.subTasksModel = std::make_shared<LogTaskModel>(taskDepth + 1);

    targetModel->appendTask(newTask);

    TaskRegistryEntry entry;
    entry.taskId = taskId;
    entry.parentTaskId = parentId;
    entry.depth = taskDepth;
    entry.owningModel = targetModel;
    entry.messagesModel = newTask.messagesModel;
    entry.subTasksModel = newTask.subTasksModel;

    m_taskRegistry.insert(taskId, entry);

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

void LogViewModel::clear()
{
    LogTaskModel::clear();
    m_taskRegistry.clear();
    m_totalMessages = 0;

    emit totalMessageCountChanged();
}

QString LogViewModel::getFullLogText() const
{
    return exportToPlainText(0);
}

void LogViewModel::copyToClipboard()
{
    QString fullText = getFullLogText();
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(fullText);
    }
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
