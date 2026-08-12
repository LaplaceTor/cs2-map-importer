#pragma once

#include <QException>
#include <QString>
#include "ImportError.h"

class ImportException : public QException {
public:
    ImportException(ImportErrorCode errorCode, const QString& message)
        : m_errorCode(errorCode), m_message(message) {}

    void raise() const override { throw *this; }
    ImportException *clone() const override { return new ImportException(*this); }

    ImportErrorCode errorCode() const noexcept { return m_errorCode; }
    ImportErrorCode GetErrorCode() const noexcept { return m_errorCode; }

    QString message() const { return m_message; }
    QString GetMessage() const { return m_message; }

private:
    ImportErrorCode m_errorCode;
    QString m_message;
};
