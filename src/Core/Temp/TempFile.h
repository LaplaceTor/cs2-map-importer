#pragma once

#include <QString>
#include <QFile>
#include <QTemporaryFile>
#include <memory>

#include "Core/Error/ImportException.h"
#include "Core/Error/ImportError.h"

namespace Core::Temp {

class TempFile {
public:
    TempFile()
        : m_file(std::make_unique<QTemporaryFile>()) {
        if (!m_file->open()) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to create temporary file: %1").arg(m_file->errorString()));
        }
        m_file->close();
    }

    explicit TempFile(const QString& templateName)
        : m_file(std::make_unique<QTemporaryFile>(templateName)) {
        if (!m_file->open()) {
            throw Core::Error::ImportException(
                Core::Error::ImportErrorCode::OperationFailed,
                QStringLiteral("Failed to create temporary file with template '%1': %2")
                    .arg(templateName, m_file->errorString()));
        }
        m_file->close();
    }

    ~TempFile() = default;

    // Disable copy
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    // Enable move
    TempFile(TempFile&&) noexcept = default;
    TempFile& operator=(TempFile&&) noexcept = default;

    QString path() const {
        return m_file ? m_file->fileName() : QString();
    }

    bool exists() const {
        return m_file && !path().isEmpty() && QFile::exists(path());
    }

    bool isValid() const {
        return m_file && m_file->isValid();
    }

private:
    std::unique_ptr<QTemporaryFile> m_file;
};

} // namespace Core::Temp
