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

bool FileLease::acquireExclusive(const QString& filePath, QString* errorMessage)
{
    release();

    if (filePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("File path is empty.");
        }
        return false;
    }

    QFileInfo info(filePath);
    if (!info.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("File does not exist: %1").arg(filePath);
        }
        return false;
    }

    if (!info.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Path is not a regular file: %1").arg(filePath);
        }
        return false;
    }

    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());

#ifdef Q_OS_WIN
    // Open with dwShareMode = 0 (exclusive access: no read, write, or delete sharing)
    HANDLE hFile = CreateFileW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0, // Exclusive access
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
                0, // Exclusive access
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
        }
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to acquire exclusive handle on '%1' (Windows Error %2): %3")
                .arg(nativePath)
                .arg(err)
                .arg(QString::fromLocal8Bit(std::system_category().message(err).c_str()).trimmed());
        }
        return false;
    }

    m_handle = static_cast<void*>(hFile);
    m_filePath = nativePath;
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("Exclusive file lease is only supported on Windows.");
    }
    return false;
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

