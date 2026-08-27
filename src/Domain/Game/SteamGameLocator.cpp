#include "Domain/Game/SteamGameLocator.h"
#include "Core/Path/FilesystemPath.h"
#include <QSet>

namespace Domain::Game {

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
    const InstallDirReaderFn& installDirReader)
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
        if (installDirReader) {
            auto dirRes = installDirReader(libraryPath, appId);
            if (dirRes.isSuccess()) {
                installDirName = dirRes.value();
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
    const InstallDirReaderFn& installDirReader)
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
        if (installDirReader) {
            auto dirRes = installDirReader(libraryPath, appId);
            if (dirRes.isSuccess()) {
                installDirName = dirRes.value();
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

