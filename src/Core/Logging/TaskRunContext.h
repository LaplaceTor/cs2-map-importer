#pragma once

#include <QString>
#include <QtGlobal>

namespace Core::Logging {

/**
 * @brief Lightweight runtime context for task execution and path tracking.
 */
struct TaskRunContext {
    quint64 taskId = 0;
    QString taskName;
    qint64 startTimestamp = 0;
    QString logFilePath;
    bool logFileReady = true;
};

} // namespace Core::Logging

