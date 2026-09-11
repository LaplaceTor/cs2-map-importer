#include "UI/ViewModels/LogViewModel.h"
#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/ApplicationLogger.h"
#include "Core/Logging/LogFileManager.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
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
        for (const auto& node : m_nodesById) {
            if (node->subTasksModel) {
                node->subTasksModel->setAutoScroll(enabled);
            }
        }
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

    int row = index.row();
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
        return Core::Logging::taskStateToString(node->state);
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
    case SubTasksModelRole:
        if (node->subTasksModel) {
            QQmlEngine::setObjectOwnership(node->subTasksModel.get(), QQmlEngine::CppOwnership);
        }
        return QVariant::fromValue(node->subTasksModel.get());
    case MessagesRole: {
        QVariantList list;
        if (node->messagesModel) {
            const auto entries = node->messagesModel->entries();
            list.reserve(entries.size());
            for (const auto& msg : entries) {
                QVariantMap map;
                map.insert(QStringLiteral("sequence"), QVariant::fromValue(msg.sequence));
                map.insert(QStringLiteral("timestamp"), msg.timestamp);
                map.insert(QStringLiteral("timestampString"), msg.timestamp > 0
                    ? QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString(QStringLiteral("hh:mm:ss"))
                    : QStringLiteral("00:00:00"));
                map.insert(QStringLiteral("level"), static_cast<int>(msg.level));
                map.insert(QStringLiteral("levelString"), Core::Logging::logLevelToString(msg.level));
                map.insert(QStringLiteral("message"), msg.message);
                list.append(map);
            }
        }
        return list;
    }
    case Qt::DisplayRole:
        return node->taskName;
    default:
        return QVariant();
    }
}

bool LogViewModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
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
    QHash<int, QByteArray> roles;
    roles[TaskIdRole] = "taskId";
    roles[ParentTaskIdRole] = "parentTaskId";
    roles[DepthRole] = "depth";
    roles[TaskNameRole] = "taskName";
    roles[StateRole] = "state";
    roles[StateStringRole] = "stateString";
    roles[ProgressRole] = "progress";
    roles[CurrentMessageRole] = "currentMessage";
    roles[ExpandedRole] = "expanded";
    roles[MessageCountRole] = "messageCount";
    roles[SubTasksCountRole] = "subTasksCount";
    roles[HasSubTasksRole] = "hasSubTasks";
    roles[MessagesModelRole] = "messagesModel";
    roles[SubTasksModelRole] = "subTasksModel";
    roles[MessagesRole] = "messages";
    return roles;
}

int LogViewModel::taskCount() const
{
    return m_visibleNodes.size();
}

