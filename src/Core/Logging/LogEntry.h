#pragma once

#include "LogLevel.h"

#include <QDateTime>
#include <QString>

namespace Core::Logging {

struct LogEntry {
    quint64 sequence{0};
    QDateTime timestamp;
    LogLevel level{LogLevel::Info};
    quint64 taskId{0};
    QString message;

    qsizetype estimatedSize() const
    {
        return static_cast<qsizetype>(sizeof(LogEntry)) + (message.size() * static_cast<qsizetype>(sizeof(char16_t)));
    }
};

} // namespace Core::Logging
