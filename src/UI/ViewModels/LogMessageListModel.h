#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

#include "Core/Logging/LogLevel.h"

namespace UI::ViewModels {

struct LogMessageItem {
    quint64 sequence = 0;
    qint64 timestamp = 0;
    Core::Logging::LogLevel level = Core::Logging::LogLevel::Info;
    QString message;
};

/**
 * @brief Standard Qt ListModel for a single task's log messages.
 * Note: Model mutations execute strictly on the owning UI thread (guaranteed by LogViewModelSinkAdapter).
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
        MessageRole
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
