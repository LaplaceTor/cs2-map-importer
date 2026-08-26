#pragma once

#include <QString>
#include <QTemporaryDir>
#include <QDir>
#include <memory>

#include "Core/Error/Exception.h"
#include "Core/Error/ErrorCode.h"

namespace Core::Temp {

class TempDirectory {
public:
    TempDirectory()
        : m_dir(std::make_unique<QTemporaryDir>()) {
        if (!m_dir->isValid()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("Failed to create temporary directory"));
        }
    }

    explicit TempDirectory(const QString& templatePath)
        : m_dir(std::make_unique<QTemporaryDir>(templatePath)) {
        if (!m_dir->isValid()) {
            throw Core::Error::Exception(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("Failed to create temporary directory with template '%1'").arg(templatePath));
        }
    }

    ~TempDirectory() = default;

    // Disable copy
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    // Enable move
    TempDirectory(TempDirectory&&) noexcept = default;
    TempDirectory& operator=(TempDirectory&&) noexcept = default;

    QString path() const {
        return m_dir ? m_dir->path() : QString();
    }

    bool exists() const {
        return m_dir && m_dir->isValid() && QDir(m_dir->path()).exists();
    }

    bool isValid() const {
        return m_dir && m_dir->isValid();
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

} // namespace Core::Temp
