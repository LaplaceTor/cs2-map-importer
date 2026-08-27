#include "ApplicationLogger.h"
#include "ApplicationLogSink.h"
#include "LogFileManager.h"

#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Core::Logging {

namespace {

QMutex s_appLoggerMutex;
std::unique_ptr<ApplicationLogSink> s_appLogSink;
QString s_appLogFilePath;
bool s_initialized = false;
QtMessageHandler s_previousQtMessageHandler = nullptr;
bool s_qtMessageHandlerInstalled = false;

void qtMessageOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static thread_local bool s_inHandler = false;
    if (s_inHandler) {
        return;
    }
    s_inHandler = true;

    LogLevel level = LogLevel::Info;
    switch (type) {
    case QtDebugMsg:
        level = LogLevel::Debug;
        break;
    case QtInfoMsg:
        level = LogLevel::Info;
        break;
    case QtWarningMsg:
        level = LogLevel::Warning;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = LogLevel::Error;
        break;
    }

    ApplicationLogger::log(level, msg);

    s_inHandler = false;
}

} // namespace

void ApplicationLogger::installQtMessageHandler()
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (!s_qtMessageHandlerInstalled) {
        s_previousQtMessageHandler = qInstallMessageHandler(qtMessageOutputHandler);
        s_qtMessageHandlerInstalled = true;
    }
}

void ApplicationLogger::uninstallQtMessageHandler()
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_qtMessageHandlerInstalled) {
        qInstallMessageHandler(s_previousQtMessageHandler);
        s_previousQtMessageHandler = nullptr;
        s_qtMessageHandlerInstalled = false;
    }
}

bool ApplicationLogger::initialize(qint64 startupTimestamp, const QString& customLogFilePath)
{
    QMutexLocker locker(&s_appLoggerMutex);
    if (s_initialized && s_appLogSink && s_appLogSink->isOpen()) {
        return true;
    }

    LogFileManager::ensureLogsDirectoryExists();

    const qint64 time = (startupTimestamp > 0) ? startupTimestamp : QDateTime::currentMSecsSinceEpoch();
    const QString targetPath = !customLogFilePath.isEmpty()
        ? customLogFilePath
        : LogFileManager::generateApplicationLogFilePath(time);

    auto sink = std::make_unique<ApplicationLogSink>();
    if (!sink->open(targetPath)) {
        return false;
    }

    s_appLogFilePath = targetPath;
    s_appLogSink = std::move(sink);
    s_initialized = true;

    if (!s_qtMessageHandlerInstalled) {
        s_previousQtMessageHandler = qInstallMessageHandler(qtMessageOutputHandler);
        s_qtMessageHandlerInstalled = true;
    }

    s_appLogSink->writeEntry(LogLevel::Info, QStringLiteral("Application log initialized"), time);
    return true;
}

bool ApplicationLogger::initialize(const QString& customLogFilePath)
{
    return initialize(0, customLogFilePath);
}

void ApplicationLogger::shutdown()
{
    uninstallQtMessageHandler();

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

#ifdef Q_OS_WIN
    const QString debugMsg = QStringLiteral("[%1] [Application] %2\n").arg(logLevelToString(level), message);
    OutputDebugStringW(reinterpret_cast<LPCWSTR>(debugMsg.utf16()));
#endif
}

} // namespace Core::Logging
