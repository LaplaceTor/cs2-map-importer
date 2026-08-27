#pragma once

#include <QString>
#include <QtGlobal>

namespace Core::Logging {

/**
 * @brief Utility for managing log directory, filename generation, and path sanitization.
 */
class LogFileManager {
public:
    /**
     * @brief Gets the directory where logs are stored. Default: "<working directory>/logs".
     */
    static QString logsDirectory();

    /**
     * @brief Overrides the default log directory (primarily for testing or custom configuration).
     */
    static void setLogsDirectory(const QString& dir);

    /**
     * @brief Resets the log directory back to default.
     */
    static void resetLogsDirectory();

    /**
     * @brief Ensures the logs directory exists on the filesystem.
     * @return true if directory exists or was successfully created, false otherwise.
     */
    static bool ensureLogsDirectoryExists();

    /**
     * @brief Sanitizes a name for safe use as a filename on Windows.
     * Replaces illegal characters (\ / : * ? " < > | and control characters) with underscores,
     * trims whitespace, enforces length limit, and falls back to "task" if empty.
     */
    static QString sanitizeFileName(const QString& name);

    /**
     * @brief Generates a task log filename formatted as: <sanitized_task_name>_<yyyyMMdd_HHmmss_zzz>_<taskId>.log
     * or <sanitized_task_name>_<yyyyMMdd_HHmmss_zzz>.log if taskId is 0.
     */
    static QString generateTaskLogFileName(const QString& taskName, qint64 startTimestamp = 0, quint64 taskId = 0);

    /**
     * @brief Generates the full file path for a task log.
     */
    static QString generateTaskLogFilePath(const QString& taskName, qint64 startTimestamp = 0, quint64 taskId = 0);

    /**
     * @brief Generates an application log filename formatted as: application_<yyyyMMdd_HHmmss_zzz>.log.
     */
    static QString generateApplicationLogFileName(qint64 startupTimestamp = 0);

    /**
     * @brief Generates the full file path for the application log.
     */
    static QString generateApplicationLogFilePath(qint64 startupTimestamp = 0);

    /**
     * @brief Formats a timestamp into yyyyMMdd_HHmmss_zzz string.
     */
    static QString formatTimestamp(qint64 timestamp = 0);
};

} // namespace Core::Logging

