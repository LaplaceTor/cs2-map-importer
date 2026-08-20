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

bool FileSink::writeBlock(const LogBlock& block, const QString& taskName)
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }

    m_stream.flush();
    if (m_stream.status() != QTextStream::Ok || m_file.error() != QFile::NoError) {
        return false;
    }

    qint64 originalPos = m_file.size();

    quint64 taskId = block.taskId();
    quint64 blockIndex = block.blockIndex();
    const auto& entries = block.entries();

    QString blockBuffer;
    for (const auto& entry : entries) {
        blockBuffer += formatEntry(entry.timestamp, taskId, taskName, blockIndex, entry.sequence, entry.level, entry.message);
        blockBuffer += QLatin1Char('\n');
    }

    QByteArray utf8Data = blockBuffer.toUtf8();
    if (utf8Data.isEmpty()) {
        return true;
    }

    qint64 bytesWritten = m_file.write(utf8Data);
    if (bytesWritten != utf8Data.size() || m_file.error() != QFile::NoError) {
        m_file.flush();
        m_file.seek(originalPos);
        m_file.resize(originalPos);
        return false;
    }

    return true;
}

bool FileSink::flush()
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }
    m_stream.flush();
    bool fileFlushOk = m_file.flush();
    return (m_stream.status() == QTextStream::Ok && fileFlushOk && m_file.error() == QFile::NoError);
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
