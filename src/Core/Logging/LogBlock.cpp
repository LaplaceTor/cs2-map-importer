#include "LogBlock.h"

namespace Core::Logging {

bool LogBlock::append(const LogEntry& entry)
{
    if (m_sealed) {
        return false;
    }

    m_entries.append(entry);
    m_sizeBytes += entry.estimatedSize();
    return true;
}

qsizetype LogBlock::entryCount() const
{
    return m_entries.size();
}

qsizetype LogBlock::size() const
{
    return m_sizeBytes;
}

void LogBlock::seal()
{
    m_sealed = true;
}

bool LogBlock::isSealed() const
{
    return m_sealed;
}

const QVector<LogEntry>& LogBlock::entries() const
{
    return m_entries;
}

} // namespace Core::Logging
