#include "Core/FileSystem/FileLease.h"
#include <QFileInfo>
#include <QDir>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <system_error>
#endif

namespace Core::FileSystem {

FileLease::~FileLease()
{
    release();
}

FileLease::FileLease(FileLease&& other) noexcept
#ifdef Q_OS_WIN
    : m_handle(other.m_handle)
    , m_filePath(std::move(other.m_filePath))
{
    other.m_handle = nullptr;
}
#else
    : m_filePath(std::move(other.m_filePath))
{
}
#endif

FileLease& FileLease::operator=(FileLease&& other) noexcept
{
    if (this != &other) {
        release();
#ifdef Q_OS_WIN
        m_handle = other.m_handle;
        other.m_handle = nullptr;
#endif
        m_filePath = std::move(other.m_filePath);
    }
    return *this;
}

FileLeaseResult FileLease::acquireExclusive(const QString& filePath)
{
    release();

    if (filePath.isEmpty()) {
        return {FileLeaseError::InvalidPath, QStringLiteral("File path is empty.")};
    }

    QFileInfo info(filePath);
    if (!info.exists()) {
        return {FileLeaseError::NotFound, QStringLiteral("File does not exist: %1").arg(filePath)};
    }

    if (!info.isFile()) {
        return {FileLeaseError::InvalidPath, QStringLiteral("Path is not a regular file: %1").arg(filePath)};
    }

    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());

#ifdef Q_OS_WIN
    // Open with dwShareMode = 0 (exclusive access: no read, write, or delete sharing)
    HANDLE hFile = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0, // Exclusive access: no sharing
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            // Retry with GENERIC_READ in case file has read-only attribute on filesystem
            hFile = CreateFileW(
                reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                GENERIC_READ,
                0, // Exclusive access: no sharing
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
        }
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        FileLeaseError errorType = FileLeaseError::Unknown;
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
            errorType = FileLeaseError::AlreadyInUse;
        } else if (err == ERROR_ACCESS_DENIED) {
            errorType = FileLeaseError::AccessDenied;
        } else if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            errorType = FileLeaseError::NotFound;
        }

        const QString systemMsg = QStringLiteral("Failed to acquire exclusive handle on '%1' (Windows Error %2): %3")
            .arg(nativePath)
            .arg(err)
            .arg(QString::fromLocal8Bit(std::system_category().message(err).c_str()).trimmed());

        return {errorType, systemMsg};
    }

    m_handle = static_cast<void*>(hFile);
    m_filePath = nativePath;
    return {FileLeaseError::None, QString()};
#else
    return {FileLeaseError::Unsupported, QStringLiteral("Exclusive file lease is only supported on Windows.")};
#endif
}

void FileLease::release() noexcept
{
#ifdef Q_OS_WIN
    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#endif
    m_filePath.clear();
}

bool FileLease::isHeld() const noexcept
{
#ifdef Q_OS_WIN
    return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
#else
    return false;
#endif
}

QString FileLease::filePath() const
{
    return m_filePath;
}

} // namespace Core::FileSystem
