#pragma once

#include "LogEntry.h"

#include <QVector>

namespace Core::Logging {

class LogBlock {
public:
    LogBlock() = default;

    bool append(const LogEntry& entry);
    qsizetype entryCount() const;
    qsizetype size() const;
    void seal();
    bool isSealed() const;

    const QVector<LogEntry>& entries() const;

private:
    QVector<LogEntry> m_entries;
    qsizetype m_sizeBytes{0};
    bool m_sealed{false};
};

} // namespace Core::Logging
