#pragma once

#include <QString>
#include <utility>
#include "ErrorCode.h"

namespace Core::Error {

/**
 * @brief Value object representing a structured error with an error code, message, and optional technical details.
 */
class Error {
public:
    Error() = default;

    Error(ErrorCode code, QString message = QString(), QString details = QString())
        : m_code(code), m_message(std::move(message)), m_details(std::move(details)) {}

    static Error success()
    {
        return Error(ErrorCode::Success);
    }

    static Error unknown(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::Unknown, msg, details);
    }

    static Error invalidArgument(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::InvalidArgument, msg, details);
    }

    static Error invalidPath(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::InvalidPath, msg, details);
    }

    static Error fileNotFound(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::FileNotFound, msg, details);
    }

    static Error directoryNotFound(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::DirectoryNotFound, msg, details);
    }

    static Error permissionDenied(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::PermissionDenied, msg, details);
    }

    static Error invalidFile(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::InvalidFile, msg, details);
    }

    static Error processFailed(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::ProcessFailed, msg, details);
    }

    static Error processTimeout(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::ProcessTimeout, msg, details);
    }

    static Error operationFailed(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::OperationFailed, msg, details);
    }

    bool isSuccess() const noexcept { return m_code == ErrorCode::Success; }
    bool isFailure() const noexcept { return m_code != ErrorCode::Success; }

    ErrorCode code() const noexcept { return m_code; }
    const QString& message() const noexcept { return m_message; }
    const QString& details() const noexcept { return m_details; }

    QString toString() const
    {
        if (m_code == ErrorCode::Success) {
            return QStringLiteral("Success");
        }
        if (m_details.isEmpty()) {
            return m_message.isEmpty() ? QStringLiteral("Error code %1").arg(static_cast<int>(m_code)) : m_message;
        }
        if (m_message.isEmpty()) {
            return m_details;
        }
        return QStringLiteral("%1 (%2)").arg(m_message, m_details);
    }

    bool operator==(const Error& other) const noexcept
    {
        return m_code == other.m_code && m_message == other.m_message && m_details == other.m_details;
    }

    bool operator!=(const Error& other) const noexcept
    {
        return !(*this == other);
    }

private:
    ErrorCode m_code = ErrorCode::Success;
    QString m_message;
    QString m_details;
};

} // namespace Core::Error

