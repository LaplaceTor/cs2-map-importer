#include "Application/Environment/SteamService.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Path/PathUtils.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameErrors.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <algorithm>
#include <utility>

namespace Application::Environment {

Core::Result<Core::Path::FilesystemPath> SteamService::detectSteamInstallPath(
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<Core::Path::FilesystemPath>([&]() -> Core::Result<Core::Path::FilesystemPath> {
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
            QStringLiteral("No Steam installation found on host system"));
    }, QStringLiteral("Steam installation detection failed"));
}

Core::Result<std::vector<SteamLibrary>> SteamService::detectLibraries(
    const Core::Path::FilesystemPath& steamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<std::vector<SteamLibrary>>([&]() -> Core::Result<std::vector<SteamLibrary>> {
        Core::Path::FilesystemPath resolvedSteamPath;

        if (steamPath.isValid()) {
            if (!steamPath.exists() || !steamPath.isDirectory()) {
                if (logCtx) {
                    logCtx->warning(QStringLiteral("Specified Steam directory does not exist or is not a directory: %1").arg(steamPath.toString()));
                }
                return Core::Result<std::vector<SteamLibrary>>::failure(
                    Core::Error::ErrorCode::DirectoryNotFound,
                    QStringLiteral("Specified Steam directory does not exist or is not a directory"),
                    steamPath.toString());
            }
            resolvedSteamPath = steamPath;
        } else {
            auto detectRes = detectSteamInstallPath(logCtx);
            if (!detectRes.isSuccess()) {
                return Core::Result<std::vector<SteamLibrary>>::failure(
                    detectRes.error(),
                    QStringLiteral("No Steam installation found on system"));
            }
            resolvedSteamPath = detectRes.value();
        }

        Core::Path::FilesystemPath vdfPath(QDir(resolvedSteamPath.toString()).filePath(QStringLiteral("steamapps/libraryfolders.vdf")));
        if (vdfPath.exists() && vdfPath.isFile()) {
            if (logCtx) {
                logCtx->debug(QStringLiteral("Loading Steam library folders configuration from: %1").arg(vdfPath.toString()));
            }
            auto parseRes = parseLibraryFolders(vdfPath, logCtx);
            if (!parseRes.isSuccess()) {
                if (logCtx) {
                    logCtx->error(QStringLiteral("Corrupted libraryfolders.vdf at: %1").arg(vdfPath.toString()));
                }
                return Core::Result<std::vector<SteamLibrary>>::failure(
                    parseRes.error(),
                    QStringLiteral("Failed to parse Steam library configuration"));
            }
            if (!parseRes.value().empty()) {
                if (logCtx) {
                    logCtx->debug(QStringLiteral("Discovered %1 Steam library folder(s)").arg(parseRes.value().size()));
                }
                return parseRes;
            }
            if (logCtx) {
                logCtx->debug(QStringLiteral("libraryfolders.vdf contains no libraries, falling back to Steam root: %1").arg(resolvedSteamPath.toString()));
            }
        } else {
            if (logCtx) {
                logCtx->debug(QStringLiteral("libraryfolders.vdf not found, falling back to Steam root: %1").arg(resolvedSteamPath.toString()));
            }
        }

        SteamLibrary defaultLib;
        defaultLib.path = resolvedSteamPath;

        // Scan for appmanifests in defaultLib
        QDir steamappsDir(QDir(resolvedSteamPath.toString()).filePath(QStringLiteral("steamapps")));
        if (steamappsDir.exists()) {
            const QStringList manifests = steamappsDir.entryList(QStringList() << QStringLiteral("appmanifest_*.acf"), QDir::Files);
            static const QRegularExpression manifestRegex(QStringLiteral("appmanifest_(\\d+)\\.acf"), QRegularExpression::CaseInsensitiveOption);
            for (const auto& manifest : manifests) {
                auto match = manifestRegex.match(manifest);
                if (match.hasMatch()) {
                    bool ok = false;
                    int appId = match.captured(1).toInt(&ok);
                    if (ok && appId > 0) {
                        defaultLib.installedAppIds.push_back(appId);
                    }
                }
            }
        }

        return Core::Result<std::vector<SteamLibrary>>::success({std::move(defaultLib)});
    }, QStringLiteral("Steam library detection failed"));
}

