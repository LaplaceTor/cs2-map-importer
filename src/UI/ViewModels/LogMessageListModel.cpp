#include "UI/ViewModels/LogMessageListModel.h"
#include <QMutexLocker>

namespace UI::ViewModels {

LogMessageListModel::LogMessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LogMessageListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    QMutexLocker locker(&m_mutex);
    return m_entries.size();
}

QVariant LogMessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    QMutexLocker locker(&m_mutex);
    int row = index.row();
    if (row < 0 || row >= m_entries.size()) {
        return QVariant();
    }

    const auto& item = m_entries.at(row);

    switch (role) {
    case SequenceRole:
        return QVariant::fromValue(item.sequence);
    case TimestampRole:
        return item.timestamp;
    case TimestampStringRole:
        return item.timestamp > 0
            ? QDateTime::fromMSecsSinceEpoch(item.timestamp).toString(QStringLiteral("hh:mm:ss"))
            : QStringLiteral("00:00:00");
    case LevelRole:
        return static_cast<int>(item.level);
    case LevelStringRole:
        return Core::Logging::logLevelToString(item.level);
    case MessageRole:
    case Qt::DisplayRole:
        return item.message;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> LogMessageListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SequenceRole] = "sequence";
    roles[TimestampRole] = "timestamp";
    roles[TimestampStringRole] = "timestampString";
    roles[LevelRole] = "level";
    roles[LevelStringRole] = "levelString";
    roles[MessageRole] = "message";
    return roles;
}

int LogMessageListModel::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries.size();
}

void LogMessageListModel::appendEntries(const QVector<LogMessageItem>& items)
{
    if (items.isEmpty()) {
        return;
    }

    int start = 0;
    int end = 0;
    {
        QMutexLocker locker(&m_mutex);
        start = m_entries.size();
        end = start + items.size() - 1;
    }

    beginInsertRows(QModelIndex(), start, end);
    {
        QMutexLocker locker(&m_mutex);
        m_entries.append(items);
    }
    endInsertRows();

    emit countChanged();
}

void LogMessageListModel::clear()
{
    beginResetModel();
    {
        QMutexLocker locker(&m_mutex);
        m_entries.clear();
    }
    endResetModel();

    emit countChanged();
}

QVector<LogMessageItem> LogMessageListModel::entries() const
{
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

} // namespace UI::ViewModels
