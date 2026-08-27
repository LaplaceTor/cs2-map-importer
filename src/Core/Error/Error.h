#pragma once

#include <QString>
#include <type_traits>
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

    static Error readFailed(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::ReadFailed, msg, details);
    }

    static Error writeFailed(const QString& msg = QString(), const QString& details = QString())
    {
        return Error(ErrorCode::WriteFailed, msg, details);
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

    template <typename EnumT>
    static Error domain(const QString& domainName, EnumT domainCode, const QString& message = QString(), const QString& details = QString(), ErrorCode highLevelCode = ErrorCode::DomainError)
    {
        static_assert(std::is_enum_v<EnumT>, "Error::domain requires an enum or enum class type for domainCode");
        Error err(highLevelCode, message, details);
        err.m_domain = domainName;
        err.m_domainCode = static_cast<int>(domainCode);
        return err;
    }

    bool isSuccess() const noexcept { return m_code == ErrorCode::Success; }
    bool isFailure() const noexcept { return m_code != ErrorCode::Success; }

    ErrorCode code() const noexcept { return m_code; }
    const QString& message() const noexcept { return m_message; }
    const QString& details() const noexcept { return m_details; }

    bool hasDomain() const noexcept { return !m_domain.isEmpty(); }
    const QString& domain() const noexcept { return m_domain; }
    int domainCode() const noexcept { return m_domainCode; }

    bool isDomain(const QString& domainName) const noexcept { return m_domain == domainName; }

    bool is(ErrorCode code) const noexcept
    {
        return m_code == code;
    }

    template <typename EnumT>
    bool is(EnumT code) const noexcept
    {
        static_assert(std::is_enum_v<EnumT>, "Error::is requires an enum or enum class type");
        return m_domainCode == static_cast<int>(code);
    }

    template <typename EnumT>
    EnumT domainCodeAs() const noexcept
    {
        static_assert(std::is_enum_v<EnumT>, "Error::domainCodeAs requires an enum or enum class type");
        return static_cast<EnumT>(m_domainCode);
    }

    QString toString() const
    {
        if (m_code == ErrorCode::Success) {
            return QStringLiteral("Success");
        }
        QString prefix;
        if (!m_domain.isEmpty()) {
            prefix = QStringLiteral("[%1:%2] ").arg(m_domain, QString::number(m_domainCode));
        }
        if (m_details.isEmpty()) {
            return prefix + (m_message.isEmpty() ? QStringLiteral("Error code %1").arg(static_cast<int>(m_code)) : m_message);
        }
        if (m_message.isEmpty()) {
            return prefix + m_details;
        }
        return QStringLiteral("%1%2 (%3)").arg(prefix, m_message, m_details);
    }

    bool operator==(const Error& other) const noexcept
    {
        return m_code == other.m_code &&
               m_domain == other.m_domain &&
               m_domainCode == other.m_domainCode &&
               m_message == other.m_message &&
               m_details == other.m_details;
    }

    bool operator!=(const Error& other) const noexcept
    {
        return !(*this == other);
    }

private:
    ErrorCode m_code = ErrorCode::Success;
    QString m_domain;
    int m_domainCode = 0;
    QString m_message;
    QString m_details;
};

} // namespace Core::Error