Core::Result<std::vector<SteamLibrary>> SteamService::parseLibraryFolders(
    const Core::Path::FilesystemPath& libraryFoldersVdfPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx)
{
    return Application::Execution::ExecutionGuard::guard<std::vector<SteamLibrary>>([&]() -> Core::Result<std::vector<SteamLibrary>> {
        if (!libraryFoldersVdfPath.isValid() || !libraryFoldersVdfPath.isFile()) {
            if (logCtx) {
                logCtx->warning(QStringLiteral("Invalid libraryfolders.vdf path: %1").arg(libraryFoldersVdfPath.toString()));
            }
            return Core::Result<std::vector<SteamLibrary>>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QStringLiteral("libraryfolders.vdf not found or invalid"),
                libraryFoldersVdfPath.toString());
        }

        Core::KeyValues::KeyValuesDocument doc;
        auto loadRes = doc.loadFromFile(libraryFoldersVdfPath);
        if (!loadRes.isSuccess()) {
            if (logCtx) {
                logCtx->warning(QStringLiteral("Failed to parse libraryfolders.vdf: %1").arg(loadRes.message()));
            }
            return Core::Result<std::vector<SteamLibrary>>::failure(
                loadRes.error(),
                QStringLiteral("Failed to parse libraryfolders.vdf"));
        }

        const auto* rootNode = &doc.root();
        const auto* lfNode = doc.findChild(QStringLiteral("libraryfolders"));
        const auto* container = lfNode ? lfNode : rootNode;

        std::vector<SteamLibrary> libraries;

        for (const auto& child : container->children()) {
            QString pathStr = child.property(QStringLiteral("path"));
            if (pathStr.isEmpty() && !child.isSection() && !child.value().isEmpty()) {
                // Older VDF format where child value was the library path (e.g. "1" "D:\\SteamLibrary")
                pathStr = child.value();
            }

            if (pathStr.isEmpty()) {
                continue;
            }

            Core::Path::FilesystemPath libPath(pathStr);
            if (!libPath.isValid()) {
                continue;
            }

            // Avoid duplicates
            auto it = std::find_if(libraries.begin(), libraries.end(), [&](const SteamLibrary& l) {
                return l.path == libPath;
            });

            if (it != libraries.end()) {
                continue;
            }

            SteamLibrary lib;
            lib.path = libPath;

            // Parse apps sub-node
            const auto* appsNode = child.findChild(QStringLiteral("apps"));
            if (appsNode) {
                for (const auto& appChild : appsNode->children()) {
                    bool ok = false;
                    int appId = appChild.name().toInt(&ok);
                    if (ok && appId > 0) {
                        if (std::find(lib.installedAppIds.begin(), lib.installedAppIds.end(), appId) == lib.installedAppIds.end()) {
                            lib.installedAppIds.push_back(appId);
                        }
                    }
                }
            }

            // Also check actual appmanifest_*.acf files in <libPath>/steamapps
            QDir steamappsDir(QDir(libPath.toString()).filePath(QStringLiteral("steamapps")));
            if (steamappsDir.exists()) {
                const QStringList manifests = steamappsDir.entryList(QStringList() << QStringLiteral("appmanifest_*.acf"), QDir::Files);
                static const QRegularExpression manifestRegex(QStringLiteral("appmanifest_(\\d+)\\.acf"), QRegularExpression::CaseInsensitiveOption);
                for (const auto& manifest : manifests) {
                    auto match = manifestRegex.match(manifest);
                    if (match.hasMatch()) {
                        bool ok = false;
                        int appId = match.captured(1).toInt(&ok);
                        if (ok && appId > 0) {
                            if (std::find(lib.installedAppIds.begin(), lib.installedAppIds.end(), appId) == lib.installedAppIds.end()) {
                                lib.installedAppIds.push_back(appId);
                            }
                        }
                    }
                }
            }

            libraries.push_back(std::move(lib));
        }

        return Core::Result<std::vector<SteamLibrary>>::success(std::move(libraries));
    }, QStringLiteral("Parse libraryfolders.vdf failed"));
}

