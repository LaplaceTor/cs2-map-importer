#include "TaskFileSink.h"
#include "LogFileManager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTimeZone>

namespace Core::Logging {

TaskFileSink::TaskFileSink() = default;

TaskFileSink::~TaskFileSink()
{
    closeAll();
}

bool TaskFileSink::ensureTaskFileOpenLocked(quint64 taskId, const QString& taskName, qint64 startTimestamp)
{
    if (m_taskFiles.contains(taskId)) {
        auto handle = m_taskFiles.value(taskId);
        if (handle && handle->file && handle->file->isOpen()) {
            return true;
        }
    }

    LogFileManager::ensureLogsDirectoryExists();

    QString filePath;
    if (m_taskFilePaths.contains(taskId)) {
        filePath = m_taskFilePaths.value(taskId);
    } else {
        const qint64 time = (startTimestamp > 0) ? startTimestamp : QDateTime::currentMSecsSinceEpoch();
        filePath = LogFileManager::generateTaskLogFilePath(taskName, time);
        m_taskFilePaths.insert(taskId, filePath);
    }

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

    auto stream = std::make_unique<QTextStream>(file.get());

    auto handle = std::make_shared<TaskFileHandle>();
    handle->filePath = filePath;
    handle->file = std::move(file);
    handle->stream = std::move(stream);

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

    handle->file->flush();
    return true;
}

bool TaskFileSink::flush()
{
    QMutexLocker locker(&m_mutex);
    bool allSuccess = true;
    for (auto it = m_taskFiles.constBegin(); it != m_taskFiles.constEnd(); ++it) {
        const auto& handle = it.value();
        if (handle) {
            if (handle->stream) {
                handle->stream->flush();
            }
            if (handle->file && handle->file->isOpen()) {
                if (!handle->file->flush() || handle->file->error() != QFile::NoError) {
                    allSuccess = false;
                }
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
        if (handle) {
            if (handle->stream) {
                handle->stream->flush();
            }
            if (handle->file && handle->file->isOpen()) {
                handle->file->flush();
                handle->file->close();
            }
        }
        m_taskFiles.remove(taskId);
    }
}

void TaskFileSink::closeAll()
{
    QMutexLocker locker(&m_mutex);
    for (auto it = m_taskFiles.constBegin(); it != m_taskFiles.constEnd(); ++it) {
        const auto& handle = it.value();
        if (handle) {
            if (handle->stream) {
                handle->stream->flush();
            }
            if (handle->file && handle->file->isOpen()) {
                handle->file->flush();
                handle->file->close();
            }
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

void TaskFileSink::registerTaskPath(quint64 taskId, const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_taskFilePaths.insert(taskId, path);
    m_lastTaskLogFilePath = path;
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
