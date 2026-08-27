#include "Logger.h"
#include "ApplicationLogger.h"

namespace Core::Logging {

void Logger::debug(const QString& message)
{
    ApplicationLogger::debug(message);
}

void Logger::info(const QString& message)
{
    ApplicationLogger::info(message);
}

void Logger::warning(const QString& message)
{
    ApplicationLogger::warning(message);
}

void Logger::error(const QString& message)
{
    ApplicationLogger::error(message);
}

} // namespace Core::Logging
