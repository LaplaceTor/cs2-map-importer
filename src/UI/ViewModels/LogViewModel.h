#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

#include "Application/Logging/TaskLogDTOs.h"
#include "UI/ViewModels/LogMessageListModel.h"

namespace Application::Logging {
class TaskLogService;
}

namespace UI::ViewModels {

struct TaskTreeNode {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    int depth = 0;
    QString taskName;
    Application::Logging::TaskState state = Application::Logging::TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    bool expanded = false;
    bool isToolTask = false;
    std::shared_ptr<LogMessageListModel> messagesModel;
    QVector<std::shared_ptr<TaskTreeNode>> children;
    std::weak_ptr<TaskTreeNode> parent;
};

/**
 * @brief Top-level ViewModel for logs, presenting a dynamic flattened tree projection
 * to QML ListView while maintaining the canonical task hierarchy in C++.
 * Consumes the Application::Logging::TaskLogService facade exclusively; model mutations
 * execute strictly on the owning UI thread (guaranteed by TaskLogService coalesced delivery).
 */
class LogViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int taskCount READ taskCount NOTIFY taskCountChanged)
    Q_PROPERTY(int totalMessageCount READ totalMessageCount NOTIFY totalMessageCountChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)

public:
    enum LogTaskRoles : int {
        TaskIdRole = Qt::UserRole + 1,
        ParentTaskIdRole,
        DepthRole,
        TaskNameRole,
        StateRole,
        StateStringRole,
        ProgressRole,
        CurrentMessageRole,
        ExpandedRole,
        MessageCountRole,
        SubTasksCountRole,
        HasSubTasksRole,
        MessagesModelRole,
    };
    Q_ENUM(LogTaskRoles)

    explicit LogViewModel(Application::Logging::TaskLogService* logService = nullptr, QObject* parent = nullptr);
    ~LogViewModel() override;

    void attachToLogService(Application::Logging::TaskLogService* service);
    void detachFromLogService();

    // QAbstractListModel overrides for flattened projection
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    int taskCount() const;
    int totalMessageCount() const;
    bool autoScroll() const noexcept { return m_autoScroll; }
    void setAutoScroll(bool enabled);

    Q_INVOKABLE void clear();
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void toggleTaskExpanded(int visibleRow);
    Q_INVOKABLE void setTaskExpanded(int visibleRow, bool expanded);
    Q_INVOKABLE int findRowByTaskId(quint64 taskId) const;
    Q_INVOKABLE UI::ViewModels::LogMessageListModel* taskMessagesModel(int row) const;
    QString exportToPlainText(int indentLevel = 0) const;

public slots:
    Q_INVOKABLE void resetView();
    Q_INVOKABLE bool openLogFile();
    Q_INVOKABLE bool openLogFolder();
    Q_INVOKABLE void toggleTaskExpandedById(quint64 taskId);
    Q_INVOKABLE UI::ViewModels::LogMessageListModel* toolMessagesModel(quint64 toolTaskId);
    Q_INVOKABLE QString toolTaskName(quint64 toolTaskId);
    Q_INVOKABLE QString toolTaskState(quint64 toolTaskId);
    Q_INVOKABLE QString toolTaskLogFilePath(quint64 toolTaskId);
    Q_INVOKABLE bool openToolLogFile(quint64 toolTaskId);
    Q_INVOKABLE QString toolFullLogText(quint64 toolTaskId);
    Q_INVOKABLE bool copyToolLogToClipboard(quint64 toolTaskId);
    QString fullLogText() const;
    QString activeTaskLogFilePath() const;
    QString lastTaskLogFilePath() const;
    QString activeTaskLogFolderPath() const;
    void appendLog(const QString& message, int level = 0);
    void processIncomingBlock(quint64 taskId, const QString& taskName,
                              const QVector<Application::Logging::TaskLogMessage>& messages);

signals:
    void taskCountChanged();
    void totalMessageCountChanged();
    void autoScrollChanged();
    void toolTaskUpdated(quint64 toolTaskId);

private slots:
    void onTaskUpdatesReceived(quint64 subscriptionId,
                               const QVector<Application::Logging::TaskLogUpdate>& updates);

private:
    std::shared_ptr<TaskTreeNode> ensureTaskNode(quint64 taskId, const QString& taskName,
                                                 const Application::Logging::TaskInfo* providedInfo = nullptr);
    std::shared_ptr<TaskTreeNode> findOrResolveToolNode(quint64 toolTaskId);
    int applyTaskUpdate(quint64 taskId, const QString& fallbackTaskName,
                        const Application::Logging::TaskInfo* providedInfo,
                        const QVector<Application::Logging::TaskLogMessage>& messages);

    QVector<std::shared_ptr<TaskTreeNode>> m_rootTasks;
    QHash<quint64, std::shared_ptr<TaskTreeNode>> m_nodesById;
    QVector<std::shared_ptr<TaskTreeNode>> m_visibleNodes;
    QHash<quint64, QString> m_taskLogFiles;
    quint64 m_activeTaskId = 0;
    quint64 m_lastTaskId = 0;
    int m_totalMessages = 0;
    bool m_autoScroll = true;
    Application::Logging::TaskLogService* m_logService = nullptr;
    quint64 m_subscriptionId = 0;
};

} // namespace UI::ViewModels
