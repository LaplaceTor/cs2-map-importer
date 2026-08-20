#pragma once

#include <QString>
#include "LogBlock.h"

namespace Core::Logging {

/**
 * @brief Abstract interface for logging sinks.
 * Sinks process sealed LogBlocks produced by tasks.
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;

    /**
     * @brief Writes a sealed LogBlock to the sink.
     * @param block The sealed LogBlock containing entries.
     * @param taskName The name of the task associated with the block.
     */
    virtual void writeBlock(const LogBlock& block, const QString& taskName) = 0;

    /**
     * @brief Flushes any buffered output in the sink.
     */
    virtual void flush() = 0;
};

} // namespace Core::Logging
