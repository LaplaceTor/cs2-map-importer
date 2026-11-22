#include "UI/ViewModels/LogViewModel.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QStringList>
#include <QUrl>
#include <functional>

#include "Application/Logging/TaskLogService.h"

namespace UI::ViewModels {

LogViewModel::LogViewModel(Application::Logging::TaskLogService* logService, QObject* parent)
    : QAbstractListModel(parent)
    , m_logService(logService)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

LogViewModel::~LogViewModel()
{
    detachFromLogService();
}

void LogViewModel::attachToLogService(Application::Logging::TaskLogService* service)
{
    detachFromLogService();
    m_logService = service;
    if (!m_logService) {
        return;
    }

    m_subscriptionId = m_logService->subscribe();
    connect(m_logService, &Application::Logging::TaskLogService::taskUpdatesReceived,
            this, &LogViewModel::onTaskUpdatesReceived);
}

void LogViewModel::detachFromLogService()
{
    if (m_logService) {
        disconnect(m_logService, nullptr, this, nullptr);
        if (m_subscriptionId != 0) {
            m_logService->unsubscribe(m_subscriptionId);
        }
    }
    m_logService = nullptr;
    m_subscriptionId = 0;
}

void LogViewModel::onTaskUpdatesReceived(quint64 subscriptionId,
                                         const QVector<Application::Logging::TaskLogUpdate>& updates)
{
    // Stale frame: published under a superseded/reset subscription (e.g. before resetView)
    if (subscriptionId == 0 || subscriptionId != m_subscriptionId) {
        return;
    }

    int totalAddedMessages = 0;
    for (const auto& update : updates) {
        if (update.taskId == 0 && update.messages.isEmpty() && !update.taskInfo.isValid) {
            continue;
        }
        const Application::Logging::TaskInfo* infoPtr =
            update.taskInfo.isValid ? &update.taskInfo : nullptr;
        totalAddedMessages += applyTaskUpdate(
            update.taskId, update.taskInfo.taskName, infoPtr, update.messages);
    }

    if (totalAddedMessages > 0) {
        emit totalMessageCountChanged();
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

int LogViewModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_visibleNodes.size();
}

QVariant LogViewModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int row = index.row();
    if (row < 0 || row >= m_visibleNodes.size()) {
        return QVariant();
    }

    const auto& node = m_visibleNodes.at(row);

    switch (role) {
    case TaskIdRole:
        return QVariant::fromValue(node->taskId);
    case ParentTaskIdRole:
        return QVariant::fromValue(node->parentTaskId);
    case DepthRole:
        return node->depth;
    case TaskNameRole:
        return node->taskName;
    case StateRole:
        return static_cast<int>(node->state);
    case StateStringRole:
        return Application::Logging::taskStateToString(node->state);
    case ProgressRole:
        return node->progress;
    case CurrentMessageRole:
        return node->currentMessage;
    case ExpandedRole:
        return node->expanded;
    case MessageCountRole:
        return node->messagesModel ? node->messagesModel->count() : 0;
    case SubTasksCountRole:
        return node->children.size();
    case HasSubTasksRole:
        return !node->children.isEmpty();
    case MessagesModelRole:
        if (node->messagesModel) {
            QQmlEngine::setObjectOwnership(node->messagesModel.get(), QQmlEngine::CppOwnership);
        }
        return QVariant::fromValue(node->messagesModel.get());
    case Qt::DisplayRole:
        return node->taskName;
    }

    return QVariant();
}

bool LogViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    const int row = index.row();
    if (row < 0 || row >= m_visibleNodes.size()) {
        return false;
    }

    if (role == ExpandedRole) {
        setTaskExpanded(row, value.toBool());
        return true;
    }

    return false;
}

QHash<int, QByteArray> LogViewModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = []() {
        QHash<int, QByteArray> r;
        r[TaskIdRole] = "taskId";
        r[ParentTaskIdRole] = "parentTaskId";
        r[DepthRole] = "depth";
        r[TaskNameRole] = "taskName";
        r[StateRole] = "state";
        r[StateStringRole] = "stateString";
        r[ProgressRole] = "progress";
        r[CurrentMessageRole] = "currentMessage";
        r[ExpandedRole] = "expanded";
        r[MessageCountRole] = "messageCount";
        r[SubTasksCountRole] = "subTasksCount";
        r[HasSubTasksRole] = "hasSubTasks";
        r[MessagesModelRole] = "messagesModel";
        return r;
    }();
    return roles;
}

