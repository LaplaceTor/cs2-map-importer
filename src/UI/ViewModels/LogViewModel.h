#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <memory>

#include "Application/Logging/TaskLogDTOs.h"
#include "UI/ViewModels/LogMessageListModel.h"
#include "UI/ViewModels/LogTaskModel.h"

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
    std::shared_ptr<LogTaskModel> subTasksModel;
    QVector<std::shared_ptr<TaskTreeNode>> children;
    std::weak_ptr<TaskTreeNode> parent;
};

struct TaskRegistryEntry {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    int depth = 0;
    std::shared_ptr<TaskTreeNode> node;
    std::shared_ptr<LogMessageListModel> messagesModel;
    std::shared_ptr<LogTaskModel> subTasksModel;
};

/**
 * @brief Top-level ViewModel for logs, presenting a dynamic flattened tree projection
 * to QML ListView while maintaining the canonical task hierarchy in C++.
 * Consumes the Application::Logging::TaskLogService facade exclusively; model mutations
 * execute strictly on the owning UI thread (guaranteed by TaskLogService queued delivery).
 */
class LogViewModel : public LogTaskModel {
    Q_OBJECT

    Q_PROPERTY(int totalMessageCount READ totalMessageCount NOTIFY totalMessageCountChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)

public:
    explicit LogViewModel(Application::Logging::TaskLogService* logService = nullptr, QObject* parent = nullptr);
    ~LogViewModel() override;

    void attachToLogService(Application::Logging::TaskLogService* service);
    void detachFromLogService();

    // QAbstractListModel overrides for flattened projection
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    // LogTaskModel overrides
    int taskCount() const override;
    void clear() override;
    void expandAll() override;
    void collapseAll() override;
    void toggleTaskExpanded(int visibleRow) override;
    void setTaskExpanded(int visibleRow, bool expanded) override;
    std::shared_ptr<LogMessageListModel> taskMessagesModel(int visibleRow) const override;
    std::shared_ptr<LogTaskModel> taskSubTasksModel(int visibleRow) const override;
    Q_INVOKABLE int findRowByTaskId(quint64 taskId) const override;
    UI::ViewModels::LogMessageListModel* getTaskMessagesModel(int row) const override;
    UI::ViewModels::LogTaskModel* getTaskSubTasksModel(int row) const override;
    QString exportToPlainText(int indentLevel = 0) const override;

    // Property getters
    int totalMessageCount() const;
    bool autoScroll() const noexcept { return m_autoScroll; }
    void setAutoScroll(bool enabled);

public slots:
    Q_INVOKABLE void resetView();
    Q_INVOKABLE bool openLogFile();
    Q_INVOKABLE bool openLogFolder();
    Q_INVOKABLE void toggleTaskExpandedById(quint64 taskId);
    Q_INVOKABLE UI::ViewModels::LogMessageListModel* getToolMessagesModel(quint64 toolTaskId);
    Q_INVOKABLE QString getToolTaskName(quint64 toolTaskId);
    Q_INVOKABLE QString getToolTaskState(quint64 toolTaskId);
    Q_INVOKABLE QString getToolTaskLogFilePath(quint64 toolTaskId);
    Q_INVOKABLE bool openToolLogFile(quint64 toolTaskId);
    Q_INVOKABLE QString getToolFullLogText(quint64 toolTaskId);
    QString getFullLogText() const;
    QString activeTaskLogFilePath() const;
    QString lastTaskLogFilePath() const;
    QString activeTaskLogFolderPath() const;
    void appendLog(const QString& message, int level = 0);
    void processIncomingBlock(quint64 taskId, const QString& taskName,
                              const QVector<Application::Logging::TaskLogMessage>& messages);

signals:
    void totalMessageCountChanged();
    void autoScrollChanged();

private slots:
    void onLogBatchReceived(quint64 subscriptionId, quint64 taskId, const QString& taskName,
                            const QVector<Application::Logging::TaskLogMessage>& messages);

private:
    std::shared_ptr<TaskTreeNode> ensureTaskNode(quint64 taskId, const QString& taskName);
    void collectVisibleDescendants(const std::shared_ptr<TaskTreeNode>& node, QVector<std::shared_ptr<TaskTreeNode>>& out) const;

    QVector<std::shared_ptr<TaskTreeNode>> m_rootTasks;
    QHash<quint64, std::shared_ptr<TaskTreeNode>> m_nodesById;
    QVector<std::shared_ptr<TaskTreeNode>> m_visibleNodes;
    QHash<quint64, QString> m_taskLogFiles;
    quint64 m_activeTaskId = 0;
    quint64 m_lastTaskId = 0;
    int m_totalMessages = 0;
    Application::Logging::TaskLogService* m_logService = nullptr;
    quint64 m_subscriptionId = 0;
};

} // namespace UI::ViewModels
