#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QVector>
#include <deque>

#include "Application/Logging/TaskLogDTOs.h"

namespace UI::ViewModels {

/**
 * @brief Standard Qt ListModel for a single task's log messages.
 *
 * Uses std::deque for O(1) front eviction when bounded by maxEntries, and caches
 * per-second formatted timestamps so bursts within the same second share QString COW buffers.
 * Model mutations execute strictly on the owning UI thread.
 */
class LogMessageListModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int truncatedCount READ truncatedCount NOTIFY truncatedCountChanged)
    Q_PROPERTY(int maxEntries READ maxEntries WRITE setMaxEntries NOTIFY maxEntriesChanged)

public:
    static constexpr int kDefaultMaxEntries = 5000;

    struct Entry {
        Application::Logging::TaskLogMessage data;
        QString timestampString;
    };

    enum LogMessageRoles : int {
        SequenceRole = Qt::UserRole + 1,
        TimestampRole,
        TimestampStringRole,
        LevelRole,
        LevelStringRole,
        MessageRole,
        ToolTaskIdRole,
    };
    Q_ENUM(LogMessageRoles)

    explicit LogMessageListModel(QObject* parent = nullptr);
    ~LogMessageListModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int truncatedCount() const noexcept { return m_truncatedCount; }
    int maxEntries() const noexcept { return m_maxEntries; }
    void setMaxEntries(int maxEntries);

    void appendEntries(const QVector<Application::Logging::TaskLogMessage>& items);
    void clear();

    const std::deque<Entry>& entries() const;

signals:
    void countChanged();
    void truncatedCountChanged();
    void maxEntriesChanged();

private:
    QString formatTimestamp(qint64 timestamp);

    std::deque<Entry> m_entries;
    int m_maxEntries = kDefaultMaxEntries;
    int m_truncatedCount = 0;
    qint64 m_cachedSecond = -1;
    QString m_cachedTimestampString;
};

} // namespace UI::ViewModels