int LogViewModel::taskCount() const
{
    return m_visibleNodes.size();
}

std::shared_ptr<TaskTreeNode> LogViewModel::ensureTaskNode(
    quint64 taskId,
    const QString& taskName,
    const Application::Logging::TaskInfo* providedInfo)
{
    if (auto existing = m_nodesById.constFind(taskId); existing != m_nodesById.constEnd()) {
        return existing.value();
    }

    Application::Logging::TaskInfo queriedInfo;
    if ((!providedInfo || !providedInfo->isValid) && taskId != 0 && m_logService) {
        queriedInfo = m_logService->taskInfo(taskId);
    }
    const Application::Logging::TaskInfo& info =
        (providedInfo && providedInfo->isValid) ? *providedInfo : queriedInfo;

    const quint64 parentId = info.isValid ? info.parentTaskId : 0;
    const QString trimmedInputName = taskName.trimmed();
    const QString effectiveName = !trimmedInputName.isEmpty()
        ? trimmedInputName
        : (info.isValid && !info.taskName.trimmed().isEmpty()
            ? info.taskName.trimmed()
            : QStringLiteral("Task %1").arg(taskId));

    auto node = std::make_shared<TaskTreeNode>();
    node->taskId = taskId;
    node->parentTaskId = parentId;
    node->taskName = effectiveName;
    node->state = info.isValid ? info.state : Application::Logging::TaskState::Running;
    node->progress = info.isValid ? info.progress : 0.0;
    node->currentMessage = info.isValid ? info.currentMessage : QString();
    node->messagesModel = std::shared_ptr<LogMessageListModel>(
        new LogMessageListModel(), [](QObject* obj) { obj->deleteLater(); });
    QQmlEngine::setObjectOwnership(node->messagesModel.get(), QQmlEngine::CppOwnership);

    const bool isTool = info.isValid ? info.isToolTask : false;
    node->isToolTask = isTool;

    if (isTool) {
        // Tool task: stored in m_nodesById for direct lookup by toolTaskId,
        // but excluded from visible task tree projection.
        m_nodesById.insert(taskId, node);
        if (parentId != 0) {
            const auto parentInfo = m_logService ? m_logService->taskInfo(parentId) : Application::Logging::TaskInfo{};
            auto parentNode = ensureTaskNode(
                parentId,
                parentInfo.isValid ? parentInfo.taskName : QString(),
                parentInfo.isValid ? &parentInfo : nullptr);
            node->parent = parentNode;
            node->depth = parentNode->depth + 1;
        }
    } else if (parentId == 0) {
        // Root task
        node->depth = 0;
        node->expanded = true;

        m_rootTasks.append(node);
        m_nodesById.insert(taskId, node);

        const int insertPos = m_visibleNodes.size();
        beginInsertRows(QModelIndex(), insertPos, insertPos);
        m_visibleNodes.append(node);
        endInsertRows();

        emit taskCountChanged();
    } else {
        // Child subtask
        const auto parentInfo = m_logService ? m_logService->taskInfo(parentId) : Application::Logging::TaskInfo{};
        auto parentNode = ensureTaskNode(
            parentId,
            parentInfo.isValid ? parentInfo.taskName : QString(),
            parentInfo.isValid ? &parentInfo : nullptr);

        node->depth = parentNode->depth + 1;
        node->expanded = false; // Default collapsed per accordion UX

        node->parent = parentNode;
        parentNode->children.append(node);
        m_nodesById.insert(taskId, node);

        // If parent is visible in flat projection, update its badge & insert child if parent is expanded
        const int parentRow = m_visibleNodes.indexOf(parentNode);
        if (parentRow >= 0) {
            const QModelIndex pIdx = index(parentRow, 0);
            emit dataChanged(pIdx, pIdx, {HasSubTasksRole, SubTasksCountRole});

            if (parentNode->expanded) {
                int insertPos = parentRow + 1;
                while (insertPos < m_visibleNodes.size() && m_visibleNodes.at(insertPos)->depth > parentNode->depth) {
                    insertPos++;
                }
                beginInsertRows(QModelIndex(), insertPos, insertPos);
                m_visibleNodes.insert(insertPos, node);
                endInsertRows();
                emit taskCountChanged();
            }
        }
    }

    if (!m_taskLogFiles.contains(taskId)) {
        QString taskFilePath;
        if (info.isValid && !info.logFilePath.isEmpty()) {
            taskFilePath = info.logFilePath;
        } else if (m_logService) {
            const qint64 startTimestamp = info.isValid ? info.startTimestamp : 0;
            taskFilePath = m_logService->fallbackTaskLogFilePath(node->taskName, startTimestamp, taskId);
        }
        m_taskLogFiles.insert(taskId, taskFilePath);
    }

    if (!node->isToolTask) {
        m_activeTaskId = taskId;
        m_lastTaskId = taskId;
    }

    return node;
}