std::shared_ptr<TaskTreeNode> LogViewModel::ensureTaskNode(quint64 taskId, const QString& taskName)
{
    if (m_nodesById.contains(taskId)) {
        return m_nodesById.value(taskId);
    }

    auto context = Core::Logging::LogManager::instance().findTask(taskId);
    quint64 parentId = context ? context->parentTaskId() : 0;

    auto node = std::make_shared<TaskTreeNode>();
    node->taskId = taskId;
    node->parentTaskId = parentId;
    node->taskName = taskName.trimmed().isEmpty() ? QStringLiteral("Task %1").arg(taskId) : taskName.trimmed();
    node->state = context ? context->state() : Core::Logging::TaskState::Running;
    node->progress = context ? context->progress() : 0.0;
    node->currentMessage = context ? context->currentMessage() : QString();
    node->messagesModel = std::make_shared<LogMessageListModel>();
    QQmlEngine::setObjectOwnership(node->messagesModel.get(), QQmlEngine::CppOwnership);

    const bool isTool = context ? context->isToolTask() : false;
    node->isToolTask = isTool;

    if (isTool) {
        // Tool task: stored in m_nodesById for direct lookup by toolTaskId,
        // but excluded from visible task tree projection and parent subtask models.
        m_nodesById.insert(taskId, node);
        if (parentId != 0) {
            auto parentCtx = Core::Logging::LogManager::instance().findTask(parentId);
            auto parentNode = ensureTaskNode(parentId, parentCtx ? parentCtx->taskName() : QString());
            node->parent = parentNode;
            node->depth = parentNode->depth + 1;
        }
    } else if (parentId == 0) {
        // Root task
        node->depth = 0;
        node->expanded = true;
        node->subTasksModel = std::make_shared<LogTaskModel>(1);
        node->subTasksModel->setAutoScroll(m_autoScroll);
        QQmlEngine::setObjectOwnership(node->subTasksModel.get(), QQmlEngine::CppOwnership);

        m_rootTasks.append(node);
        m_nodesById.insert(taskId, node);

        int insertPos = m_visibleNodes.size();
        beginInsertRows(QModelIndex(), insertPos, insertPos);
        m_visibleNodes.append(node);
        endInsertRows();

        emit taskCountChanged();
    } else {
        // Child subtask
        auto parentCtx = Core::Logging::LogManager::instance().findTask(parentId);
        auto parentNode = ensureTaskNode(parentId, parentCtx ? parentCtx->taskName() : QString());

        node->depth = parentNode->depth + 1;
        node->expanded = false; // Default collapsed per grill decision
        node->subTasksModel = std::make_shared<LogTaskModel>(node->depth + 1);
        node->subTasksModel->setAutoScroll(m_autoScroll);
        QQmlEngine::setObjectOwnership(node->subTasksModel.get(), QQmlEngine::CppOwnership);

        node->parent = parentNode;
        parentNode->children.append(node);
        m_nodesById.insert(taskId, node);

        // Keep parentNode's subTasksModel in sync for backward compatibility / subModel access
        LogTaskItem childItem;
        childItem.taskId = taskId;
        childItem.parentTaskId = parentId;
        childItem.depth = node->depth;
        childItem.taskName = node->taskName;
        childItem.state = node->state;
        childItem.progress = node->progress;
        childItem.currentMessage = node->currentMessage;
        childItem.expanded = false;
        childItem.messagesModel = node->messagesModel;
        childItem.subTasksModel = node->subTasksModel;
        parentNode->subTasksModel->appendTask(childItem);

        // If parent is visible in flat projection, update its badge & insert child if parent is expanded
        int parentRow = m_visibleNodes.indexOf(parentNode);
        if (parentRow >= 0) {
            QModelIndex pIdx = index(parentRow, 0);
            emit dataChanged(pIdx, pIdx, {HasSubTasksRole, SubTasksCountRole});

            if (parentNode->expanded) {
                // Insert after parent and all preceding visible descendants
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
        if (context && !context->logFilePath().isEmpty()) {
            taskFilePath = context->logFilePath();
        } else {
            const qint64 startTimestamp = context ? context->startTimestamp() : 0;
            taskFilePath = Core::Logging::LogFileManager::generateTaskLogFilePath(node->taskName, startTimestamp, taskId);
        }
        m_taskLogFiles.insert(taskId, taskFilePath);
    }
    m_activeTaskId = taskId;
    m_lastTaskId = taskId;

    return node;
}

void LogViewModel::processIncomingBlock(const Core::Logging::LogBlock& block, const QString& taskName)
{
    quint64 taskId = block.taskId();
    const auto& entries = block.entries();
    if (entries.isEmpty() && taskId == 0) {
        return;
    }

    auto node = ensureTaskNode(taskId, taskName);

    if (!entries.isEmpty() && node->messagesModel) {
        QVector<LogMessageItem> newItems;
        newItems.reserve(entries.size());
        for (const auto& logEntry : entries) {
            LogMessageItem msgItem;
            msgItem.sequence = logEntry.sequence;
            msgItem.timestamp = logEntry.timestamp;
            msgItem.level = logEntry.level;
            msgItem.message = logEntry.message;
            msgItem.toolTaskId = logEntry.toolTaskId;
            newItems.append(msgItem);
        }
        node->messagesModel->appendEntries(newItems);
        m_totalMessages += entries.size();
    }

    const auto previousState = node->state;
    // Refresh state from context
    auto context = Core::Logging::LogManager::instance().findTask(taskId);
    if (context) {
        node->state = context->state();
        node->progress = context->progress();
        node->currentMessage = context->currentMessage();
        if (!taskName.trimmed().isEmpty() && node->taskName.startsWith(QStringLiteral("Task "))) {
            node->taskName = taskName.trimmed();
        }
    }

    if (m_autoScroll && previousState != Core::Logging::TaskState::Completed && node->state == Core::Logging::TaskState::Completed) {
        if (node->expanded) {
            int r = m_visibleNodes.indexOf(node);
            if (r >= 0) {
                toggleTaskExpanded(r);
            } else {
                node->expanded = false;
            }
        }
    }

    // Update parent's subTasksModel if applicable
    if (auto parentNode = node->parent.lock()) {
        int rowInParent = parentNode->subTasksModel->findRowByTaskId(taskId);
        if (rowInParent >= 0) {
            parentNode->subTasksModel->updateTaskMetadata(
                rowInParent, node->state, node->progress, node->currentMessage, node->taskName);
        }
    }

    // Notify flat projection view if node is visible
    int visibleRow = m_visibleNodes.indexOf(node);
    if (visibleRow >= 0) {
        QModelIndex idx = index(visibleRow, 0);
        emit dataChanged(idx, idx, {
            TaskNameRole,
            StateRole,
            StateStringRole,
            ProgressRole,
            CurrentMessageRole,
            MessageCountRole
        });
    }

    emit totalMessageCountChanged();
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
        if (auto p = node->parent.lock()) {
            int r = p->subTasksModel->findRowByTaskId(node->taskId);
            if (r >= 0) {
                p->subTasksModel->setTaskExpanded(r, false);
            }
        }

        // Count contiguous visible descendants
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

        QModelIndex idx = index(visibleRow, 0);
        emit dataChanged(idx, idx, {ExpandedRole});
    } else {
        // --- EXPAND (with Sibling-Level Accordion) ---
        if (auto p = node->parent.lock()) {
            for (const auto& sib : p->children) {
                if (sib != node && sib->expanded) {
                    int sibRow = m_visibleNodes.indexOf(sib);
                    if (sibRow >= 0) {
                        toggleTaskExpanded(sibRow);
                        // Re-evaluate visibleRow after siblings collapsed and removed rows
                        visibleRow = m_visibleNodes.indexOf(node);
                    } else {
                        sib->expanded = false;
                    }
                }
            }
        } else {
            // Sibling accordion for root tasks
            for (const auto& rootTask : m_rootTasks) {
                if (rootTask != node && rootTask->expanded) {
                    int rootRow = m_visibleNodes.indexOf(rootTask);
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
        if (auto p = node->parent.lock()) {
            int r = p->subTasksModel->findRowByTaskId(node->taskId);
            if (r >= 0) {
                p->subTasksModel->setTaskExpanded(r, true);
            }
        }

        // Collect visible children to insert
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
            int insertPos = visibleRow + 1;
            beginInsertRows(QModelIndex(), insertPos, insertPos + toInsert.size() - 1);
            for (int i = 0; i < toInsert.size(); ++i) {
                m_visibleNodes.insert(insertPos + i, toInsert[i]);
            }
            endInsertRows();
            emit taskCountChanged();
        }

        QModelIndex idx = index(visibleRow, 0);
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
    int row = findRowByTaskId(taskId);
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

std::shared_ptr<LogMessageListModel> LogViewModel::taskMessagesModel(int visibleRow) const
{
    if (visibleRow >= 0 && visibleRow < m_visibleNodes.size()) {
        return m_visibleNodes.at(visibleRow)->messagesModel;
    }
    return nullptr;
}

std::shared_ptr<LogTaskModel> LogViewModel::taskSubTasksModel(int visibleRow) const
{
    if (visibleRow >= 0 && visibleRow < m_visibleNodes.size()) {
        return m_visibleNodes.at(visibleRow)->subTasksModel;
    }
    return nullptr;
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

UI::ViewModels::LogMessageListModel* LogViewModel::getTaskMessagesModel(int row) const
{
    auto m = taskMessagesModel(row);
    return m ? m.get() : nullptr;
}

UI::ViewModels::LogTaskModel* LogViewModel::getTaskSubTasksModel(int row) const
{
    auto m = taskSubTasksModel(row);
    return m ? m.get() : nullptr;
}

void LogViewModel::resetView()
{
    m_viewGeneration.fetch_add(1, std::memory_order_relaxed);

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

QString LogViewModel::getFullLogText() const
{
    return exportToPlainText(0);
}

QString LogViewModel::exportToPlainText(int indentLevel) const
{
    QStringList result;
    std::function<void(const std::shared_ptr<TaskTreeNode>&, int)> exportNode;
    exportNode = [&](const std::shared_ptr<TaskTreeNode>& node, int indent) {
        QString indentStr(indent * 2, QLatin1Char(' '));
        if (indent == 0) {
            result.append(QStringLiteral("=== %1 ===").arg(node->taskName));
        } else {
            result.append(QStringLiteral("%1--- %2 ---").arg(indentStr, node->taskName));
        }
        result.append(QString());

        if (node->messagesModel) {
            const auto entries = node->messagesModel->entries();
            for (const auto& msg : entries) {
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
                result.append(QStringLiteral("%1[%2] %3  %4").arg(indentStr, timeStr, levelStr, msg.message));
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

QString LogViewModel::activeTaskLogFolderPath() const
{
    quint64 targetId = m_activeTaskId != 0 ? m_activeTaskId : m_lastTaskId;
    if (targetId != 0) {
        auto context = Core::Logging::LogManager::instance().findTask(targetId);
        if (context && !context->workflowDirectory().isEmpty() && QDir(context->workflowDirectory()).exists()) {
            return context->workflowDirectory();
        }
        QString filePath = m_taskLogFiles.value(targetId);
        if (!filePath.isEmpty() && QFileInfo::exists(filePath)) {
            return QFileInfo(filePath).dir().absolutePath();
        }
    }
    return Core::Logging::LogFileManager::logsDirectory();
}

bool LogViewModel::openLogFolder()
{
    QString targetFolder = activeTaskLogFolderPath();
    if (targetFolder.isEmpty() || !QDir(targetFolder).exists()) {
        targetFolder = Core::Logging::LogFileManager::logsDirectory();
    }
    Core::Logging::LogFileManager::ensureLogsDirectoryExists();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(targetFolder));
}

UI::ViewModels::LogMessageListModel* LogViewModel::getToolMessagesModel(quint64 toolTaskId)
{
    auto it = m_nodesById.find(toolTaskId);
    if (it != m_nodesById.end() && it.value() && it.value()->messagesModel) {
        QQmlEngine::setObjectOwnership(it.value()->messagesModel.get(), QQmlEngine::CppOwnership);
        return it.value()->messagesModel.get();
    }
    return nullptr;
}

QString LogViewModel::getToolTaskName(quint64 toolTaskId)
{
    auto it = m_nodesById.find(toolTaskId);
    if (it != m_nodesById.end() && it.value()) {
        return it.value()->taskName;
    }
    return QString();
}

QString LogViewModel::getToolTaskState(quint64 toolTaskId)
{
    auto it = m_nodesById.find(toolTaskId);
    if (it != m_nodesById.end() && it.value()) {
        return Core::Logging::taskStateToString(it.value()->state);
    }
    return QStringLiteral("UNKNOWN");
}

QString LogViewModel::getToolTaskLogFilePath(quint64 toolTaskId)
{
    return m_taskLogFiles.value(toolTaskId);
}

bool LogViewModel::openToolLogFile(quint64 toolTaskId)
{
    QString path = getToolTaskLogFilePath(toolTaskId);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString LogViewModel::getToolFullLogText(quint64 toolTaskId)
{
    auto it = m_nodesById.find(toolTaskId);
    if (it != m_nodesById.end() && it.value() && it.value()->messagesModel) {
        QStringList lines;
        lines.append(QStringLiteral("=== Tool: %1 ===").arg(it.value()->taskName));
        for (const auto& msg : it.value()->messagesModel->entries()) {
            QString timeStr = msg.timestamp > 0
                ? QDateTime::fromMSecsSinceEpoch(msg.timestamp).toString(QStringLiteral("hh:mm:ss"))
                : QStringLiteral("00:00:00");
            lines.append(QStringLiteral("[%1] %2").arg(timeStr, msg.message));
        }
        return lines.join(QLatin1Char('\n'));
    }
    return QString();
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
