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

Core::Path::FilesystemPath SteamService::detectSteamInstallPath(
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) {
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
                return path;
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
                return path;
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
                return path;
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
            return path;
        }
    }

    if (logCtx) {
        logCtx->warning(QStringLiteral("No Steam installation path found on system"));
    }
    return Core::Path::FilesystemPath();
}

std::vector<SteamLibrary> SteamService::detectLibraries(
    const Core::Path::FilesystemPath& steamPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) {
    Core::Path::FilesystemPath resolvedSteamPath = steamPath.isValid() ? steamPath : detectSteamInstallPath(logCtx);
    if (!resolvedSteamPath.isValid() || !resolvedSteamPath.isDirectory()) {
        if (logCtx) {
            logCtx->warning(QStringLiteral("Invalid or non-existent Steam directory: %1").arg(resolvedSteamPath.toString()));
        }
        return {};
    }

    Core::Path::FilesystemPath vdfPath(QDir(resolvedSteamPath.toString()).filePath(QStringLiteral("steamapps/libraryfolders.vdf")));
    if (vdfPath.exists() && vdfPath.isFile()) {
        if (logCtx) {
            logCtx->debug(QStringLiteral("Loading Steam library folders configuration from: %1").arg(vdfPath.toString()));
        }
        auto libs = parseLibraryFolders(vdfPath, logCtx);
        if (!libs.empty()) {
            if (logCtx) {
                logCtx->debug(QStringLiteral("Discovered %1 Steam library folder(s)").arg(libs.size()));
            }
            return libs;
        }
    }

    // Fallback: If libraryfolders.vdf is not found or empty, treat the Steam install directory as the single library
    if (logCtx) {
        logCtx->debug(QStringLiteral("libraryfolders.vdf not found or empty, falling back to Steam root: %1").arg(resolvedSteamPath.toString()));
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

    return {defaultLib};
}

std::vector<SteamLibrary> SteamService::parseLibraryFolders(
    const Core::Path::FilesystemPath& libraryFoldersVdfPath,
    std::shared_ptr<Core::Logging::TaskLoggingContext> logCtx) {
    if (!libraryFoldersVdfPath.isValid() || !libraryFoldersVdfPath.isFile()) {
        if (logCtx) {
            logCtx->warning(QStringLiteral("Invalid libraryfolders.vdf path: %1").arg(libraryFoldersVdfPath.toString()));
        }
        return {};
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromFile(libraryFoldersVdfPath);
    if (!loadRes.isSuccess()) {
        if (logCtx) {
            logCtx->warning(QStringLiteral("Failed to parse libraryfolders.vdf: %1").arg(loadRes.message()));
        }
        return {};
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

    return libraries;
}

QString SteamService::readAppInstallDir(const Core::Path::FilesystemPath& libraryPath, int appId) {
    if (!libraryPath.isValid() || appId <= 0) {
        return QString();
    }

    QString manifestPathStr = QDir(libraryPath.toString()).filePath(QStringLiteral("steamapps/appmanifest_%1.acf").arg(appId));
    Core::Path::FilesystemPath manifestPath(manifestPathStr);
    if (!manifestPath.exists() || !manifestPath.isFile()) {
        return QString();
    }

    Core::KeyValues::KeyValuesDocument doc;
    if (!doc.loadFromFile(manifestPath).isSuccess()) {
        return QString();
    }

    const auto* appState = doc.findChild(QStringLiteral("AppState"));
    const auto* node = appState ? appState : &doc.root();

    return node->property(QStringLiteral("installdir"));
}

QString SteamService::readAppName(const Core::Path::FilesystemPath& libraryPath, int appId) {
    if (!libraryPath.isValid() || appId <= 0) {
        return QString();
    }

    QString manifestPathStr = QDir(libraryPath.toString()).filePath(QStringLiteral("steamapps/appmanifest_%1.acf").arg(appId));
    Core::Path::FilesystemPath manifestPath(manifestPathStr);
    if (!manifestPath.exists() || !manifestPath.isFile()) {
        return QString();
    }

    Core::KeyValues::KeyValuesDocument doc;
    if (!doc.loadFromFile(manifestPath).isSuccess()) {
        return QString();
    }

    const auto* appState = doc.findChild(QStringLiteral("AppState"));
    const auto* node = appState ? appState : &doc.root();

    return node->property(QStringLiteral("name"));
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
