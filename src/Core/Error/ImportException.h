#pragma once

#include <QException>
#include <QString>

#include "ImportError.h"

namespace Core::Error {

class ImportException : public QException {
public:
    explicit ImportException(ImportErrorCode errorCode, const QString& message = QString())
        : m_errorCode(errorCode), m_message(message) {}

    ~ImportException() override = default;

    void raise() const override { throw *this; }
    ImportException *clone() const override { return new ImportException(*this); }

    ImportErrorCode errorCode() const noexcept { return m_errorCode; }

    const QString& message() const noexcept { return m_message; }

private:
    ImportErrorCode m_errorCode;
    QString m_message;
};

} // namespace Core::Error
