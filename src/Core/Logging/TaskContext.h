#pragma once

#include "LogBlock.h"
#include "LogLevel.h"
#include "TaskState.h"

#include <QMutex>
#include <QString>
#include <QVector>
#include <functional>

namespace Core::Logging {

class TaskContext {
public:
    using SequenceGenerator = std::function<quint64()>;

    explicit TaskContext(quint64 taskId, QString taskName, SequenceGenerator sequenceGenerator = nullptr);

    quint64 taskId() const;
    QString taskName() const;
    TaskState state() const;
    double progress() const;
    QString currentMessage() const;
    bool isFinished() const;

    void setRotationThresholds(qsizetype maxEntries, qsizetype maxBytes);
    qsizetype maxEntries() const;
    qsizetype maxBytes() const;

    void setProgress(double progress);
    void setCurrentMessage(const QString& message);
    void setState(TaskState state);

    void complete(const QString& message = QString());
    void fail(const QString& message = QString());
    void cancel(const QString& message = QString());

    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);
    void critical(const QString& message);

    void log(LogLevel level, const QString& message);

    QVector<LogBlock> blocks() const;
    QVector<LogEntry> allEntries() const;
    qsizetype totalEntryCount() const;

private:
    quint64 nextSequence();

    quint64 m_taskId{0};
    QString m_taskName;
    TaskState m_state{TaskState::Pending};
    double m_progress{0.0};
    QString m_currentMessage;

    SequenceGenerator m_sequenceGenerator;
    quint64 m_fallbackSequence{1};

    qsizetype m_maxEntries{4096};
    qsizetype m_maxBytes{256 * 1024}; // 256 KiB

    QVector<LogBlock> m_blocks;

    mutable QMutex m_mutex;
};

} // namespace Core::Logging
