#pragma once

#include <QString>
#include <QtGlobal>
#include "LogLevel.h"

namespace Core::Logging {

struct LogEntry {
    quint64 sequence = 0;
    qint64 timestamp = 0; // UTC Unix epoch milliseconds
    LogLevel level = LogLevel::Info;
    QString message;

    // Estimated memory footprint in bytes for size management / flush decisions
    qsizetype estimatedByteSize() const noexcept
    {
        return static_cast<qsizetype>(sizeof(LogEntry)) + message.capacity() * static_cast<qsizetype>(sizeof(QChar));
    }
};

} // namespace Core::Logging