std::shared_ptr<TaskTreeNode> LogViewModel::findOrResolveToolNode(quint64 toolTaskId)
{
    if (toolTaskId == 0) {
        return nullptr;
    }
    if (auto it = m_nodesById.constFind(toolTaskId); it != m_nodesById.constEnd()) {
        return it.value();
    }
    if (m_logService) {
        const Application::Logging::TaskInfo info = m_logService->taskInfo(toolTaskId);
        if (info.isValid) {
            return ensureTaskNode(toolTaskId, info.taskName, &info);
        }
    }
    return nullptr;
}

int LogViewModel::applyTaskUpdate(quint64 taskId, const QString& fallbackTaskName,
                                  const Application::Logging::TaskInfo* providedInfo,
                                  const QVector<Application::Logging::TaskLogMessage>& messages)
{
    Application::Logging::TaskInfo queriedInfo;
    const Application::Logging::TaskInfo* effectiveInfo = providedInfo;
    if ((!effectiveInfo || !effectiveInfo->isValid) && taskId != 0 && m_logService) {
        queriedInfo = m_logService->taskInfo(taskId);
        if (queriedInfo.isValid) {
            effectiveInfo = &queriedInfo;
        }
    }

    const QString initialName = !fallbackTaskName.trimmed().isEmpty()
        ? fallbackTaskName
        : (effectiveInfo && effectiveInfo->isValid ? effectiveInfo->taskName : QString());

    auto node = ensureTaskNode(taskId, initialName, effectiveInfo);

    int added = 0;
    if (!messages.isEmpty() && node->messagesModel) {
        node->messagesModel->appendEntries(messages);
        added = static_cast<int>(messages.size());
        m_totalMessages += added;
    }

    if (effectiveInfo && effectiveInfo->isValid) {
        node->state = effectiveInfo->state;
        node->progress = effectiveInfo->progress;
        node->currentMessage = effectiveInfo->currentMessage;
        const QString candidateName = !effectiveInfo->taskName.trimmed().isEmpty()
            ? effectiveInfo->taskName.trimmed()
            : fallbackTaskName.trimmed();
        if (!candidateName.isEmpty() && node->taskName.startsWith(QStringLiteral("Task "))) {
            node->taskName = candidateName;
        }
    }

    const int visibleRow = m_visibleNodes.indexOf(node);
    if (visibleRow >= 0) {
        const QModelIndex idx = index(visibleRow, 0);
        emit dataChanged(idx, idx, {
            TaskNameRole,
            StateRole,
            StateStringRole,
            ProgressRole,
            CurrentMessageRole,
            MessageCountRole
        });
    }

    if (node->isToolTask) {
        emit toolTaskUpdated(node->taskId);
    }

    return added;
}

void LogViewModel::processIncomingBlock(quint64 taskId, const QString& taskName,
                                        const QVector<Application::Logging::TaskLogMessage>& messages)
{
    if (messages.isEmpty() && taskId == 0) {
        return;
    }

    const int added = applyTaskUpdate(taskId, taskName, nullptr, messages);
    if (added > 0) {
        emit totalMessageCountChanged();
    }
}

