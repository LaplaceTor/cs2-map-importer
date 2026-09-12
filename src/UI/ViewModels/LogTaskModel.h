#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>
#include <memory>
#include <optional>

#include "Application/Logging/TaskLogDTOs.h"
#include "UI/ViewModels/LogMessageListModel.h"

namespace UI::ViewModels {

class LogTaskModel;

struct LogTaskItem {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    int depth = 0;
    QString taskName;
    Application::Logging::TaskState state = Application::Logging::TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    bool expanded = true;
    std::shared_ptr<LogMessageListModel> messagesModel;
    std::shared_ptr<LogTaskModel> subTasksModel;
};

/**
 * @brief Standard Qt ListModel for hierarchical Task items.
 * Note: Model mutations execute strictly on the owning UI thread (guaranteed by TaskLogService queued delivery).
 */
class LogTaskModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int taskCount READ taskCount NOTIFY taskCountChanged)

public:
    enum LogTaskRoles {
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
        SubTasksModelRole,
        MessagesRole
    };
    Q_ENUM(LogTaskRoles)

    explicit LogTaskModel(int depth = 0, QObject* parent = nullptr);
    ~LogTaskModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    virtual int taskCount() const;
    int depth() const noexcept { return m_depth; }

    int appendTask(const LogTaskItem& task);
    bool updateTaskMetadata(int row, Application::Logging::TaskState state, double progress, const QString& currentMessage, const QString& taskName = QString());

    std::optional<LogTaskItem> taskSnapshot(int row) const;
    virtual std::shared_ptr<LogMessageListModel> taskMessagesModel(int row) const;
    virtual std::shared_ptr<LogTaskModel> taskSubTasksModel(int row) const;
    Q_INVOKABLE virtual int findRowByTaskId(quint64 taskId) const;

    Q_INVOKABLE virtual UI::ViewModels::LogMessageListModel* getTaskMessagesModel(int row) const;
    Q_INVOKABLE virtual UI::ViewModels::LogTaskModel* getTaskSubTasksModel(int row) const;

    Q_INVOKABLE virtual void clear();
    Q_INVOKABLE virtual void expandAll();
    Q_INVOKABLE virtual void collapseAll();
    Q_INVOKABLE virtual void toggleTaskExpanded(int index);
    Q_INVOKABLE virtual void setTaskExpanded(int index, bool expanded);

    // =========================================================================
    // Diagnostic & Clipboard Export API (Export-only; does NOT participate in UI rendering)
    // =========================================================================
    virtual QString exportToPlainText(int indentLevel = 0) const;

signals:
    void taskCountChanged();

protected:
    int m_depth = 0;
    bool m_autoScroll = true;
    QVector<LogTaskItem> m_tasks;
    QHash<quint64, int> m_taskIdToRow;
};

} // namespace UI::ViewModels