Core::Result<QString> SteamService::readAppInstallDir(const Core::Path::FilesystemPath& libraryPath, int appId) {
    return Application::Execution::ExecutionGuard::guard<QString>([&]() -> Core::Result<QString> {
        if (!libraryPath.isValid() || !libraryPath.isDirectory()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::DirectoryNotFound,
                QStringLiteral("Invalid Steam library path"),
                libraryPath.toString());
        }
        if (appId <= 0) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("Invalid Steam AppID"),
                QString::number(appId));
        }

        QString manifestPathStr = QDir(libraryPath.toString()).filePath(QStringLiteral("steamapps/appmanifest_%1.acf").arg(appId));
        Core::Path::FilesystemPath manifestPath(manifestPathStr);
        if (!manifestPath.exists() || !manifestPath.isFile()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QStringLiteral("App manifest file not found"),
                manifestPath.toString());
        }

        Core::KeyValues::KeyValuesDocument doc;
        auto loadRes = doc.loadFromFile(manifestPath);
        if (!loadRes.isSuccess()) {
            return Core::Result<QString>::failure(
                loadRes.error(),
                QStringLiteral("Failed to parse app manifest"));
        }

        const auto* appState = doc.findChild(QStringLiteral("AppState"));
        const auto* node = appState ? appState : &doc.root();

        QString installDir = node->property(QStringLiteral("installdir"));
        if (installDir.isEmpty()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Field 'installdir' not found in app manifest"),
                manifestPath.toString());
        }
        return Core::Result<QString>::success(installDir);
    }, QStringLiteral("Read app installdir failed"));
}

Core::Result<QString> SteamService::readAppName(const Core::Path::FilesystemPath& libraryPath, int appId) {
    return Application::Execution::ExecutionGuard::guard<QString>([&]() -> Core::Result<QString> {
        if (!libraryPath.isValid() || !libraryPath.isDirectory()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::DirectoryNotFound,
                QStringLiteral("Invalid Steam library path"),
                libraryPath.toString());
        }
        if (appId <= 0) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("Invalid Steam AppID"),
                QString::number(appId));
        }

        QString manifestPathStr = QDir(libraryPath.toString()).filePath(QStringLiteral("steamapps/appmanifest_%1.acf").arg(appId));
        Core::Path::FilesystemPath manifestPath(manifestPathStr);
        if (!manifestPath.exists() || !manifestPath.isFile()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QStringLiteral("App manifest file not found"),
                manifestPath.toString());
        }

        Core::KeyValues::KeyValuesDocument doc;
        auto loadRes = doc.loadFromFile(manifestPath);
        if (!loadRes.isSuccess()) {
            return Core::Result<QString>::failure(
                loadRes.error(),
                QStringLiteral("Failed to parse app manifest"));
        }

        const auto* appState = doc.findChild(QStringLiteral("AppState"));
        const auto* node = appState ? appState : &doc.root();

        QString appName = node->property(QStringLiteral("name"));
        if (appName.isEmpty()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Field 'name' not found in app manifest"),
                manifestPath.toString());
        }
        return Core::Result<QString>::success(appName);
    }, QStringLiteral("Read app name failed"));
}

Core::Result<void> SteamService::validateGameFiles(
    int appId,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) {
    return Application::Execution::ExecutionGuard::guard<void>([&]() -> Core::Result<void> {
        if (appId <= 0) {
            if (logCtx) {
                logCtx->error(QStringLiteral("Invalid Steam AppID: %1").arg(appId));
            }
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("Invalid Steam AppID"),
                QString::number(appId));
        }
        if (logCtx) {
            logCtx->info(QStringLiteral("Requesting Steam game files validation for AppID: %1").arg(appId));
        }
        QUrl validateUrl(QStringLiteral("steam://validate/") + QString::number(appId));
        bool ok = QDesktopServices::openUrl(validateUrl);
        if (!ok) {
            if (logCtx) {
                logCtx->error(QStringLiteral("Failed to open Steam validation URL for AppID: %1").arg(appId));
            }
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::OperationFailed,
                QStringLiteral("Failed to open Steam validation URL"),
                validateUrl.toString());
        }
        return Core::Result<void>::success();
    }, QStringLiteral("Steam validation failed"));
}

Core::Result<void> SteamService::validateGameFiles(
    Domain::Game::GameType type,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) {
    return Application::Execution::ExecutionGuard::guard<void>([&]() -> Core::Result<void> {
        const auto* def = Domain::Game::GameRegistry::findByType(type);
        if (!def || def->primaryAppId <= 0) {
            QString typeStr = Domain::Game::GameRegistry::gameTypeToString(type);
            if (logCtx) {
                logCtx->error(QStringLiteral("No primary AppID registered for game type: %1").arg(typeStr));
            }
            return Core::Result<void>::failure(
                Domain::Game::GameErrors::unsupportedGame(
                    QStringLiteral("No primary AppID registered for game type"),
                    typeStr));
        }
        return validateGameFiles(def->primaryAppId, logCtx);
    }, QStringLiteral("Steam validation failed"));
}

} // namespace Application::Environment
