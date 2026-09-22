#include "TaskFileSink.h"
#include "ApplicationLogger.h"
#include "LogFileManager.h"
#include "LogSource.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTimeZone>

namespace Core::Logging {

TaskFileSink::TaskFileSink()
    : ILogSink()
{
}

TaskFileSink::~TaskFileSink()
{
    closeAll();
}

bool TaskFileSink::onTaskCreated(quint64 taskId, const QString& taskName, qint64 startTimestamp, const QString& logFilePath)
{
    QMutexLocker locker(&m_mutex);
    if (logFilePath.isEmpty()) {
        ApplicationLogger::error(QStringLiteral("TaskFileSink: Cannot create log file for task [%1] '%2' with empty path")
            .arg(QString::number(taskId), taskName));
        m_taskLogReady.insert(taskId, false);
        return false;
    }
    m_taskFilePaths.insert(taskId, logFilePath);
    const bool ok = ensureTaskFileOpenLocked(taskId, taskName, startTimestamp);
    m_taskLogReady.insert(taskId, ok);
    if (!ok) {
        ApplicationLogger::error(QStringLiteral("TaskFileSink: Failed to create or open log file for task [%1] '%2' at path '%3'")
            .arg(QString::number(taskId), taskName, logFilePath));
    }
    return ok;
}

bool TaskFileSink::isTaskFileOpen(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    if (m_taskFiles.contains(taskId)) {
        const auto handle = m_taskFiles.value(taskId);
        return handle && handle->file && handle->file->isOpen();
    }
    return false;
}

bool TaskFileSink::hasTaskLogFile(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_taskLogReady.value(taskId, false);
}

void TaskFileSink::onTaskTerminated(quint64 taskId, TaskState state)
{
    Q_UNUSED(state);
    closeTask(taskId);
}

bool TaskFileSink::ensureTaskFileOpenLocked(quint64 taskId, const QString& taskName, qint64 startTimestamp)
{
    Q_UNUSED(taskName);
    Q_UNUSED(startTimestamp);
    if (m_taskFiles.contains(taskId)) {
        auto handle = m_taskFiles.value(taskId);
        if (handle && handle->file && handle->file->isOpen()) {
            return true;
        }
    }

    if (!m_taskFilePaths.contains(taskId)) {
        m_taskLogReady.insert(taskId, false);
        return false;
    }

    const QString filePath = m_taskFilePaths.value(taskId);
    if (filePath.isEmpty()) {
        m_taskLogReady.insert(taskId, false);
        return false;
    }

    LogFileManager::ensureLogsDirectoryExists();

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            m_taskLogReady.insert(taskId, false);
            return false;
        }
    }

    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_taskLogReady.insert(taskId, false);
        return false;
    }

    if (file->size() == 0) {
        const QString startTimeStr = QDateTime::fromMSecsSinceEpoch(
            startTimestamp > 0 ? startTimestamp : QDateTime::currentMSecsSinceEpoch(),
            QTimeZone::systemTimeZone()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        const QString header = QStringLiteral("=== Task: %1 (ID: %2) | Started: %3 ===\n\n")
            .arg(taskName.isEmpty() ? QStringLiteral("Task %1").arg(taskId) : taskName)
            .arg(taskId)
            .arg(startTimeStr);
        file->write(header.toUtf8());
    }

    auto handle = std::make_shared<TaskFileHandle>();
    handle->filePath = filePath;
    handle->file = std::move(file);

    m_taskFiles.insert(taskId, handle);
    m_taskLogReady.insert(taskId, true);
    m_lastTaskLogFilePath = filePath;
    return true;
}

bool TaskFileSink::writeBlock(const LogBlock& block, const QString& taskName)
{
    QMutexLocker locker(&m_mutex);
    if (!block.isSealed()) {
        return false;
    }

    const quint64 taskId = block.taskId();
    const auto& entries = block.entries();

    qint64 startTimestamp = 0;
    if (!entries.isEmpty()) {
        startTimestamp = entries.first().timestamp;
    }

    if (!ensureTaskFileOpenLocked(taskId, taskName, startTimestamp)) {
        return false;
    }

    auto handle = m_taskFiles.value(taskId);
    if (!handle || !handle->file || !handle->file->isOpen()) {
        return false;
    }

    const quint64 blockIndex = block.blockIndex();
    QString blockBuffer;

    for (const auto& entry : entries) {
        if (entry.taskId != taskId) {
            return false;
        }

        blockBuffer += formatEntry(entry.timestamp, taskId, taskName, blockIndex,
                                   entry.sequence, entry.source, entry.level, entry.message);
        blockBuffer += QLatin1Char('\n');
    }

    const QByteArray utf8Data = blockBuffer.toUtf8();
    if (utf8Data.isEmpty()) {
        return true;
    }

    const qint64 bytesWritten = handle->file->write(utf8Data);
    if (bytesWritten != utf8Data.size() || handle->file->error() != QFile::NoError) {
        return false;
    }

    return true;
}

bool TaskFileSink::flush()
{
    QMutexLocker locker(&m_mutex);
    bool allSuccess = true;
    for (auto it = m_taskFiles.begin(); it != m_taskFiles.end(); ++it) {
        auto handle = it.value();
        if (handle && handle->file && handle->file->isOpen()) {
            if (!handle->file->flush() || handle->file->error() != QFile::NoError) {
                allSuccess = false;
            }
        }
    }
    return allSuccess;
}

void TaskFileSink::closeTask(quint64 taskId)
{
    QMutexLocker locker(&m_mutex);
    if (m_taskFiles.contains(taskId)) {
        auto handle = m_taskFiles.value(taskId);
        if (handle && handle->file && handle->file->isOpen()) {
            handle->file->flush();
            handle->file->close();
        }
        m_taskFiles.remove(taskId);
    }
}

void TaskFileSink::closeAll()
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_taskFiles.begin(); it != m_taskFiles.end(); ++it) {
        auto handle = it.value();
        if (handle && handle->file && handle->file->isOpen()) {
            handle->file->flush();
            handle->file->close();
        }
    }
    m_taskFiles.clear();
}

QString TaskFileSink::taskLogFilePath(quint64 taskId) const
{
    QMutexLocker locker(&m_mutex);
    return m_taskFilePaths.value(taskId);
}

QString TaskFileSink::lastTaskLogFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastTaskLogFilePath;
}

QString TaskFileSink::formatEntry(qint64 timestamp, quint64 taskId, const QString& taskName,
                                  quint64 blockIndex, quint64 sequence, LogSource source,
                                  LogLevel level, const QString& message)
{
    Q_UNUSED(timestamp);
    Q_UNUSED(taskId);
    Q_UNUSED(taskName);
    Q_UNUSED(blockIndex);
    Q_UNUSED(sequence);

    if (source == LogSource::ExternalTool) {
        return message;
    }

    const QString levelStr = logLevelToString(level);
    return QStringLiteral("[%1] %2").arg(levelStr, message);
}

} // namespace Core::Logging