void LogViewModel::toggleTaskExpanded(int visibleRow)
{
    if (visibleRow < 0 || visibleRow >= m_visibleNodes.size()) {
        return;
    }

    auto node = m_visibleNodes.at(visibleRow);

    if (node->expanded) {
        // --- COLLAPSE ---
        node->expanded = false;

        int removeCount = 0;
        for (int i = visibleRow + 1; i < m_visibleNodes.size(); ++i) {
            if (m_visibleNodes.at(i)->depth > node->depth) {
                removeCount++;
            } else {
                break;
            }
        }

        if (removeCount > 0) {
            beginRemoveRows(QModelIndex(), visibleRow + 1, visibleRow + removeCount);
            m_visibleNodes.remove(visibleRow + 1, removeCount);
            endRemoveRows();
            emit taskCountChanged();
        }

        const QModelIndex idx = index(visibleRow, 0);
        emit dataChanged(idx, idx, {ExpandedRole});
    } else {
        // --- EXPAND (with Sibling-Level Accordion) ---
        if (auto p = node->parent.lock()) {
            for (const auto& sib : p->children) {
                if (sib != node && sib->expanded) {
                    const int sibRow = m_visibleNodes.indexOf(sib);
                    if (sibRow >= 0) {
                        toggleTaskExpanded(sibRow);
                        visibleRow = m_visibleNodes.indexOf(node);
                    } else {
                        sib->expanded = false;
                    }
                }
            }
        } else {
            for (const auto& rootTask : m_rootTasks) {
                if (rootTask != node && rootTask->expanded) {
                    const int rootRow = m_visibleNodes.indexOf(rootTask);
                    if (rootRow >= 0) {
                        toggleTaskExpanded(rootRow);
                        visibleRow = m_visibleNodes.indexOf(node);
                    } else {
                        rootTask->expanded = false;
                    }
                }
            }
        }

        node->expanded = true;

        QVector<std::shared_ptr<TaskTreeNode>> toInsert;
        std::function<void(const std::shared_ptr<TaskTreeNode>&)> collect;
        collect = [&](const std::shared_ptr<TaskTreeNode>& n) {
            for (const auto& child : n->children) {
                toInsert.append(child);
                if (child->expanded) {
                    collect(child);
                }
            }
        };
        collect(node);

        if (!toInsert.isEmpty()) {
            const int insertPos = visibleRow + 1;
            beginInsertRows(QModelIndex(), insertPos, insertPos + toInsert.size() - 1);
            for (int i = 0; i < toInsert.size(); ++i) {
                m_visibleNodes.insert(insertPos + i, toInsert[i]);
            }
            endInsertRows();
            emit taskCountChanged();
        }

        const QModelIndex idx = index(visibleRow, 0);
        emit dataChanged(idx, idx, {ExpandedRole});
    }
}

void LogViewModel::setTaskExpanded(int visibleRow, bool expanded)
{
    if (visibleRow >= 0 && visibleRow < m_visibleNodes.size()) {
        if (m_visibleNodes.at(visibleRow)->expanded != expanded) {
            toggleTaskExpanded(visibleRow);
        }
    }
}

void LogViewModel::toggleTaskExpandedById(quint64 taskId)
{
    const int row = findRowByTaskId(taskId);
    if (row >= 0) {
        toggleTaskExpanded(row);
    }
}

void LogViewModel::clear()
{
    resetView();
}

void LogViewModel::expandAll()
{
    beginResetModel();
    m_visibleNodes.clear();
    std::function<void(const std::shared_ptr<TaskTreeNode>&)> expandRec;
    expandRec = [&](const std::shared_ptr<TaskTreeNode>& n) {
        n->expanded = true;
        m_visibleNodes.append(n);
        for (const auto& c : n->children) {
            expandRec(c);
        }
    };
    for (const auto& root : m_rootTasks) {
        expandRec(root);
    }
    endResetModel();
    emit taskCountChanged();
}

