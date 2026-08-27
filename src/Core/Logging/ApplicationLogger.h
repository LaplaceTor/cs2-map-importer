#pragma once

#include <QString>
#include <memory>
#include "LogLevel.h"

namespace Core::Logging {

/**
 * @brief Global logger for internal application lifecycle, services, and diagnostic events.
 * Writes exclusively to application_<timestamp>.log and is completely decoupled from
 * workflow and external tool stdout/stderr streams.
 */
class ApplicationLogger {
public:
    /**
     * @brief Initializes the application logging subsystem.
     * @param startupTimestamp Unix epoch millisecond timestamp recorded at application entry.
     * @param customLogFilePath Optional explicit log file path (useful for testing).
     * @return true if initialized and log file opened successfully, false otherwise.
     */
    static bool initialize(qint64 startupTimestamp = 0, const QString& customLogFilePath = QString());

    /**
     * @brief Initializes the application logging subsystem with an explicit log file path.
     */
    static bool initialize(const QString& customLogFilePath);

    /**
     * @brief Flushes and closes the application log file.
     */
    static void shutdown();

    /**
     * @brief Checks if the application logger is currently active.
     */
    static bool isInitialized();

    /**
     * @brief Returns the path to the current application log file.
     */
    static QString logFilePath();

    /**
     * @brief Installs a global Qt message handler redirecting qDebug/qInfo/qWarning/qCritical to ApplicationLogger.
     * Automatically invoked by initialize().
     */
    static void installQtMessageHandler();

    /**
     * @brief Uninstalls the Qt message handler and restores default handling.
     * Automatically invoked by shutdown().
     */
    static void uninstallQtMessageHandler();

    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);
    static void log(LogLevel level, const QString& message);
};

} // namespace Core::Logging

