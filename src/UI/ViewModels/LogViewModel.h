#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

#include "Core/Logging/ILogSink.h"
#include "Core/Logging/LogBlock.h"
#include "Core/Logging/LogEntry.h"
#include "Core/Logging/LogLevel.h"
#include "Core/Logging/LogManager.h"
#include "Core/Logging/TaskState.h"

namespace UI::ViewModels {

struct LogMessageItem {
    quint64 sequence = 0;
    qint64 timestamp = 0;
    Core::Logging::LogLevel level = Core::Logging::LogLevel::Info;
    QString message;
    QString formattedHtml;
};

struct LogTaskItem {
    quint64 taskId = 0;
    QString taskName;
    Core::Logging::TaskState state = Core::Logging::TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    bool expanded = true;
    QVector<LogMessageItem> messages;
    QString formattedMessages;
};

class LogViewModel : public QAbstractListModel,
                     public Core::Logging::ILogSink,
                     public std::enable_shared_from_this<LogViewModel> {
    Q_OBJECT

    Q_PROPERTY(int taskCount READ taskCount NOTIFY taskCountChanged)
    Q_PROPERTY(int totalMessageCount READ totalMessageCount NOTIFY totalMessageCountChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY logTextChanged)
    Q_PROPERTY(QString formattedLogText READ formattedLogText NOTIFY logTextChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)

public:
    enum LogTaskRoles {
        TaskIdRole = Qt::UserRole + 1,
        TaskNameRole,
        StateRole,
        StateStringRole,
        ProgressRole,
        CurrentMessageRole,
        ExpandedRole,
        MessageCountRole,
        FormattedMessagesRole
    };
    Q_ENUM(LogTaskRoles)

    explicit LogViewModel(QObject* parent = nullptr);
    ~LogViewModel() override;

    void registerWithLogManager();
    void unregisterFromLogManager();

    // QAbstractListModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Property getters
    int taskCount() const;
    int totalMessageCount() const;
    int lineCount() const;
    QString formattedLogText() const;
    bool autoScroll() const noexcept { return m_autoScroll; }
    void setAutoScroll(bool enabled);

    // ILogSink implementation
    bool writeBlock(const Core::Logging::LogBlock& block, const QString& taskName) override;
    bool flush() override;

public slots:
    void clear();
    void copyToClipboard();
    QString getFullLogText() const;
    void expandAll();
    void collapseAll();
    void toggleTaskExpanded(int index);
    void setTaskExpanded(int index, bool expanded);
    void appendLog(const QString& message, int level = 0);

signals:
    void taskCountChanged();
    void totalMessageCountChanged();
    void logTextChanged();
    void autoScrollChanged();

private:
    void processIncomingBlock(const Core::Logging::LogBlock& block, const QString& taskName);
    QString formatMessageHtml(const LogMessageItem& item) const;

    mutable QRecursiveMutex m_mutex;
    QVector<LogTaskItem> m_tasks;
    QHash<quint64, int> m_taskIdToIndex;
    int m_totalMessages = 0;
    bool m_autoScroll = true;
};

} // namespace UI::ViewModels