void LogViewModel::collapseAll()
{
    beginResetModel();
    m_visibleNodes.clear();
    std::function<void(const std::shared_ptr<TaskTreeNode>&)> collapseRec;
    collapseRec = [&](const std::shared_ptr<TaskTreeNode>& n) {
        n->expanded = false;
        for (const auto& c : n->children) {
            collapseRec(c);
        }
    };
    for (const auto& root : m_rootTasks) {
        collapseRec(root);
        m_visibleNodes.append(root);
    }
    endResetModel();
    emit taskCountChanged();
}

int LogViewModel::findRowByTaskId(quint64 taskId) const
{
    for (int i = 0; i < m_visibleNodes.size(); ++i) {
        if (m_visibleNodes.at(i)->taskId == taskId) {
            return i;
        }
    }
    return -1;
}

UI::ViewModels::LogMessageListModel* LogViewModel::taskMessagesModel(int row) const
{
    if (row >= 0 && row < m_visibleNodes.size()) {
        const auto& m = m_visibleNodes.at(row)->messagesModel;
        if (m) {
            QQmlEngine::setObjectOwnership(m.get(), QQmlEngine::CppOwnership);
            return m.get();
        }
    }
    return nullptr;
}

void LogViewModel::resetView()
{
    // Invalidate log updates published before this reset that may still be queued:
    // renewing the subscription makes onTaskUpdatesReceived drop them.
    if (m_logService && m_subscriptionId != 0) {
        m_logService->unsubscribe(m_subscriptionId);
        m_subscriptionId = m_logService->subscribe();
    }

    // Phase 1: Retain and clear child message models before dropping node references
    // so QML delegates detach cleanly prior to model reset.
    QVector<std::shared_ptr<LogMessageListModel>> msgModels;
    msgModels.reserve(m_nodesById.size());

    for (const auto& node : m_nodesById) {
        if (node && node->messagesModel) {
            msgModels.append(node->messagesModel);
            node->messagesModel->clear();
        }
    }

    // Phase 2: Teardown top-level view model and clear containers
    beginResetModel();
    m_rootTasks.clear();
    m_nodesById.clear();
    m_visibleNodes.clear();
    m_taskLogFiles.clear();
    m_activeTaskId = 0;
    m_lastTaskId = 0;
    m_totalMessages = 0;
    endResetModel();

    emit taskCountChanged();
    emit totalMessageCountChanged();
}

QString LogViewModel::fullLogText() const
{
    return exportToPlainText(0);
}

QString LogViewModel::exportToPlainText(int indentLevel) const
{
    QStringList result;
    std::function<void(const std::shared_ptr<TaskTreeNode>&, int)> exportNode;
    exportNode = [&](const std::shared_ptr<TaskTreeNode>& node, int indent) {
        const QString indentStr(indent * 2, QLatin1Char(' '));
        if (indent == 0) {
            result.append(QStringLiteral("=== %1 ===").arg(node->taskName));
        } else {
            result.append(QStringLiteral("%1--- %2 ---").arg(indentStr, node->taskName));
        }
        result.append(QString());

        if (node->messagesModel) {
            const auto& entries = node->messagesModel->entries();
            for (const auto& entry : entries) {
                QString levelStr;
                switch (entry.data.level) {
                case Application::Logging::LogLevel::Debug:    levelStr = QStringLiteral("DEBUG"); break;
                case Application::Logging::LogLevel::Info:     levelStr = QStringLiteral("INFO "); break;
                case Application::Logging::LogLevel::Warning:  levelStr = QStringLiteral("WARN "); break;
                case Application::Logging::LogLevel::Error:    levelStr = QStringLiteral("ERROR"); break;
                case Application::Logging::LogLevel::Critical: levelStr = QStringLiteral("CRIT "); break;
                }
                result.append(QStringLiteral("%1[%2] %3  %4")
                                  .arg(indentStr, entry.timestampString, levelStr, entry.data.message));
            }
        }

        for (const auto& child : node->children) {
            result.append(QString());
            exportNode(child, indent + 1);
        }
    };

    for (const auto& rootNode : m_rootTasks) {
        exportNode(rootNode, indentLevel);
        result.append(QString());
    }

    return result.join(QLatin1Char('\n'));
}

