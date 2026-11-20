#include "Domain/Game/SteamGameLocator.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Path/FilesystemPath.h"
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <utility>

namespace Domain::Game {

namespace {

void scanAppManifestIds(const Core::Path::FilesystemPath& libraryPath, std::vector<int>& appIds)
{
    QDir steamappsDir(QDir(libraryPath.toString()).filePath(QStringLiteral("steamapps")));
    if (!steamappsDir.exists()) {
        return;
    }

    const QStringList manifests = steamappsDir.entryList(QStringList() << QStringLiteral("appmanifest_*.acf"), QDir::Files);
    static const QRegularExpression manifestRegex(QStringLiteral("appmanifest_(\\d+)\\.acf"), QRegularExpression::CaseInsensitiveOption);
    for (const auto& manifest : manifests) {
        auto match = manifestRegex.match(manifest);
        if (match.hasMatch()) {
            bool ok = false;
            int appId = match.captured(1).toInt(&ok);
            if (ok && appId > 0) {
                if (std::find(appIds.begin(), appIds.end(), appId) == appIds.end()) {
                    appIds.push_back(appId);
                }
            }
        }
    }
}

Core::Result<QString> readAppManifestProperty(
    const Core::Path::FilesystemPath& libraryPath,
    int appId,
    const QString& propertyKey,
    const QString& missingFieldMessage)
{
    if (!libraryPath.isValid() || !libraryPath.isDirectory()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QCoreApplication::translate("SteamGameLocator", "Invalid Steam library path"),
            libraryPath.toString());
    }
    if (appId <= 0) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("SteamGameLocator", "Invalid Steam AppID"),
            QString::number(appId));
    }

    const QString manifestPathStr = QDir(libraryPath.toString()).filePath(
        QStringLiteral("steamapps/appmanifest_%1.acf").arg(appId));
    Core::Path::FilesystemPath manifestPath(manifestPathStr);
    if (!manifestPath.exists() || !manifestPath.isFile()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("SteamGameLocator", "App manifest file not found"),
            manifestPath.toString());
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromFile(manifestPath);
    if (!loadRes.isSuccess()) {
        return Core::Result<QString>::failure(
            loadRes.error(),
            QCoreApplication::translate("SteamGameLocator", "Failed to parse app manifest"));
    }

    const auto* appState = doc.findChild(QStringLiteral("AppState"));
    const auto* node = appState ? appState : &doc.root();

    const QString value = node->property(propertyKey);
    if (value.isEmpty()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidFile,
            missingFieldMessage,
            manifestPath.toString());
    }
    return Core::Result<QString>::success(value);
}

} // namespace

Core::Result<std::vector<SteamLibrary>> SteamGameLocator::discoverLibraries(
    const Core::Path::FilesystemPath& steamRootPath)
{
    if (!steamRootPath.isValid() || !steamRootPath.exists() || !steamRootPath.isDirectory()) {
        return Core::Result<std::vector<SteamLibrary>>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QCoreApplication::translate("SteamGameLocator", "Specified Steam directory does not exist or is not a directory"),
            steamRootPath.toString());
    }

    Core::Path::FilesystemPath vdfPath(QDir(steamRootPath.toString()).filePath(QStringLiteral("steamapps/libraryfolders.vdf")));
    if (vdfPath.exists() && vdfPath.isFile()) {
        auto parseRes = parseLibraryFolders(vdfPath);
        if (!parseRes.isSuccess()) {
            return Core::Result<std::vector<SteamLibrary>>::failure(
                parseRes.error(),
                QCoreApplication::translate("SteamGameLocator", "Failed to parse Steam library configuration"));
        }
        return parseRes;
    }

    // Fallback when libraryfolders.vdf does not exist: treat Steam root as a single library
    SteamLibrary defaultLib;
    defaultLib.path = steamRootPath;
    scanAppManifestIds(steamRootPath, defaultLib.installedAppIds);

    return Core::Result<std::vector<SteamLibrary>>::success({std::move(defaultLib)});
}

Core::Result<std::vector<SteamLibrary>> SteamGameLocator::parseLibraryFolders(
    const Core::Path::FilesystemPath& libraryFoldersVdfPath)
{
    if (!libraryFoldersVdfPath.isValid() || !libraryFoldersVdfPath.isFile()) {
        return Core::Result<std::vector<SteamLibrary>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("SteamGameLocator", "libraryfolders.vdf not found or invalid"),
            libraryFoldersVdfPath.toString());
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadRes = doc.loadFromFile(libraryFoldersVdfPath);
    if (!loadRes.isSuccess()) {
        return Core::Result<std::vector<SteamLibrary>>::failure(
            loadRes.error(),
            QCoreApplication::translate("SteamGameLocator", "Failed to parse libraryfolders.vdf"));
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
        scanAppManifestIds(libPath, lib.installedAppIds);

        libraries.push_back(std::move(lib));
    }

    return Core::Result<std::vector<SteamLibrary>>::success(std::move(libraries));
}

Core::Result<QString> SteamGameLocator::readAppInstallDir(
    const Core::Path::FilesystemPath& libraryPath,
    int appId)
{
    return readAppManifestProperty(
        libraryPath,
        appId,
        QStringLiteral("installdir"),
        QCoreApplication::translate("SteamGameLocator", "Field 'installdir' not found in app manifest"));
}

