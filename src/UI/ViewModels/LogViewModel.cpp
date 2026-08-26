#include "UI/ViewModels/LogViewModel.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutexLocker>

namespace UI::ViewModels {

LogViewModel::LogViewModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

LogViewModel::~LogViewModel()
{
    unregisterFromLogManager();
}

void LogViewModel::registerWithLogManager()
{
    try {
        Core::Logging::LogManager::instance().addSink(shared_from_this());
    } catch (...) {
        // Handle standalone/unregistered instances
    }
}

void LogViewModel::unregisterFromLogManager()
{
    Core::Logging::LogManager::instance().removeSink(sinkId());
}

int LogViewModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

QVariant LogViewModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    QMutexLocker locker(&m_mutex);
    int row = index.row();
    if (row < 0 || row >= m_tasks.size()) {
        return QVariant();
    }

    const auto& task = m_tasks.at(row);

    switch (role) {
    case TaskIdRole:
        return QVariant::fromValue(task.taskId);
    case TaskNameRole:
        return task.taskName;
    case StateRole:
        return static_cast<int>(task.state);
    case StateStringRole:
        return Core::Logging::taskStateToString(task.state);
    case ProgressRole:
        return task.progress;
    case CurrentMessageRole:
        return task.currentMessage;
    case ExpandedRole:
        return task.expanded;
    case MessageCountRole:
        return task.messages.size();
    case FormattedMessagesRole:
        return task.formattedMessages;
    case Qt::DisplayRole:
        return task.taskName;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LogViewModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TaskIdRole] = "taskId";
    roles[TaskNameRole] = "taskName";
    roles[StateRole] = "state";
    roles[StateStringRole] = "stateString";
    roles[ProgressRole] = "progress";
    roles[CurrentMessageRole] = "currentMessage";
    roles[ExpandedRole] = "expanded";
    roles[MessageCountRole] = "messageCount";
    roles[FormattedMessagesRole] = "formattedMessages";
    return roles;
}

int LogViewModel::taskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

int LogViewModel::totalMessageCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalMessages;
}

int LogViewModel::lineCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalMessages;
}

QString LogViewModel::formattedLogText() const
{
    QMutexLocker locker(&m_mutex);
    QStringList parts;
    for (const auto& task : m_tasks) {
        if (!task.formattedMessages.isEmpty()) {
            parts.append(task.formattedMessages);
        }
    }
    return parts.join(QStringLiteral("<br>"));
}

void LogViewModel::setAutoScroll(bool enabled)
{
    if (m_autoScroll != enabled) {
        m_autoScroll = enabled;
        emit autoScrollChanged();
    }
}

bool LogViewModel::writeBlock(const Core::Logging::LogBlock& block, const QString& taskName)
{
    QMetaObject::invokeMethod(this, [this, block, taskName]() {
        processIncomingBlock(block, taskName);
    }, Qt::QueuedConnection);
    return true;
}

bool LogViewModel::flush()
{
    return true;
}

void LogViewModel::processIncomingBlock(const Core::Logging::LogBlock& block, const QString& taskName)
{
    quint64 taskId = block.taskId();
    const auto& entries = block.entries();
    if (entries.isEmpty() && taskId == 0) {
        return;
    }

    int taskIndex = -1;
    bool isNewTask = false;

    {
        QMutexLocker locker(&m_mutex);
        if (m_taskIdToIndex.contains(taskId)) {
            taskIndex = m_taskIdToIndex.value(taskId);
        } else {
            isNewTask = true;
            taskIndex = m_tasks.size();
        }
    }

    if (isNewTask) {
        beginInsertRows(QModelIndex(), taskIndex, taskIndex);
        {
            QMutexLocker locker(&m_mutex);
            LogTaskItem newTask;
            newTask.taskId = taskId;
            newTask.taskName = taskName.trimmed().isEmpty() ? QStringLiteral("Task %1").arg(taskId) : taskName.trimmed();
            newTask.state = Core::Logging::TaskState::Running;
            newTask.expanded = true;

            // Fetch live metadata if context is available
            auto context = Core::Logging::LogManager::instance().findTask(taskId);
            if (context) {
                newTask.state = context->state();
                newTask.progress = context->progress();
                newTask.currentMessage = context->currentMessage();
            }

            m_tasks.append(newTask);
            m_taskIdToIndex.insert(taskId, taskIndex);
        }
        endInsertRows();
        emit taskCountChanged();
    }

    // Append entries and update task
    {
        QMutexLocker locker(&m_mutex);
        if (taskIndex < 0 || taskIndex >= m_tasks.size()) {
            return;
        }

        auto& task = m_tasks[taskIndex];
        if (!taskName.trimmed().isEmpty() && task.taskName.startsWith(QStringLiteral("Task "))) {
            task.taskName = taskName.trimmed();
        }

        QStringList htmlAppends;
        for (const auto& entry : entries) {
            LogMessageItem msgItem;
            msgItem.sequence = entry.sequence;
            msgItem.timestamp = entry.timestamp;
            msgItem.level = entry.level;
            msgItem.message = entry.message;
            msgItem.formattedHtml = formatMessageHtml(msgItem);

            task.messages.append(msgItem);
            htmlAppends.append(msgItem.formattedHtml);
            m_totalMessages++;
        }

        if (!htmlAppends.isEmpty()) {
            if (!task.formattedMessages.isEmpty()) {
                task.formattedMessages.append(QStringLiteral("<br>"));
            }
            task.formattedMessages.append(htmlAppends.join(QStringLiteral("<br>")));
        }

        // Refresh snapshot state
        auto context = Core::Logging::LogManager::instance().findTask(taskId);
        if (context) {
            task.state = context->state();
            task.progress = context->progress();
            task.currentMessage = context->currentMessage();
        }
    }

    QModelIndex changedIndex = index(taskIndex, 0);
    emit dataChanged(changedIndex, changedIndex, {
        TaskNameRole,
        StateRole,
        StateStringRole,
        ProgressRole,
        CurrentMessageRole,
        MessageCountRole,
        FormattedMessagesRole
    });

    emit totalMessageCountChanged();
    emit logTextChanged();
}