QString LogViewModel::activeTaskLogFilePath() const
{
    if (m_activeTaskId != 0) {
        const Application::Logging::TaskInfo info =
            m_logService ? m_logService->taskInfo(m_activeTaskId) : Application::Logging::TaskInfo{};
        if (info.isValid && info.state == Application::Logging::TaskState::Running) {
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
        targetPath = m_logService ? m_logService->applicationLogFilePath() : QString();
    }
    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        targetPath = m_logService ? m_logService->expectedApplicationLogFilePath() : QString();
    }

    if (targetPath.isEmpty() || !QFileInfo::exists(targetPath)) {
        return false;
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(targetPath));
}

QString LogViewModel::activeTaskLogFolderPath() const
{
    const quint64 targetId = m_activeTaskId != 0 ? m_activeTaskId : m_lastTaskId;
    if (targetId != 0) {
        const Application::Logging::TaskInfo info =
            m_logService ? m_logService->taskInfo(targetId) : Application::Logging::TaskInfo{};
        if (info.isValid && !info.workflowDirectory.isEmpty() && QDir(info.workflowDirectory).exists()) {
            return info.workflowDirectory;
        }
        const QString filePath = m_taskLogFiles.value(targetId);
        if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
            return QFileInfo(filePath).dir().absolutePath();
        }
    }
    return m_logService ? m_logService->logsDirectory() : QString();
}

bool LogViewModel::openLogFolder()
{
    QString targetFolder = activeTaskLogFolderPath();
    if (targetFolder.isEmpty() || !QDir(targetFolder).exists()) {
        targetFolder = m_logService ? m_logService->logsDirectory() : QString();
    }
    if (m_logService) {
        m_logService->ensureLogsDirectory();
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(targetFolder));
}

UI::ViewModels::LogMessageListModel* LogViewModel::toolMessagesModel(quint64 toolTaskId)
{
    auto node = findOrResolveToolNode(toolTaskId);
    if (node && node->messagesModel) {
        QQmlEngine::setObjectOwnership(node->messagesModel.get(), QQmlEngine::CppOwnership);
        return node->messagesModel.get();
    }
    return nullptr;
}

QString LogViewModel::toolTaskName(quint64 toolTaskId)
{
    auto node = findOrResolveToolNode(toolTaskId);
    return node ? node->taskName : QString();
}

QString LogViewModel::toolTaskState(quint64 toolTaskId)
{
    auto node = findOrResolveToolNode(toolTaskId);
    if (node) {
        return Application::Logging::taskStateToString(node->state);
    }
    return QStringLiteral("UNKNOWN");
}

QString LogViewModel::toolTaskLogFilePath(quint64 toolTaskId)
{
    findOrResolveToolNode(toolTaskId);
    return m_taskLogFiles.value(toolTaskId);
}

bool LogViewModel::openToolLogFile(quint64 toolTaskId)
{
    const QString path = toolTaskLogFilePath(toolTaskId);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString LogViewModel::toolFullLogText(quint64 toolTaskId)
{
    auto node = findOrResolveToolNode(toolTaskId);
    if (node && node->messagesModel) {
        QStringList lines;
        lines.append(QStringLiteral("=== Tool: %1 ===").arg(node->taskName));
        for (const auto& entry : node->messagesModel->entries()) {
            lines.append(QStringLiteral("[%1] %2").arg(entry.timestampString, entry.data.message));
        }
        return lines.join(QLatin1Char('\n'));
    }
    return QString();
}

bool LogViewModel::copyToolLogToClipboard(quint64 toolTaskId)
{
    const QString text = toolFullLogText(toolTaskId);
    if (text.isEmpty()) {
        return false;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    clipboard->setText(text);
    return true;
}

void LogViewModel::appendLog(const QString& message, int level)
{
    if (message.isEmpty()) {
        return;
    }
    QVector<Application::Logging::TaskLogMessage> messages;
    Application::Logging::TaskLogMessage entry;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();
    entry.level = static_cast<Application::Logging::LogLevel>(level);
    entry.message = message;
    messages.append(std::move(entry));
    processIncomingBlock(0, QCoreApplication::translate("LogViewModel", "General"), messages);
}

} // namespace UI::ViewModels
