#pragma once

#include <QString>
#include <QList>
#include <cstdint>
#include <cstddef>

#include "LogEntry.h"

namespace Core::Logging {

class LogBlock {
public:
    explicit LogBlock(QString taskId = QString(), std::uint64_t blockIndex = 0);
    ~LogBlock() = default;

    // Move construct / assign allowed; copy disabled to avoid unintentional duplication of large log blocks
    LogBlock(const LogBlock&) = delete;
    LogBlock& operator=(const LogBlock&) = delete;
    LogBlock(LogBlock&&) noexcept = default;
    LogBlock& operator=(LogBlock&&) noexcept = default;

    const QString& taskId() const noexcept { return m_taskId; }
    std::uint64_t blockIndex() const noexcept { return m_blockIndex; }

    bool append(const LogEntry& entry);
    bool append(LogEntry&& entry);

    std::size_t entryCount() const noexcept;
    std::size_t size() const noexcept;

    void seal() noexcept;
    bool isSealed() const noexcept;

    const QList<LogEntry>& entries() const noexcept { return m_entries; }

private:
    QString m_taskId;
    std::uint64_t m_blockIndex = 0;
    QList<LogEntry> m_entries;
    std::size_t m_totalByteSize = 0;
    bool m_sealed = false;
};

} // namespace Core::Logging
