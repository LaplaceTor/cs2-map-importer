#include <QCoreApplication>
#include "Application/Environment/Internal/SteamLibraryDetector.h"
#include <QSettings>
#include <vector>

namespace Application::Environment::Internal {

Core::Result<Core::Path::FilesystemPath> SteamLibraryDetector::detectSteamInstallPath(
    const Application::Async::SystemTaskLog* logCtx)
{
    if (logCtx) {
        logCtx->debug(QStringLiteral("Detecting Steam installation path..."));
    }

    // 1. HKEY_CURRENT_USER\Software\Valve\Steam (SteamPath)
    {
        QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
        QString steamPath = reg.value(QStringLiteral("SteamPath")).toString();
        if (!steamPath.isEmpty()) {
            Core::Path::FilesystemPath path(steamPath);
            if (path.exists() && path.isDirectory()) {
                if (logCtx) {
                    logCtx->debug(QStringLiteral("Found Steam installation in HKCU registry: %1").arg(path.toString()));
                }
                return Core::Result<Core::Path::FilesystemPath>::success(path);
            }
        }
    }
    // 2. HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam (InstallPath)
    {
        QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam"), QSettings::NativeFormat);
        QString installPath = reg.value(QStringLiteral("InstallPath")).toString();
        if (!installPath.isEmpty()) {
            Core::Path::FilesystemPath path(installPath);
            if (path.exists() && path.isDirectory()) {
                if (logCtx) {
                    logCtx->debug(QStringLiteral("Found Steam installation in HKLM registry: %1").arg(path.toString()));
                }
                return Core::Result<Core::Path::FilesystemPath>::success(path);
            }
        }
    }
    // 3. HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam (InstallPath)
    {
        QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"), QSettings::NativeFormat);
        QString installPath = reg.value(QStringLiteral("InstallPath")).toString();
        if (!installPath.isEmpty()) {
            Core::Path::FilesystemPath path(installPath);
            if (path.exists() && path.isDirectory()) {
                if (logCtx) {
                    logCtx->debug(QStringLiteral("Found Steam installation in WOW6432Node registry: %1").arg(path.toString()));
                }
                return Core::Result<Core::Path::FilesystemPath>::success(path);
            }
        }
    }

    // Fallback standard Windows locations
    const std::vector<QString> fallbacks = {
        QStringLiteral("C:/Program Files (x86)/Steam"),
        QStringLiteral("C:/Program Files/Steam")
    };
    for (const auto& candidate : fallbacks) {
        Core::Path::FilesystemPath path(candidate);
        if (path.exists() && path.isDirectory()) {
            if (logCtx) {
                logCtx->debug(QStringLiteral("Found Steam installation in standard fallback path: %1").arg(path.toString()));
            }
            return Core::Result<Core::Path::FilesystemPath>::success(path);
        }
    }

    if (logCtx) {
        logCtx->warning(QStringLiteral("No Steam installation path found on system"));
    }
    return Core::Result<Core::Path::FilesystemPath>::failure(
        Core::Error::ErrorCode::DirectoryNotFound,
        QCoreApplication::translate("SteamLibraryDetector", "No Steam installation found on host system"));
}

Core::Result<std::vector<Domain::Game::SteamLibrary>> SteamLibraryDetector::detectLibraries(
    const Core::Path::FilesystemPath& steamPath,
    const Application::Async::SystemTaskLog* logCtx)
{
    Core::Path::FilesystemPath resolvedSteamPath;

    if (steamPath.isValid()) {
        resolvedSteamPath = steamPath;
    } else {
        auto detectRes = detectSteamInstallPath(logCtx);
        if (!detectRes.isSuccess()) {
            return Core::Result<std::vector<Domain::Game::SteamLibrary>>::failure(
                detectRes.error(),
                QCoreApplication::translate("SteamLibraryDetector", "No Steam installation found on system"));
        }
        resolvedSteamPath = detectRes.value();
    }

    auto res = Domain::Game::SteamGameLocator::discoverLibraries(resolvedSteamPath);
    if (!res.isSuccess()) {
        if (logCtx) {
            logCtx->error(QStringLiteral("Steam library discovery failed at %1: %2")
                .arg(resolvedSteamPath.toString(), res.message()));
        }
        return res;
    }

    if (logCtx) {
        logCtx->debug(QStringLiteral("Discovered %1 Steam library folder(s)").arg(res.value().size()));
    }
    return res;
}

} // namespace Application::Environment::Internal
