#include "LogFileManager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>

namespace Core::Logging {

namespace {

QMutex s_dirMutex;
QString s_customLogsDirectory;

} // namespace

QString LogFileManager::logsDirectory()
{
    QMutexLocker locker(&s_dirMutex);
    if (!s_customLogsDirectory.isEmpty()) {
        return s_customLogsDirectory;
    }
    return QDir::current().filePath(QStringLiteral("logs"));
}

void LogFileManager::setLogsDirectory(const QString& dir)
{
    QMutexLocker locker(&s_dirMutex);
    s_customLogsDirectory = dir;
}

void LogFileManager::resetLogsDirectory()
{
    QMutexLocker locker(&s_dirMutex);
    s_customLogsDirectory.clear();
}

bool LogFileManager::ensureLogsDirectoryExists()
{
    const QString dirPath = logsDirectory();
    QDir dir(dirPath);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(QStringLiteral("."));
}

QString LogFileManager::sanitizeFileName(const QString& name)
{
    QString sanitized = name.trimmed();
    if (sanitized.isEmpty()) {
        return QStringLiteral("task");
    }

    // Replace invalid Windows characters and control characters with '_'
    for (int i = 0; i < sanitized.length(); ++i) {
        const QChar c = sanitized.at(i);
        if (c < QChar(32) || c == QLatin1Char('\\') || c == QLatin1Char('/') ||
            c == QLatin1Char(':') || c == QLatin1Char('*') || c == QLatin1Char('?') ||
            c == QLatin1Char('"') || c == QLatin1Char('<') || c == QLatin1Char('>') ||
            c == QLatin1Char('|') || c.isSpace()) {
            sanitized[i] = QLatin1Char('_');
        }
    }

    // Collapse multiple consecutive underscores into a single underscore
    static const QRegularExpression multiUnderscore(QStringLiteral("_{2,}"));
    sanitized.replace(multiUnderscore, QStringLiteral("_"));

    // Trim leading/trailing underscores and periods
    while (!sanitized.isEmpty() && (sanitized.startsWith(QLatin1Char('_')) || sanitized.startsWith(QLatin1Char('.')))) {
        sanitized.remove(0, 1);
    }
    while (!sanitized.isEmpty() && (sanitized.endsWith(QLatin1Char('_')) || sanitized.endsWith(QLatin1Char('.')))) {
        sanitized.chop(1);
    }

    // Limit maximum length to 64 characters
    constexpr int maxLen = 64;
    if (sanitized.length() > maxLen) {
        sanitized.truncate(maxLen);
        while (!sanitized.isEmpty() && (sanitized.endsWith(QLatin1Char('_')) || sanitized.endsWith(QLatin1Char('.')))) {
            sanitized.chop(1);
        }
    }

    if (sanitized.isEmpty()) {
        return QStringLiteral("task");
    }

    return sanitized;
}

QString LogFileManager::formatTimestamp(qint64 timestamp)
{
    QDateTime dt = (timestamp > 0)
        ? QDateTime::fromMSecsSinceEpoch(timestamp, QTimeZone::utc())
        : QDateTime::currentDateTimeUtc();
    return dt.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
}

QString LogFileManager::generateTaskLogFileName(const QString& taskName, qint64 startTimestamp, quint64 taskId)
{
    const QString safeName = sanitizeFileName(taskName);
    const QString timeStr = formatTimestamp(startTimestamp);
    if (taskId > 0) {
        return QStringLiteral("%1_%2_%3.log").arg(safeName, timeStr, QString::number(taskId));
    }
    return QStringLiteral("%1_%2.log").arg(safeName, timeStr);
}

QString LogFileManager::generateTaskLogFilePath(const QString& taskName, qint64 startTimestamp, quint64 taskId)
{
    const QString fileName = generateTaskLogFileName(taskName, startTimestamp, taskId);
    return QDir(logsDirectory()).filePath(fileName);
}

QString LogFileManager::generateApplicationLogFileName(qint64 startupTimestamp)
{
    const QString timeStr = formatTimestamp(startupTimestamp);
    return QStringLiteral("application_%1.log").arg(timeStr);
}

QString LogFileManager::generateApplicationLogFilePath(qint64 startupTimestamp)
{
    const QString fileName = generateApplicationLogFileName(startupTimestamp);
    return QDir(logsDirectory()).filePath(fileName);
}

} // namespace Core::Logging