Core::Result<QString> SteamGameLocator::readAppName(
    const Core::Path::FilesystemPath& libraryPath,
    int appId)
{
    return readAppManifestProperty(
        libraryPath,
        appId,
        QStringLiteral("name"),
        QCoreApplication::translate("SteamGameLocator", "Field 'name' not found in app manifest"));
}

std::vector<Core::Path::FilesystemPath> SteamGameLocator::locateCandidateDirectories(
    const Core::Path::FilesystemPath& libraryPath,
    int appId,
    const QString& appInstallDirName)
{
    std::vector<Core::Path::FilesystemPath> candidates;
    if (!libraryPath.isValid() || !libraryPath.exists()) {
        return candidates;
    }

    Core::Path::FilesystemPath commonDir = libraryPath.join(QStringLiteral("steamapps")).join(QStringLiteral("common"));

    if (!appInstallDirName.isEmpty()) {
        candidates.push_back(commonDir.join(appInstallDirName));
    }

    auto defs = GameRegistry::findAllByAppId(appId);
    for (const auto* def : defs) {
        if (!def || def->defaultFolderName.isEmpty()) {
            continue;
        }
        if (def->defaultFolderName != appInstallDirName) {
            Core::Path::FilesystemPath defaultDir = commonDir.join(def->defaultFolderName);
            candidates.push_back(defaultDir);
        }
    }

    return candidates;
}

std::vector<ResolvedGameInstallation> SteamGameLocator::resolveGamesInLibrary(
    const Core::Path::FilesystemPath& libraryPath,
    const std::vector<int>& installedAppIds,
    const DiagnosticFn& diagnosticHandler)
{
    std::vector<ResolvedGameInstallation> results;
    if (!libraryPath.isValid() || !libraryPath.exists()) {
        return results;
    }

    QSet<GameType> resolvedTypes;

    for (int appId : installedAppIds) {
        auto defs = GameRegistry::findAllByAppId(appId);
        if (defs.empty()) {
            continue;
        }

        QString installDirName;
        auto dirRes = readAppInstallDir(libraryPath, appId);
        if (dirRes.isSuccess()) {
            installDirName = dirRes.value();
        } else {
            // Scheme A (Best-effort heuristic fallback):
            // 'installdir' in appmanifest_<appId>.acf is an optional hint pointing to custom install folder names.
            // If manifest reading fails (file corrupt, permission denied, or missing key), we preserve diagnostics
            // by notifying diagnosticHandler, then intentionally fall back to registry/default folder heuristics.
            if (diagnosticHandler) {
                diagnosticHandler(QStringLiteral("Failed to read manifest for AppID %1 in %2: %3 (falling back to default heuristics)")
                    .arg(QString::number(appId), libraryPath.toString(), dirRes.message()));
            }
        }
        auto candidateDirs = locateCandidateDirectories(libraryPath, appId, installDirName);

        for (const auto* def : defs) {
            if (!def || resolvedTypes.contains(def->type)) {
                continue;
            }

            for (const auto& candidateDir : candidateDirs) {
                if (!candidateDir.exists()) {
                    continue;
                }

                Core::Result<ResolvedGameInstallation> resolved;
                if (def->isSource2()) {
                    resolved = GameInstallationResolver::resolveSource2(candidateDir, def->type);
                } else {
                    resolved = GameInstallationResolver::resolveSource1(def->type, candidateDir);
                }

                if (resolved.isSuccess() && resolved->isValid) {
                    resolvedTypes.insert(def->type);
                    results.push_back(std::move(resolved.value()));
                    break;
                }
            }
        }
    }

    return results;
}

std::optional<ResolvedGameInstallation> SteamGameLocator::resolveGameInLibrary(
    const Core::Path::FilesystemPath& libraryPath,
    GameType type,
    const DiagnosticFn& diagnosticHandler)
{
    if (type == GameType::Unknown || type == GameType::Custom) {
        return std::nullopt;
    }

    const auto* def = GameRegistry::findByType(type);
    if (!def) {
        return std::nullopt;
    }

    for (int appId : def->allAppIds) {
        QString installDirName;
        auto dirRes = readAppInstallDir(libraryPath, appId);
        if (dirRes.isSuccess()) {
            installDirName = dirRes.value();
        } else {
            // Scheme A (Best-effort heuristic fallback):
            // 'installdir' in appmanifest_<appId>.acf is an optional hint pointing to custom install folder names.
            // If manifest reading fails, notify diagnosticHandler and intentionally fall back to registry/default folder heuristics.
            if (diagnosticHandler) {
                diagnosticHandler(QStringLiteral("Failed to read manifest for AppID %1 in %2: %3 (falling back to default heuristics)")
                    .arg(QString::number(appId), libraryPath.toString(), dirRes.message()));
            }
        }
        auto candidateDirs = locateCandidateDirectories(libraryPath, appId, installDirName);

        for (const auto& candidateDir : candidateDirs) {
            if (!candidateDir.exists()) {
                continue;
            }

            Core::Result<ResolvedGameInstallation> resolved;
            if (def->isSource2()) {
                resolved = GameInstallationResolver::resolveSource2(candidateDir, type);
            } else {
                resolved = GameInstallationResolver::resolveSource1(type, candidateDir);
            }

            if (resolved.isSuccess() && resolved->isValid) {
                return resolved.value();
            }
        }
    }

    return std::nullopt;
}

} // namespace Domain::Game
