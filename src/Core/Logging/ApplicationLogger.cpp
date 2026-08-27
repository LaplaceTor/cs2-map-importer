#include "ApplicationLogger.h"
#include "ApplicationLogSink.h"
#include "LogFileManager.h"

#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>

namespace Core::Logging {

namespace {

QMutex s_appLoggerMutex;
std::unique_ptr<ApplicationLogSink> s_appLogSink;
QString s_appLogFilePath;
bool s_initialized = false;

} // namespace

bool ApplicationLogger::initialize(const QString& customLogFilePath)
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_initialized && s_appLogSink && s_appLogSink->isOpen()) {
        return true;
    }

    LogFileManager::ensureLogsDirectoryExists();

    const QString targetPath = !customLogFilePath.isEmpty()
        ? customLogFilePath
        : LogFileManager::generateApplicationLogFilePath(QDateTime::currentMSecsSinceEpoch());

    auto sink = std::make_unique<ApplicationLogSink>();
    if (!sink->open(targetPath)) {
        qWarning().noquote() << QStringLiteral("[ApplicationLogger] Failed to open log file: %1").arg(targetPath);
        return false;
    }

    s_appLogFilePath = targetPath;
    s_appLogSink = std::move(sink);
    s_initialized = true;

    s_appLogSink->writeEntry(LogLevel::Info, QStringLiteral("Application log initialized"));
    return true;
}

void ApplicationLogger::shutdown()
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_appLogSink && s_appLogSink->isOpen()) {
        s_appLogSink->writeEntry(LogLevel::Info, QStringLiteral("Application log shutdown"));
        s_appLogSink->flush();
        s_appLogSink->close();
    }
    s_appLogSink.reset();
    s_initialized = false;
}

bool ApplicationLogger::isInitialized()
{
    QMutexLocker locker(&s_appLoggerMutex);
    return s_initialized && s_appLogSink && s_appLogSink->isOpen();
}

QString ApplicationLogger::logFilePath()
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_appLogSink && s_appLogSink->isOpen()) {
        return s_appLogSink->filePath();
    }
    return s_appLogFilePath;
}

void ApplicationLogger::debug(const QString& message)
{
    log(LogLevel::Debug, message);
}

void ApplicationLogger::info(const QString& message)
{
    log(LogLevel::Info, message);
}

void ApplicationLogger::warning(const QString& message)
{
    log(LogLevel::Warning, message);
}

void ApplicationLogger::error(const QString& message)
{
    log(LogLevel::Error, message);
}

void ApplicationLogger::log(LogLevel level, const QString& message)
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_appLogSink && s_appLogSink->isOpen()) {
        s_appLogSink->writeEntry(level, message);
    }

    // Mirror to standard Qt logging for developer console visibility
    switch (level) {
    case LogLevel::Debug:
        qDebug().noquote() << message;
        break;
    case LogLevel::Info:
        qInfo().noquote() << message;
        break;
    case LogLevel::Warning:
        qWarning().noquote() << message;
        break;
    case LogLevel::Error:
    case LogLevel::Critical:
        qCritical().noquote() << message;
        break;
    }
}

} // namespace Core::Logging

