#pragma once

#include <exception>
#include <QException>
#include <QString>
#include <QByteArray>
#include <utility>
#include "Error.h"
#include "ErrorCode.h"

namespace Core::Error {

/**
 * @brief Standardized exception class carrying structured Error information.
 * Inherits from QException (which inherits std::exception) for safe thread transport and std::exception compatibility.
 */
class Exception : public QException {
public:
    explicit Exception(ErrorCode errorCode, const QString& message = QString(), const QString& details = QString())
        : m_error(errorCode, message, details),
          m_what(m_error.toString().toUtf8()) {}

    explicit Exception(Error error)
        : m_error(std::move(error)),
          m_what(m_error.toString().toUtf8()) {}

    ~Exception() override = default;

    void raise() const override { throw *this; }
    Exception* clone() const override { return new Exception(*this); }

    const char* what() const noexcept override
    {
        return m_what.constData();
    }

    const Error& error() const noexcept { return m_error; }
    ErrorCode errorCode() const noexcept { return m_error.code(); }
    const QString& message() const noexcept { return m_error.message(); }
    const QString& details() const noexcept { return m_error.details(); }

private:
    Error m_error;
    QByteArray m_what;
};

/**
 * @brief Backward compatibility alias for legacy code.
 */
using ImportException = Exception;

} // namespace Core::Error
