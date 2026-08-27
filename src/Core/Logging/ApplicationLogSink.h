#pragma once

#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>
#include "LogLevel.h"

namespace Core::Logging {

/**
 * @brief Dedicated log sink for writing application internal lifecycle logs.
 * Thread-safe and completely isolated from task/external tool logs.
 */
class ApplicationLogSink {
public:
    explicit ApplicationLogSink(const QString& filePath = QString());
    ~ApplicationLogSink();

    ApplicationLogSink(const ApplicationLogSink&) = delete;
    ApplicationLogSink& operator=(const ApplicationLogSink&) = delete;
    ApplicationLogSink(ApplicationLogSink&&) = delete;
    ApplicationLogSink& operator=(ApplicationLogSink&&) = delete;

    /**
     * @brief Opens or creates the application log file at the specified path.
     */
    bool open(const QString& filePath);

    /**
     * @brief Flushes and closes the application log file.
     */
    void close();

    /**
     * @brief Checks if the log file is currently open.
     */
    bool isOpen() const;

    /**
     * @brief Returns the path to the currently open log file.
     */
    QString filePath() const;

    /**
     * @brief Writes a single application log message.
     */
    bool writeEntry(LogLevel level, const QString& message, qint64 timestamp = 0);

    /**
     * @brief Flushes any buffered content to disk.
     */
    bool flush();

    /**
     * @brief Formats an application log line.
     * Format: [2026-08-27 21:28:01.012] [Application] [INFO] message
     */
    static QString formatEntry(qint64 timestamp, LogLevel level, const QString& message);

private:
    mutable QMutex m_mutex;
    QFile m_file;
    QTextStream m_stream;
};

} // namespace Core::Logging

