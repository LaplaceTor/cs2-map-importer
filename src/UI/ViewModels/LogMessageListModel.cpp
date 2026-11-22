#include "UI/ViewModels/LogMessageListModel.h"

#include <QDateTime>
#include <QQmlEngine>

namespace UI::ViewModels {

LogMessageListModel::LogMessageListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
}

int LogMessageListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant LogMessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_entries.size())) {
        return QVariant();
    }

    const Entry& entry = m_entries[static_cast<size_t>(row)];

    switch (role) {
    case SequenceRole:
        return QVariant::fromValue(entry.data.sequence);
    case TimestampRole:
        return entry.data.timestamp;
    case TimestampStringRole:
        return entry.timestampString;
    case LevelRole:
        return static_cast<int>(entry.data.level);
    case LevelStringRole:
        return Application::Logging::logLevelToString(entry.data.level);
    case MessageRole:
    case Qt::DisplayRole:
        return entry.data.message;
    case ToolTaskIdRole:
        return QVariant::fromValue(entry.data.toolTaskId);
    }

    return QVariant();
}

QHash<int, QByteArray> LogMessageListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = []() {
        QHash<int, QByteArray> r;
        r[SequenceRole] = "sequence";
        r[TimestampRole] = "timestamp";
        r[TimestampStringRole] = "timestampString";
        r[LevelRole] = "level";
        r[LevelStringRole] = "levelString";
        r[MessageRole] = "message";
        r[ToolTaskIdRole] = "toolTaskId";
        return r;
    }();
    return roles;
}

int LogMessageListModel::count() const
{
    return static_cast<int>(m_entries.size());
}

void LogMessageListModel::setMaxEntries(int maxEntries)
{
    if (m_maxEntries == maxEntries) {
        return;
    }
    m_maxEntries = maxEntries;

    if (m_maxEntries > 0 && static_cast<int>(m_entries.size()) > m_maxEntries) {
        const int overflow = static_cast<int>(m_entries.size()) - m_maxEntries;
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        for (int i = 0; i < overflow; ++i) {
            m_entries.pop_front();
        }
        endRemoveRows();
        m_truncatedCount += overflow;
        emit countChanged();
        emit truncatedCountChanged();
    }

    emit maxEntriesChanged();
}

void LogMessageListModel::appendEntries(const QVector<Application::Logging::TaskLogMessage>& items)
{
    if (items.isEmpty()) {
        return;
    }

    const int oldSize = static_cast<int>(m_entries.size());
    const int addedSize = static_cast<int>(items.size());
    const int oldTruncated = m_truncatedCount;

    if (m_maxEntries > 0 && addedSize >= m_maxEntries) {
        const int skipCount = addedSize - m_maxEntries;
        beginResetModel();
        m_entries.clear();
        for (int i = skipCount; i < addedSize; ++i) {
            const auto& item = items.at(i);
            m_entries.push_back(Entry{item, formatTimestamp(item.timestamp)});
        }
        m_truncatedCount += oldSize + skipCount;
        endResetModel();
    } else if (m_maxEntries > 0 && oldSize + addedSize > m_maxEntries) {
        const int overflow = oldSize + addedSize - m_maxEntries;
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        for (int i = 0; i < overflow; ++i) {
            m_entries.pop_front();
        }
        endRemoveRows();
        m_truncatedCount += overflow;

        const int start = static_cast<int>(m_entries.size());
        const int end = start + addedSize - 1;
        beginInsertRows(QModelIndex(), start, end);
        for (const auto& item : items) {
            m_entries.push_back(Entry{item, formatTimestamp(item.timestamp)});
        }
        endInsertRows();
    } else {
        const int start = oldSize;
        const int end = start + addedSize - 1;
        beginInsertRows(QModelIndex(), start, end);
        for (const auto& item : items) {
            m_entries.push_back(Entry{item, formatTimestamp(item.timestamp)});
        }
        endInsertRows();
    }

    emit countChanged();
    if (m_truncatedCount != oldTruncated) {
        emit truncatedCountChanged();
    }
}

void LogMessageListModel::clear()
{
    const bool hadTruncated = m_truncatedCount > 0;
    beginResetModel();
    m_entries.clear();
    m_truncatedCount = 0;
    endResetModel();

    emit countChanged();
    if (hadTruncated) {
        emit truncatedCountChanged();
    }
}

const std::deque<LogMessageListModel::Entry>& LogMessageListModel::entries() const
{
    return m_entries;
}

QString LogMessageListModel::formatTimestamp(qint64 timestamp)
{
    if (timestamp <= 0) {
        return QStringLiteral("00:00:00");
    }
    const qint64 second = timestamp / 1000;
    if (second != m_cachedSecond) {
        m_cachedSecond = second;
        m_cachedTimestampString =
            QDateTime::fromMSecsSinceEpoch(timestamp).toString(QStringLiteral("hh:mm:ss"));
    }
    return m_cachedTimestampString;
}

} // namespace UI::ViewModels
