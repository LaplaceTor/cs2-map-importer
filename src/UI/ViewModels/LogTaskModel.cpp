#include "UI/ViewModels/LogTaskModel.h"
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

namespace UI::ViewModels {

LogTaskModel::LogTaskModel(int depth, QObject* parent)
    : QAbstractListModel(parent)
    , m_depth(depth)
{
}

int LogTaskModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

QVariant LogTaskModel::data(const QModelIndex& index, int role) const
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
        return QVariant::fromValue(task.messagesModel.get());
    case SubTasksModelRole:
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

    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, index, value, role]() {
            setData(index, value, role);
        }, Qt::QueuedConnection);
        return true;
    }

    int row = index.row();
    bool changed = false;

    if (role == ExpandedRole || role == Qt::EditRole) {
        bool newExpanded = value.toBool();
        {
            QMutexLocker locker(&m_mutex);
            if (row >= 0 && row < m_tasks.size()) {
                if (m_tasks[row].expanded != newExpanded) {
                    m_tasks[row].expanded = newExpanded;
                    changed = true;
                }
            }
        }
        if (changed) {
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

int LogTaskModel::taskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.size();
}

int LogTaskModel::appendTask(const LogTaskItem& task)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, task]() {
            appendTask(task);
        }, Qt::QueuedConnection);
        return -1;
    }

    int newRow = m_tasks.size();

    beginInsertRows(QModelIndex(), newRow, newRow);
    {
        QMutexLocker locker(&m_mutex);
        m_tasks.append(task);
        m_taskIdToRow.insert(task.taskId, newRow);
    }
    endInsertRows();

    emit taskCountChanged();
    return newRow;
}

bool LogTaskModel::updateTaskMetadata(int row, Core::Logging::TaskState state, double progress, const QString& currentMessage, const QString& taskName)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, row, state, progress, currentMessage, taskName]() {
            updateTaskMetadata(row, state, progress, currentMessage, taskName);
        }, Qt::QueuedConnection);
        return true;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (row < 0 || row >= m_tasks.size()) {
            return false;
        }
        auto& task = m_tasks[row];
        task.state = state;
        task.progress = progress;
        task.currentMessage = currentMessage;
        if (!taskName.trimmed().isEmpty() && task.taskName.startsWith(QStringLiteral("Task "))) {
            task.taskName = taskName.trimmed();
        }
    }

    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {
        TaskNameRole,
        StateRole,
        StateStringRole,
        ProgressRole,
        CurrentMessageRole,
        MessageCountRole,
        SubTasksCountRole,
        HasSubTasksRole
    });
    return true;
}

LogTaskItem* LogTaskModel::getTaskItem(int row)
{
    QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return &m_tasks[row];
}

const LogTaskItem* LogTaskModel::getTaskItem(int row) const
{
    QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return &m_tasks.at(row);
}

int LogTaskModel::findRowByTaskId(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_taskIdToRow.value(taskId, -1);
}

LogMessageListModel* LogTaskModel::getTaskMessagesModel(int row) const
{
    QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return m_tasks[row].messagesModel.get();
}

LogTaskModel* LogTaskModel::getTaskSubTasksModel(int row) const
{
    QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_tasks.size()) {
        return nullptr;
    }
    return m_tasks[row].subTasksModel.get();
}

void LogTaskModel::clear()
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            clear();
        }, Qt::QueuedConnection);
        return;
    }

    QVector<std::shared_ptr<LogMessageListModel>> msgModels;
    QVector<std::shared_ptr<LogTaskModel>> subModels;

    beginResetModel();
    {
        QMutexLocker locker(&m_mutex);
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
    }
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
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            expandAll();
        }, Qt::QueuedConnection);
        return;
    }

    QVector<std::shared_ptr<LogTaskModel>> childModels;
    int count = 0;
    {
        QMutexLocker locker(&m_mutex);
        count = m_tasks.size();
        for (int i = 0; i < count; ++i) {
            m_tasks[i].expanded = true;
            if (m_tasks[i].subTasksModel) {
                childModels.append(m_tasks[i].subTasksModel);
            }
        }
    }

    for (const auto& child : childModels) {
        child->expandAll();
    }

    if (count > 0) {
        emit dataChanged(index(0, 0), index(count - 1, 0), {ExpandedRole});
    }
}

void LogTaskModel::collapseAll()
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            collapseAll();
        }, Qt::QueuedConnection);
        return;
    }

    QVector<std::shared_ptr<LogTaskModel>> childModels;
    int count = 0;
    {
        QMutexLocker locker(&m_mutex);
        count = m_tasks.size();
        for (int i = 0; i < count; ++i) {
            m_tasks[i].expanded = false;
            if (m_tasks[i].subTasksModel) {
                childModels.append(m_tasks[i].subTasksModel);
            }
        }
    }

    for (const auto& child : childModels) {
        child->collapseAll();
    }

    if (count > 0) {
        emit dataChanged(index(0, 0), index(count - 1, 0), {ExpandedRole});
    }
}

void LogTaskModel::toggleTaskExpanded(int index)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, index]() {
            toggleTaskExpanded(index);
        }, Qt::QueuedConnection);
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (index >= 0 && index < m_tasks.size()) {
            m_tasks[index].expanded = !m_tasks[index].expanded;
            changed = true;
        }
    }
    if (changed) {
        QModelIndex modelIdx = this->index(index, 0);
        emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
    }
}

void LogTaskModel::setTaskExpanded(int index, bool expanded)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, index, expanded]() {
            setTaskExpanded(index, expanded);
        }, Qt::QueuedConnection);
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (index >= 0 && index < m_tasks.size()) {
            if (m_tasks[index].expanded != expanded) {
                m_tasks[index].expanded = expanded;
                changed = true;
            }
        }
    }
    if (changed) {
        QModelIndex modelIdx = this->index(index, 0);
        emit dataChanged(modelIdx, modelIdx, {ExpandedRole});
    }
}

QString LogTaskModel::exportToPlainText(int indentLevel) const
{
    QMutexLocker locker(&m_mutex);
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
