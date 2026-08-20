#pragma once

#include <QVector>
#include <QtGlobal>
#include <cstddef>

#include "LogEntry.h"

namespace Core::Logging {

class LogBlock {
public:
    explicit LogBlock(quint64 taskId, quint64 blockIndex = 0);
    ~LogBlock() = default;

    // Move construct / assign allowed; copy disabled to avoid unintentional duplication of large log blocks
    LogBlock(const LogBlock&) = delete;
    LogBlock& operator=(const LogBlock&) = delete;
    LogBlock(LogBlock&&) noexcept = default;
    LogBlock& operator=(LogBlock&&) noexcept = default;

    quint64 taskId() const noexcept { return m_taskId; }
    quint64 blockIndex() const noexcept { return m_blockIndex; }

    bool append(const LogEntry& entry);
    bool append(LogEntry&& entry);

    std::size_t entryCount() const noexcept;
    std::size_t size() const noexcept;

    void seal() noexcept;
    bool isSealed() const noexcept;

    const QVector<LogEntry>& entries() const noexcept { return m_entries; }

private:
    quint64 m_taskId = 0;
    quint64 m_blockIndex = 0; // Sequential index of this block within the Task log
    QVector<LogEntry> m_entries;
    std::size_t m_totalByteSize = 0;
    bool m_sealed = false;
};

} // namespace Core::Logging
