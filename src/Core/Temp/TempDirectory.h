#pragma once

#include <QString>
#include <QTemporaryDir>
#include <QDir>

namespace Core::Temp {

class TempDirectory {
public:
    TempDirectory() = default;

    explicit TempDirectory(const QString& templatePath)
        : m_dir(templatePath) {}

    ~TempDirectory() = default;

    // Disable copy
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    // Enable move
    TempDirectory(TempDirectory&&) noexcept = default;
    TempDirectory& operator=(TempDirectory&&) noexcept = default;

    QString path() const {
        return m_dir.path();
    }

    QString Path() const {
        return path();
    }

    bool exists() const {
        return m_dir.isValid() && QDir(m_dir.path()).exists();
    }

    bool Exists() const {
        return exists();
    }

    bool isValid() const {
        return m_dir.isValid();
    }

    bool IsValid() const {
        return isValid();
    }

private:
    QTemporaryDir m_dir;
};

} // namespace Core::Temp

namespace Core {
    using Temp::TempDirectory;
}

using Core::Temp::TempDirectory;
