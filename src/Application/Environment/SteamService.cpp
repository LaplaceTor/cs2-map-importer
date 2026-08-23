#include "Application/Environment/SteamService.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <algorithm>

namespace Application::Environment {

Core::Path::FilesystemPath SteamService::detectSteamInstallPath() {
#ifdef Q_OS_WIN
    // 1. HKEY_CURRENT_USER\Software\Valve\Steam (SteamPath)
    {
        QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
        QString steamPath = reg.value(QStringLiteral("SteamPath")).toString();
        if (!steamPath.isEmpty()) {
            Core::Path::FilesystemPath path(steamPath);
            if (path.exists() && path.isDirectory()) {
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
                return path;
            }
        }
    }
#endif

    // Fallback standard locations
    const std::vector<QString> fallbacks = {
        QStringLiteral("C:/Program Files (x86)/Steam"),
        QStringLiteral("C:/Program Files/Steam"),
        QDir::homePath() + QStringLiteral("/.steam/steam"),
        QDir::homePath() + QStringLiteral("/.local/share/Steam"),
        QDir::homePath() + QStringLiteral("/Library/Application Support/Steam")
    };
    for (const auto& candidate : fallbacks) {
        Core::Path::FilesystemPath path(candidate);
        if (path.exists() && path.isDirectory()) {
            return path;
        }
    }

    return Core::Path::FilesystemPath();
}

std::vector<SteamLibrary> SteamService::detectLibraries(const Core::Path::FilesystemPath& steamPath) {
    Core::Path::FilesystemPath resolvedSteamPath = steamPath.isValid() ? steamPath : detectSteamInstallPath();
    if (!resolvedSteamPath.isValid() || !resolvedSteamPath.isDirectory()) {
        return {};
    }

    Core::Path::FilesystemPath vdfPath(QDir(resolvedSteamPath.toString()).filePath(QStringLiteral("steamapps/libraryfolders.vdf")));
    if (vdfPath.exists() && vdfPath.isFile()) {
        auto libs = parseLibraryFolders(vdfPath);
        if (!libs.empty()) {
            return libs;
        }
    }

    // Fallback: If libraryfolders.vdf is not found or empty, treat the Steam install directory as the single library
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

std::vector<SteamLibrary> SteamService::parseLibraryFolders(const Core::Path::FilesystemPath& libraryFoldersVdfPath) {
    if (!libraryFoldersVdfPath.isValid() || !libraryFoldersVdfPath.isFile()) {
        return {};
    }

    Core::KeyValues::KeyValuesDocument doc;
    QString error;
    if (!doc.loadFromFile(libraryFoldersVdfPath, &error)) {
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
    if (!doc.loadFromFile(manifestPath)) {
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
    if (!doc.loadFromFile(manifestPath)) {
        return QString();
    }

    const auto* appState = doc.findChild(QStringLiteral("AppState"));
    const auto* node = appState ? appState : &doc.root();

    return node->property(QStringLiteral("name"));
}

} // namespace Application::Environment

