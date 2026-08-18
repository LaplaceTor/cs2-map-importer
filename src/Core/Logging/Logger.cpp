#include "Logger.h"

#include <QDebug>

namespace Core::Logging {

void Logger::debug(const QString& message)
{
    qDebug().noquote() << message;
}

void Logger::info(const QString& message)
{
    qInfo().noquote() << message;
}

void Logger::warning(const QString& message)
{
    qWarning().noquote() << message;
}

void Logger::error(const QString& message)
{
    qCritical().noquote() << message;
}

} // namespace Core::Logging
