#include "ApplicationLogSink.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTimeZone>

namespace Core::Logging {

ApplicationLogSink::ApplicationLogSink(const QString& filePath)
{
    if (!filePath.isEmpty()) {
        open(filePath);
    }
}

ApplicationLogSink::~ApplicationLogSink()
{
    close();
}

bool ApplicationLogSink::open(const QString& filePath)
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
        if (!dir.mkpath(QStringLiteral("."))) {
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

void ApplicationLogSink::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

bool ApplicationLogSink::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.isOpen();
}

QString ApplicationLogSink::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.fileName();
}

bool ApplicationLogSink::writeEntry(LogLevel level, const QString& message, qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }

    const qint64 entryTime = (timestamp > 0) ? timestamp : QDateTime::currentMSecsSinceEpoch();
    QString sanitizedMessage = message;
    sanitizedMessage.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    sanitizedMessage.replace(QLatin1Char('\r'), QStringLiteral("\\r"));

    const QString formatted = formatEntry(entryTime, level, sanitizedMessage);
    const QByteArray utf8Data = (formatted + QLatin1Char('\n')).toUtf8();

    const qint64 bytesWritten = m_file.write(utf8Data);
    if (bytesWritten != utf8Data.size() || m_file.error() != QFile::NoError) {
        return false;
    }

    m_file.flush();
    return true;
}

bool ApplicationLogSink::flush()
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        return false;
    }
    m_stream.flush();
    const bool fileFlushOk = m_file.flush();
    return (m_stream.status() == QTextStream::Ok && fileFlushOk && m_file.error() == QFile::NoError);
}

QString ApplicationLogSink::formatEntry(qint64 timestamp, LogLevel level, const QString& message)
{
    const QString timeStr = QDateTime::fromMSecsSinceEpoch(timestamp, QTimeZone::utc()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString levelStr = logLevelToString(level);

    return QStringLiteral("[%1] [Application] [%2] %3")
        .arg(timeStr, levelStr, message);
}

} // namespace Core::Logging

