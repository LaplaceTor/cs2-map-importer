#include "LogBlock.h"

namespace Core::Logging {

LogBlock::LogBlock(quint64 taskId, quint64 blockIndex)
    : m_taskId(taskId)
    , m_blockIndex(blockIndex)
{
}

LogBlock::LogBlock(const LogBlock& other)
    : m_taskId(other.m_taskId)
    , m_blockIndex(other.m_blockIndex)
    , m_entries(other.m_entries)
    , m_totalByteSize(other.m_totalByteSize)
    , m_sealed(other.m_sealed)
{
}

LogBlock& LogBlock::operator=(const LogBlock& other)
{
    if (this != &other) {
        m_taskId = other.m_taskId;
        m_blockIndex = other.m_blockIndex;
        m_entries = other.m_entries;
        m_totalByteSize = other.m_totalByteSize;
        m_sealed = other.m_sealed;
    }
    return *this;
}

LogBlock LogBlock::clone() const
{
    return LogBlock(*this);
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

qsizetype LogBlock::entryCount() const noexcept
{
    return m_entries.size();
}

qsizetype LogBlock::size() const noexcept
{
    return m_totalByteSize;
}

void LogBlock::seal() noexcept
{
    m_sealed = true;
}

bool LogBlock::isSealed() const noexcept
{
    return m_sealed;
}

} // namespace Core::Logging
