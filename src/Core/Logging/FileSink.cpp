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
    if (!m_file.isOpen() || !block.isSealed()) {
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
        if (entry.taskId != taskId) {
            return false;
        }

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
        m_stream.seek(originalPos);
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
    Q_UNUSED(timestamp);
    Q_UNUSED(taskId);
    Q_UNUSED(taskName);
    Q_UNUSED(blockIndex);
    Q_UNUSED(sequence);

    const QString levelStr = logLevelToString(level);
    return QStringLiteral("[%1] %2").arg(levelStr, message);
}

} // namespace Core::Logging
