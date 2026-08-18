#pragma once

#include <QString>
#include <QFile>
#include <QTemporaryFile>
#include <memory>

namespace Core::Temp {

class TempFile {
public:
    TempFile()
        : m_file(std::make_unique<QTemporaryFile>()) {
        if (m_file->open()) {
            m_file->close();
        }
    }

    explicit TempFile(const QString& templateName)
        : m_file(std::make_unique<QTemporaryFile>(templateName)) {
        if (m_file->open()) {
            m_file->close();
        }
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

    QString Path() const {
        return path();
    }

    bool exists() const {
        return m_file && !path().isEmpty() && QFile::exists(path());
    }

    bool Exists() const {
        return exists();
    }

    bool isValid() const {
        return m_file && m_file->isValid();
    }

    bool IsValid() const {
        return isValid();
    }

private:
    std::unique_ptr<QTemporaryFile> m_file;
};

} // namespace Core::Temp

namespace Core {
    using Temp::TempFile;
}

using Core::Temp::TempFile;
