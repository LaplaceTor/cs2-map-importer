#include "LogBlock.h"

namespace Core::Logging {

LogBlock::LogBlock(QString taskId, std::uint64_t blockIndex)
    : m_taskId(std::move(taskId))
    , m_blockIndex(blockIndex)
{
}

bool LogBlock::append(const LogEntry& entry)
{
    if (m_sealed) {
        return false;
    }

    m_totalByteSize += entry.estimatedByteSize();
    m_entries.append(entry);
    return true;
}

bool LogBlock::append(LogEntry&& entry)
{
    if (m_sealed) {
        return false;
    }

    m_totalByteSize += entry.estimatedByteSize();
    m_entries.append(std::move(entry));
    return true;
}

std::size_t LogBlock::entryCount() const noexcept
{
    return static_cast<std::size_t>(m_entries.size());
}

std::size_t LogBlock::size() const noexcept
{
    return m_totalByteSize;
}

void LogBlock::seal() noexcept
{
    m_sealed = true;
    m_entries.squeeze();
}

bool LogBlock::isSealed() const noexcept
{
    return m_sealed;
}

} // namespace Core::Logging
