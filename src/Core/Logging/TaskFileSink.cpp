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
        return false;
    }
    m_taskFilePaths.insert(taskId, logFilePath);
    const bool ok = ensureTaskFileOpenLocked(taskId, taskName, startTimestamp);
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
    const QString filePath = m_taskFilePaths.value(taskId);
    if (filePath.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(filePath);
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
        return false;
    }

    const QString filePath = m_taskFilePaths.value(taskId);
    if (filePath.isEmpty()) {
        return false;
    }

    LogFileManager::ensureLogsDirectoryExists();

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return false;
        }
    }

    auto file = std::make_unique<QFile>(filePath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }

    auto handle = std::make_shared<TaskFileHandle>();
    handle->filePath = filePath;
    handle->file = std::move(file);

    m_taskFiles.insert(taskId, handle);
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

        QString message = entry.message;
        message.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
        message.replace(QLatin1Char('\r'), QStringLiteral("\\r"));

        blockBuffer += formatEntry(entry.timestamp, taskId, taskName, blockIndex,
                                   entry.sequence, entry.source, entry.level, message);
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
    const QString timeStr = QDateTime::fromMSecsSinceEpoch(timestamp, QTimeZone::utc()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString levelStr = logLevelToString(level);
    const QString sourceStr = logSourceToString(source);
    const QString namePart = taskName.isEmpty() ? QStringLiteral("Task %1").arg(taskId) : QStringLiteral("Task %1 - %2").arg(taskId).arg(taskName);

    return QStringLiteral("[%1] [%2] [Block %3] [Seq %4] [Source: %5] [%6] %7")
        .arg(timeStr, namePart)
        .arg(blockIndex)
        .arg(sequence)
        .arg(sourceStr, levelStr, message);
}

} // namespace Core::Logging
