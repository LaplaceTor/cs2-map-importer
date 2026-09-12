#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

#include "Application/Logging/TaskLogDTOs.h"

namespace UI::ViewModels {

struct LogMessageItem {
    quint64 sequence = 0;
    qint64 timestamp = 0;
    Application::Logging::LogLevel level = Application::Logging::LogLevel::Info;
    QString message;
    quint64 toolTaskId = 0;
};

/**
 * @brief Standard Qt ListModel for a single task's log messages.
 * Note: Model mutations execute strictly on the owning UI thread (guaranteed by TaskLogService queued delivery).
 */
class LogMessageListModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum LogMessageRoles {
        SequenceRole = Qt::UserRole + 1,
        TimestampRole,
        TimestampStringRole,
        LevelRole,
        LevelStringRole,
        MessageRole,
        ToolTaskIdRole
    };
    Q_ENUM(LogMessageRoles)

    explicit LogMessageListModel(QObject* parent = nullptr);
    ~LogMessageListModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    void appendEntries(const QVector<LogMessageItem>& items);
    void clear();

    QVector<LogMessageItem> entries() const;

signals:
    void countChanged();

private:
    QVector<LogMessageItem> m_entries;
};

} // namespace UI::ViewModels