void LogViewModel::clear()
{
    beginResetModel();
    {
        QMutexLocker locker(&m_mutex);
        m_tasks.clear();
        m_taskIdToIndex.clear();
        m_totalMessages = 0;
    }
    endResetModel();

    emit taskCountChanged();
    emit totalMessageCountChanged();
    emit logTextChanged();
}

QString LogViewModel::getFullLogText() const
{
    QMutexLocker locker(&m_mutex);
    QStringList result;
    for (const auto& task : m_tasks) {
        result.append(QStringLiteral("=== %1 ===").arg(task.taskName));
        result.append(QString());
        for (const auto& msg : task.messages) {
            QString timeStr = msg.timestamp > 0
                ? QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString(QStringLiteral("hh:mm:ss"))
                : QStringLiteral("00:00:00");
            QString levelStr;
            switch (msg.level) {
            case Core::Logging::LogLevel::Debug:    levelStr = QStringLiteral("DEBUG"); break;
            case Core::Logging::LogLevel::Info:     levelStr = QStringLiteral("INFO "); break;
            case Core::Logging::LogLevel::Warning:  levelStr = QStringLiteral("WARN "); break;
            case Core::Logging::LogLevel::Error:    levelStr = QStringLiteral("ERROR"); break;
            case Core::Logging::LogLevel::Critical: levelStr = QStringLiteral("CRIT "); break;
            }
            result.append(QStringLiteral("[%1] %2  %3").arg(timeStr, levelStr, msg.message));
        }
        result.append(QString());
    }
    return result.join(QLatin1Char('\n'));
}

void LogViewModel::copyToClipboard()
{
    QString fullText = getFullLogText();
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(fullText);
    }
}

void LogViewModel::expandAll()
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_tasks.size(); ++i) {
        m_tasks[i].expanded = true;
    }
    if (!m_tasks.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_tasks.size() - 1, 0), {ExpandedRole});
    }
}

void LogViewModel::collapseAll()
{
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_tasks.size(); ++i) {
        m_tasks[i].expanded = false;
    }
    if (!m_tasks.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_tasks.size() - 1, 0), {ExpandedRole});
    }
}

void LogViewModel::toggleTaskExpanded(int index)
{
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_tasks.size()) {
        return;
    }
    m_tasks[index].expanded = !m_tasks[index].expanded;
    QModelIndex modelIdx = this->index(index, 0);
    emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
}

void LogViewModel::setTaskExpanded(int index, bool expanded)
{
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_tasks.size()) {
        return;
    }
    if (m_tasks[index].expanded != expanded) {
        m_tasks[index].expanded = expanded;
        QModelIndex modelIdx = this->index(index, 0);
        emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
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

QString LogViewModel::formatMessageHtml(const LogMessageItem& item) const
{
    QString timeStr = item.timestamp > 0
        ? QDateTime::fromMSecsSinceEpoch(item.timestamp).toString(QStringLiteral("hh:mm:ss"))
        : QString();
    QString safeMsg = item.message.toHtmlEscaped();
    QString levelColor = QStringLiteral("#E0E0E0");
    QString levelTag = QStringLiteral("INFO");

    switch (item.level) {
    case Core::Logging::LogLevel::Debug:
        levelColor = QStringLiteral("#9E9E9E");
        levelTag = QStringLiteral("DEBUG");
        break;
    case Core::Logging::LogLevel::Info:
        levelColor = QStringLiteral("#ECEFF1");
        levelTag = QStringLiteral("INFO ");
        break;
    case Core::Logging::LogLevel::Warning:
        levelColor = QStringLiteral("#FFD740");
        levelTag = QStringLiteral("WARN ");
        break;
    case Core::Logging::LogLevel::Error:
        levelColor = QStringLiteral("#FF5252");
        levelTag = QStringLiteral("ERROR");
        break;
    case Core::Logging::LogLevel::Critical:
        levelColor = QStringLiteral("#FF1744");
        levelTag = QStringLiteral("CRIT ");
        break;
    }

    if (!timeStr.isEmpty()) {
        return QStringLiteral("<font color='#757575'>[%1]</font> <font color='%2'><b>%3</b> %4</font>")
            .arg(timeStr, levelColor, levelTag, safeMsg);
    } else {
        return QStringLiteral("<font color='%1'><b>%2</b> %3</font>").arg(levelColor, levelTag, safeMsg);
    }
}

} // namespace UI::ViewModels
