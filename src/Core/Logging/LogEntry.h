#pragma once

#include <QString>
#include <cstdint>
#include "LogLevel.h"

namespace Core::Logging {

struct LogEntry {
    std::uint64_t sequence = 0;
    std::int64_t timestamp = 0; // Epoch milliseconds or microseconds timestamp
    LogLevel level = LogLevel::Info;
    QString taskId;
    QString message;

    // Estimated memory footprint in bytes for size management / flush decisions
    std::size_t estimatedByteSize() const noexcept
    {
        // Fixed struct overhead + string buffers capacity/length estimation in bytes
        return sizeof(LogEntry)
            + static_cast<std::size_t>(taskId.capacity() * static_cast<int>(sizeof(char16_t)))
            + static_cast<std::size_t>(message.capacity() * static_cast<int>(sizeof(char16_t)));
    }
};

} // namespace Core::Logging
