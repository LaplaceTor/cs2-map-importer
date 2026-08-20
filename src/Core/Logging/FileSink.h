#pragma once

#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>
#include <memory>

#include "ILogSink.h"

namespace Core::Logging {

/**
 * @brief Log sink that writes sealed log blocks to a file using Qt file APIs.
 */
class FileSink : public ILogSink {
public:
    explicit FileSink(const QString& filePath = QString());
    ~FileSink() override;

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    /**
     * @brief Opens the log file at the specified path.
     * @param filePath Path to the log file.
     * @return true if opened successfully, false otherwise.
     */
    bool open(const QString& filePath);

    /**
     * @brief Closes the underlying log file.
     */
    void close();

    /**
     * @brief Checks if the log file is currently open.
     */
    bool isOpen() const;

    /**
     * @brief Gets the path of the current log file.
     */
    QString filePath() const;

    /**
     * @brief Writes a sealed LogBlock to the log file safely.
     */
    void writeBlock(const LogBlock& block, const QString& taskName) override;

    /**
     * @brief Flushes the file stream.
     */
    void flush() override;

    /**
     * @brief Formats a single log entry into a string line.
     */
    static QString formatEntry(qint64 timestamp, quint64 taskId, const QString& taskName, LogLevel level, const QString& message);

private:
    mutable QMutex m_mutex;
    QFile m_file;
    QTextStream m_stream;
};

} // namespace Core::Logging
