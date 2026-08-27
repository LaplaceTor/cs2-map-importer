#include "UI/ViewModels/LogTaskModel.h"
#include <QQmlEngine>

namespace UI::ViewModels {

LogTaskModel::LogTaskModel(int depth, QObject* parent)
    : QAbstractListModel(parent)
    , m_depth(depth)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

int LogTaskModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tasks.size();
}

QVariant LogTaskModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    int row = index.row();
    if (row < 0 || row >= m_tasks.size()) {
        return QVariant();
    }

    const auto& task = m_tasks.at(row);

    switch (role) {
    case TaskIdRole:
        return QVariant::fromValue(task.taskId);
    case ParentTaskIdRole:
        return QVariant::fromValue(task.parentTaskId);
    case DepthRole:
        return task.depth;
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
        return task.messagesModel ? task.messagesModel->count() : 0;
    case SubTasksCountRole:
        return task.subTasksModel ? task.subTasksModel->taskCount() : 0;
    case HasSubTasksRole:
        return task.subTasksModel ? (task.subTasksModel->taskCount() > 0) : false;
    case MessagesModelRole:
        if (task.messagesModel) {
            QQmlEngine::setObjectOwnership(task.messagesModel.get(), QQmlEngine::CppOwnership);
        }
        return QVariant::fromValue(task.messagesModel.get());
    case SubTasksModelRole:
        if (task.subTasksModel) {
            QQmlEngine::setObjectOwnership(task.subTasksModel.get(), QQmlEngine::CppOwnership);
        }
        return QVariant::fromValue(task.subTasksModel.get());
    case MessagesRole: {
        QVariantList list;
        if (task.messagesModel) {
            const auto entries = task.messagesModel->entries();
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
        return task.taskName;
    default:
        return QVariant();
    }
}

bool LogTaskModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    if (row < 0 || row >= m_tasks.size()) {
        return false;
    }

    if (role == ExpandedRole || role == Qt::EditRole) {
        bool newExpanded = value.toBool();
        if (m_tasks[row].expanded != newExpanded) {
            m_tasks[row].expanded = newExpanded;
            emit dataChanged(index, index, {ExpandedRole});
            return true;
        }
    }

    return false;
}

QHash<int, QByteArray> LogTaskModel::roleNames() const
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

void LogTaskModel::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
}

int LogTaskModel::taskCount() const
{
    return m_tasks.size();
}

int LogTaskModel::appendTask(const LogTaskItem& task)
{
    int newRow = m_tasks.size();

    beginInsertRows(QModelIndex(), newRow, newRow);
    m_tasks.append(task);
    m_taskIdToRow.insert(task.taskId, newRow);
    endInsertRows();

    emit taskCountChanged();
    return newRow;
}

bool LogTaskModel::updateTaskMetadata(int row, Core::Logging::TaskState state, double progress, const QString& currentMessage, const QString& taskName)
{
    if (row < 0 || row >= m_tasks.size()) {
        return false;
    }
    auto& task = m_tasks[row];
    const auto previousState = task.state;
    task.state = state;
    task.progress = progress;
    task.currentMessage = currentMessage;
    if (!taskName.trimmed().isEmpty() && task.taskName.startsWith(QStringLiteral("Task "))) {
        task.taskName = taskName.trimmed();
    }

    QVector<int> changedRoles = {
        TaskNameRole,
        StateRole,
        StateStringRole,
        ProgressRole,
        CurrentMessageRole,
        MessageCountRole,
        SubTasksCountRole,
        HasSubTasksRole
    };

    if (m_autoScroll && previousState != Core::Logging::TaskState::Completed && state == Core::Logging::TaskState::Completed) {
        task.expanded = false;
        changedRoles.append(ExpandedRole);
    }

    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, changedRoles);
    return true;
}

std::optional<LogTaskItem> LogTaskModel::taskSnapshot(int row) const
{
    if (row < 0 || row >= m_tasks.size()) {
        return std::nullopt;
    }
    return m_tasks.at(row);
}

