#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <memory>

#include "Core/Logging/LogBlock.h"
#include "Core/Logging/LogEntry.h"
#include "Core/Logging/LogLevel.h"
#include "Core/Logging/TaskState.h"
#include "UI/ViewModels/LogMessageListModel.h"
#include "UI/ViewModels/LogTaskModel.h"

namespace UI::ViewModels {

struct TaskRegistryEntry {
    quint64 taskId = 0;
    quint64 parentTaskId = 0;
    int depth = 0;
    LogTaskModel* owningModel = nullptr;
    std::shared_ptr<LogMessageListModel> messagesModel;
    std::shared_ptr<LogTaskModel> subTasksModel;
};

/**
 * @brief Top-level ViewModel for logs, acting as the root LogTaskModel.
 * Note: Model mutations execute strictly on the owning UI thread (guaranteed by LogViewModelSinkAdapter).
 */
class LogViewModel : public LogTaskModel {
    Q_OBJECT

    Q_PROPERTY(int totalMessageCount READ totalMessageCount NOTIFY totalMessageCountChanged)
    Q_PROPERTY(bool autoScroll READ autoScroll WRITE setAutoScroll NOTIFY autoScrollChanged)

public:
    explicit LogViewModel(QObject* parent = nullptr);
    ~LogViewModel() override;

    void registerWithLogManager();
    void unregisterFromLogManager();

    // Property getters
    int totalMessageCount() const;
    bool autoScroll() const noexcept { return m_autoScroll; }
    void setAutoScroll(bool enabled);

public slots:
    /**
     * @brief Resets only the presentation state of the log view.
     * Does not affect LogManager tasks, task log files, sinks, or workflow execution.
     */
    Q_INVOKABLE void resetView();
    Q_INVOKABLE bool openLogFile();
    QString getFullLogText() const;
    QString activeTaskLogFilePath() const;
    QString lastTaskLogFilePath() const;
    void appendLog(const QString& message, int level = 0);
    void processIncomingBlock(const Core::Logging::LogBlock& block, const QString& taskName);

signals:
    void totalMessageCountChanged();
    void autoScrollChanged();

private:
    TaskRegistryEntry ensureTaskRegistered(quint64 taskId, const QString& taskName);

    QHash<quint64, TaskRegistryEntry> m_taskRegistry;
    QHash<quint64, QString> m_taskLogFiles;
    quint64 m_activeTaskId = 0;
    quint64 m_lastTaskId = 0;
    int m_totalMessages = 0;
    bool m_autoScroll = true;
    quint64 m_registeredSinkId = 0;
};

} // namespace UI::ViewModels
