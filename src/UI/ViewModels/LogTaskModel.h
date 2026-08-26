#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>
#include <memory>

#include "Core/Logging/LogLevel.h"
#include "Core/Logging/TaskState.h"
#include "UI/ViewModels/LogMessageListModel.h"

namespace UI::ViewModels {

class LogTaskModel;

struct LogTaskItem {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    int depth = 0;
    QString taskName;
    Core::Logging::TaskState state = Core::Logging::TaskState::Pending;
    double progress = 0.0;
    QString currentMessage;
    bool expanded = true;
    std::shared_ptr<LogMessageListModel> messagesModel;
    std::shared_ptr<LogTaskModel> subTasksModel;
};

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

    int taskCount() const;
    int depth() const noexcept { return m_depth; }

    int appendTask(const LogTaskItem& task);
    bool updateTaskMetadata(int row, Core::Logging::TaskState state, double progress, const QString& currentMessage, const QString& taskName = QString());

    LogTaskItem* getTaskItem(int row);
    const LogTaskItem* getTaskItem(int row) const;
    int findRowByTaskId(quint64 taskId) const;

    Q_INVOKABLE UI::ViewModels::LogMessageListModel* getTaskMessagesModel(int row) const;
    Q_INVOKABLE UI::ViewModels::LogTaskModel* getTaskSubTasksModel(int row) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void toggleTaskExpanded(int index);
    Q_INVOKABLE void setTaskExpanded(int index, bool expanded);

    // =========================================================================
    // Diagnostic & Clipboard Export API (Export-only; does NOT participate in UI rendering)
    // =========================================================================
    QString exportToPlainText(int indentLevel = 0) const;

signals:
    void taskCountChanged();

protected:
    mutable QMutex m_mutex;
    int m_depth = 0;
    QVector<LogTaskItem> m_tasks;
    QHash<quint64, int> m_taskIdToRow;
};

} // namespace UI::ViewModels
