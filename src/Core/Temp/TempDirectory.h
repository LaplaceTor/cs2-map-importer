#pragma once

#include <QString>
#include <QTemporaryDir>
#include <QDir>
#include <memory>

namespace Core::Temp {

class TempDirectory {
public:
    TempDirectory()
        : m_dir(std::make_unique<QTemporaryDir>()) {}

    explicit TempDirectory(const QString& templatePath)
        : m_dir(std::make_unique<QTemporaryDir>(templatePath)) {}

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

    QString Path() const {
        return path();
    }

    bool exists() const {
        return m_dir && m_dir->isValid() && QDir(m_dir->path()).exists();
    }

    bool Exists() const {
        return exists();
    }

    bool isValid() const {
        return m_dir && m_dir->isValid();
    }

    bool IsValid() const {
        return isValid();
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

} // namespace Core::Temp

namespace Core {
    using Temp::TempDirectory;
}

using Core::Temp::TempDirectory;