std::shared_ptr<LogMessageListModel> LogTaskModel::taskMessagesModel(int row) const
{
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return m_tasks.at(row).messagesModel;
}

std::shared_ptr<LogTaskModel> LogTaskModel::taskSubTasksModel(int row) const
{
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return m_tasks.at(row).subTasksModel;
}

int LogTaskModel::findRowByTaskId(quint64 taskId) const
{
    return m_taskIdToRow.value(taskId, -1);
}

LogMessageListModel* LogTaskModel::getTaskMessagesModel(int row) const
{
    auto ptr = taskMessagesModel(row);
    if (ptr) {
        QQmlEngine::setObjectOwnership(ptr.get(), QQmlEngine::CppOwnership);
    }
    return ptr.get();
}

LogTaskModel* LogTaskModel::getTaskSubTasksModel(int row) const
{
    auto ptr = taskSubTasksModel(row);
    if (ptr) {
        QQmlEngine::setObjectOwnership(ptr.get(), QQmlEngine::CppOwnership);
    }
    return ptr.get();
}

void LogTaskModel::clear()
{
    QVector<std::shared_ptr<LogMessageListModel>> msgModels;
    QVector<std::shared_ptr<LogTaskModel>> subModels;

    beginResetModel();
    for (auto& task : m_tasks) {
        if (task.messagesModel) {
            msgModels.append(task.messagesModel);
        }
        if (task.subTasksModel) {
            subModels.append(task.subTasksModel);
        }
    }
    m_tasks.clear();
    m_taskIdToRow.clear();
    endResetModel();

    for (const auto& msgModel : msgModels) {
        msgModel->clear();
    }
    for (const auto& subModel : subModels) {
        subModel->clear();
    }

    emit taskCountChanged();
}

void LogTaskModel::expandAll()
{
    int count = m_tasks.size();
    for (int i = 0; i < count; ++i) {
        m_tasks[i].expanded = true;
        if (m_tasks[i].subTasksModel) {
            m_tasks[i].subTasksModel->expandAll();
        }
    }

    if (count > 0) {
        emit dataChanged(index(0, 0), index(count - 1, 0), {ExpandedRole});
    }
}

void LogTaskModel::collapseAll()
{
    int count = m_tasks.size();
    for (int i = 0; i < count; ++i) {
        m_tasks[i].expanded = false;
        if (m_tasks[i].subTasksModel) {
            m_tasks[i].subTasksModel->collapseAll();
        }
    }

    if (count > 0) {
        emit dataChanged(index(0, 0), index(count - 1, 0), {ExpandedRole});
    }
}

void LogTaskModel::toggleTaskExpanded(int index)
{
    if (index >= 0 && index < m_tasks.size()) {
        m_tasks[index].expanded = !m_tasks[index].expanded;
        QModelIndex modelIdx = this->index(index, 0);
        emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
    }
}

void LogTaskModel::setTaskExpanded(int index, bool expanded)
{
    if (index >= 0 && index < m_tasks.size()) {
        if (m_tasks[index].expanded != expanded) {
            m_tasks[index].expanded = expanded;
            QModelIndex modelIdx = this->index(index, 0);
            emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
        }
    }
}

QString LogTaskModel::exportToPlainText(int indentLevel) const
{
    QString indent(indentLevel * 2, QLatin1Char(' '));
    QStringList result;

    for (const auto& task : m_tasks) {
        if (indentLevel == 0) {
            result.append(QStringLiteral("=== %1 ===").arg(task.taskName));
        } else {
            result.append(QStringLiteral("%1--- %2 ---").arg(indent, task.taskName));
        }
        result.append(QString());

        if (task.messagesModel) {
            const auto entries = task.messagesModel->entries();
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
                result.append(QStringLiteral("%1[%2] %3  %4").arg(indent, timeStr, levelStr, msg.message));
            }
        }

        if (task.subTasksModel && task.subTasksModel->taskCount() > 0) {
            result.append(QString());
            result.append(task.subTasksModel->exportToPlainText(indentLevel + 1));
        }
        result.append(QString());
    }

    return result.join(QLatin1Char('\n'));
}

} // namespace UI::ViewModels
