#pragma once

#include <QVector>
#include <QtGlobal>

#include "LogEntry.h"

namespace Core::Logging {

class LogBlock {
public:
    explicit LogBlock(quint64 taskId, quint64 blockIndex = 0);
    ~LogBlock() = default;

    LogBlock(const LogBlock&);
    LogBlock& operator=(const LogBlock&);
    LogBlock(LogBlock&&) noexcept = default;
    LogBlock& operator=(LogBlock&&) noexcept = default;

    LogBlock clone() const;

    quint64 taskId() const noexcept { return m_taskId; }
    quint64 blockIndex() const noexcept { return m_blockIndex; }

    bool append(const LogEntry& entry);
    bool append(LogEntry&& entry);

    qsizetype entryCount() const noexcept;
    qsizetype size() const noexcept;

    void seal() noexcept;
    bool isSealed() const noexcept;

    const QVector<LogEntry>& entries() const noexcept { return m_entries; }

private:
    quint64 m_taskId = 0;
    quint64 m_blockIndex = 0; // Sequential index of this block within the Task log
    QVector<LogEntry> m_entries;
    qsizetype m_totalByteSize = 0;
    bool m_sealed = false;
};

} // namespace Core::Logging
