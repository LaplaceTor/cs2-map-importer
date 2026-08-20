#pragma once

#include <QString>
#include <QChar>
#include <QtGlobal>
#include <cstddef>
#include "LogLevel.h"

namespace Core::Logging {

struct LogEntry {
    quint64 sequence = 0;
    qint64 timestamp = 0; // UTC Unix epoch milliseconds
    LogLevel level = LogLevel::Info;
    QString message;

    // Estimated memory footprint in bytes for size management / flush decisions
    std::size_t estimatedByteSize() const noexcept
    {
        return sizeof(LogEntry) + static_cast<std::size_t>(message.capacity() * sizeof(QChar));
    }
};

} // namespace Core::Logging
