#include "FileSink.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTimeZone>

namespace Core::Logging {

FileSink::FileSink(const QString& filePath)
{
    if (!filePath.isEmpty()) {
        open(filePath);
    }
}

FileSink::~FileSink()
{
    close();
}

bool FileSink::open(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }

    if (filePath.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }

    m_stream.setDevice(&m_file);
    return true;
}

void FileSink::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

bool FileSink::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.isOpen();
}

QString FileSink::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.fileName();
}

void FileSink::writeBlock(const LogBlock& block, const QString& taskName)
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        return;
    }

    quint64 taskId = block.taskId();
    quint64 blockIndex = block.blockIndex();
    const auto& entries = block.entries();

    for (const auto& entry : entries) {
        QString line = formatEntry(entry.timestamp, taskId, taskName, blockIndex, entry.sequence, entry.level, entry.message);
        m_stream << line << "\n";
    }
    // Stream buffering is maintained for I/O performance. Explicit flush() handles file flushing.
}

void FileSink::flush()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
    }
}

QString FileSink::formatEntry(qint64 timestamp, quint64 taskId, const QString& taskName, quint64 blockIndex, quint64 sequence, LogLevel level, const QString& message)
{
    QString timeStr = QDateTime::fromMSecsSinceEpoch(timestamp, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString levelStr = logLevelToString(level);

    QString namePart = taskName.isEmpty() ? QString("Task %1").arg(taskId) : QString("Task %1 - %2").arg(taskId).arg(taskName);

    return QString("[%1] [%2] [Block %3] [Seq %4] [%5] %6")
        .arg(timeStr)
        .arg(namePart)
        .arg(blockIndex)
        .arg(sequence)
        .arg(levelStr)
        .arg(message);
}

} // namespace Core::Logging
