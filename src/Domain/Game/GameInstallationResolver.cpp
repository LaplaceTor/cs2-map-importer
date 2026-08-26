#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/GameDefinition.h"
#include "Domain/Game/GameRegistry.h"
#include "Domain/Game/GameValidator.h"
#include "Domain/Game/GameInfoParser.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace Domain::Game {

QString ResolvedGameInstallation::modName() const
{
    const auto* def = GameRegistry::findByType(type);
    if (def && type != GameType::Custom && type != GameType::Unknown && !def->modName().isEmpty()) {
        return def->modName();
    }
    if (gameInfoPath.isValid()) {
        return gameInfoPath.parentPath().fileName();
    }
    return QString();
}

Core::Path::FilesystemPath ResolvedGameInstallation::contentDirectory() const
{
    if (!baseDirectory.isValid()) return Core::Path::FilesystemPath();
    const QString name = modName();
    if (name.isEmpty()) return Core::Path::FilesystemPath();
    return Core::Path::FilesystemPath(QDir(baseDirectory.toString()).filePath(
        isSource2 ? (QStringLiteral("content/") + name) : name));
}

Core::Path::FilesystemPath ResolvedGameInstallation::modDirectory() const
{
    if (gameInfoPath.isValid()) {
        return gameInfoPath.parentPath();
    }
    if (!baseDirectory.isValid()) return Core::Path::FilesystemPath();
    const auto* def = GameRegistry::findByType(type);
    if (def && !def->modSubdirectory.isEmpty()) {
        return Core::Path::FilesystemPath(QDir(baseDirectory.toString()).filePath(def->modSubdirectory));
    }
    return Core::Path::FilesystemPath();
}

Core::Path::FilesystemPath ResolvedGameInstallation::addonGameDirectory(const QString& addonName) const
{
    if (!baseDirectory.isValid() || !isSource2) return Core::Path::FilesystemPath();
    const QString name = modName();
    if (name.isEmpty()) return Core::Path::FilesystemPath();
    QString rel = QStringLiteral("game/") + name + QStringLiteral("_addons");
    if (!addonName.isEmpty()) {
        rel += QLatin1Char('/') + addonName;
    }
    return Core::Path::FilesystemPath(QDir(baseDirectory.toString()).filePath(rel));
}

Core::Path::FilesystemPath ResolvedGameInstallation::addonContentDirectory(const QString& addonName) const
{
    if (!baseDirectory.isValid() || !isSource2) return Core::Path::FilesystemPath();
    const QString name = modName();
    if (name.isEmpty()) return Core::Path::FilesystemPath();
    QString rel = QStringLiteral("content/") + name + QStringLiteral("_addons");
    if (!addonName.isEmpty()) {
        rel += QLatin1Char('/') + addonName;
    }
    return Core::Path::FilesystemPath(QDir(baseDirectory.toString()).filePath(rel));
}

Core::Async::TaskResult<ResolvedGameInstallation> GameInstallationResolver::createResolved(
    GameType type,
    const Core::Path::FilesystemPath& baseDir,
    const GameInfo& info)
{
    ResolvedGameInstallation res;
    res.type = type;
    const auto* def = GameRegistry::findByType(type);
    if (def && type != GameType::Custom && type != GameType::Unknown) {
        res.isSource2 = def->isSource2();
    } else {
        res.isSource2 = (info.gameInfoPath().extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
    }
    res.baseDirectory = baseDir.isValid() ? baseDir : info.baseDirectory();
    res.gameInfoPath = info.gameInfoPath();
    res.gameInfo = info;
    res.isValid = true;

    return Core::Async::TaskResult<ResolvedGameInstallation>::success(std::move(res));
}

Core::Async::TaskResult<ResolvedGameInstallation> GameInstallationResolver::resolveSource1(
    GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Source 1 directory path is empty or invalid: %1").arg(directory.toString()));
    }
    if (!directory.exists()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QStringLiteral("Source 1 directory does not exist: %1").arg(directory.toString()));
    }
    if (!directory.isDirectory()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Source 1 path is not a directory: %1").arg(directory.toString()));
    }

    auto infoResult = GameValidator::validateDirectory(directory, type);
    if (!infoResult.isSuccess()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(infoResult.error());
    }

    return createResolved(type, directory, infoResult.value());
}

Core::Async::TaskResult<ResolvedGameInstallation> GameInstallationResolver::resolveSource2(
    const Core::Path::FilesystemPath& directory,
    GameType type)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Source 2 directory path is empty or invalid: %1").arg(directory.toString()));
    }
    if (!directory.exists()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QStringLiteral("Source 2 directory does not exist: %1").arg(directory.toString()));
    }

    const auto* def = GameRegistry::findByType(type);
    Core::Path::FilesystemPath candidateBaseDir = directory;
    Core::Path::FilesystemPath giPath;

    if (directory.isFile()) {
        if (directory.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0) {
            giPath = directory;
            // Base dir heuristics: if <baseDir>/game/<mod>/gameinfo.gi, base is <baseDir>
            if (directory.parentPath().parentPath().fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
                candidateBaseDir = directory.parentPath().parentPath().parentPath();
            } else {
                candidateBaseDir = directory.parentPath();
            }
        } else {
            return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Target file is not a Source 2 gameinfo.gi: %1").arg(directory.toString()));
        }
    } else if (directory.isDirectory()) {
        // 1. If a specific Source 2 definition was given, check its expected modSubdirectory and gameInfoFileName
        if (def && def->isSource2() && !def->modSubdirectory.isEmpty()) {
            giPath = Core::Path::FilesystemPath(QDir(directory.toString()).filePath(
                def->modSubdirectory + QLatin1Char('/') + def->gameInfoFileName));
        }

        // 2. If not found, check if directory itself is <baseDir>/game/<mod>
        if (!giPath.exists() || !giPath.isFile()) {
            if (directory.parentPath().fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
                Core::Path::FilesystemPath directGi(QDir(directory.toString()).filePath(QStringLiteral("gameinfo.gi")));
                if (directGi.exists() && directGi.isFile()) {
                    giPath = directGi;
                    candidateBaseDir = directory.parentPath().parentPath();
                }
            }
        }

        // 3. If not found, scan <directory>/game/*/gameinfo.gi
        if (!giPath.exists() || !giPath.isFile()) {
            QDir gameDir(QDir(directory.toString()).filePath(QStringLiteral("game")));
            if (gameDir.exists()) {
                const auto subdirs = gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateGi(gameDir.filePath(subdir + QStringLiteral("/gameinfo.gi")));
                    if (candidateGi.exists() && candidateGi.isFile()) {
                        giPath = candidateGi;
                        candidateBaseDir = directory;
                        break;
                    }
                }
            }
        }

        // 4. If not found, check root <directory>/gameinfo.gi
        if (!giPath.exists() || !giPath.isFile()) {
            Core::Path::FilesystemPath rootGi(QDir(directory.toString()).filePath(QStringLiteral("gameinfo.gi")));
            if (rootGi.exists() && rootGi.isFile()) {
                giPath = rootGi;
                candidateBaseDir = directory;
            }
        }
    }

    if (!giPath.exists() || !giPath.isFile()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("Could not locate gameinfo.gi in Source 2 structure at: %1").arg(directory.toString()));
    }

    auto parseResult = GameInfoParser::parse(giPath, EngineType::Source2);
    if (!parseResult.isSuccess()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(parseResult.error());
    }

    const auto& optInfo = parseResult.value();
    auto identifiedType = GameValidator::identifyGameType(optInfo);
    GameType resolvedType = identifiedType.value_or(GameType::Custom);

    if (type != GameType::Unknown && type != GameType::Custom) {
        auto valRes = GameValidator::validateGameInfo(optInfo, type);
        if (!valRes.isSuccess()) {
            return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
                valRes.error(),
                QStringLiteral("Source 2 gameinfo at '%1' does not match expected game type '%2'")
                    .arg(giPath.toString(), GameRegistry::gameTypeToString(type)));
        }
        resolvedType = type;
    }

    return createResolved(resolvedType, candidateBaseDir, optInfo);
}

Core::Async::TaskResult<ResolvedGameInstallation> GameInstallationResolver::inspectGameInfo(
    const Core::Path::FilesystemPath& path)
{
    if (!path.isValid() || path.isEmpty()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("GameInfo path is empty or invalid: %1").arg(path.toString()));
    }
    if (!path.exists()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("GameInfo path does not exist: %1").arg(path.toString()));
    }

    Core::Path::FilesystemPath actualPath = path;
    if (actualPath.isDirectory()) {
        // 1. Try direct gameinfo.txt then gameinfo.gi
        Core::Path::FilesystemPath txtPath(QDir(actualPath.toString()).filePath(QStringLiteral("gameinfo.txt")));
        Core::Path::FilesystemPath giPath(QDir(actualPath.toString()).filePath(QStringLiteral("gameinfo.gi")));
        if (txtPath.exists() && txtPath.isFile()) {
            actualPath = txtPath;
        } else if (giPath.exists() && giPath.isFile()) {
            actualPath = giPath;
        } else {
            // 2. Check Source 2 layout: actualPath/game/*/gameinfo.gi
            QDir gameDir(QDir(actualPath.toString()).filePath(QStringLiteral("game")));
            if (gameDir.exists()) {
                const auto subdirs = gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateGi(gameDir.filePath(subdir + QStringLiteral("/gameinfo.gi")));
                    if (candidateGi.exists() && candidateGi.isFile()) {
                        actualPath = candidateGi;
                        break;
                    }
                }
            }
            if (!actualPath.isFile()) {
                // 3. Check Source 1 layout: actualPath/*/gameinfo.txt
                QDir baseDir(actualPath.toString());
                const auto subdirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const auto& subdir : subdirs) {
                    Core::Path::FilesystemPath candidateTxt(baseDir.filePath(subdir + QStringLiteral("/gameinfo.txt")));
                    if (candidateTxt.exists() && candidateTxt.isFile()) {
                        actualPath = candidateTxt;
                        break;
                    }
                }
            }
            if (!actualPath.isFile()) {
                return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
                    Core::Error::ErrorCode::FileNotFound,
                    QStringLiteral("No gameinfo.txt or gameinfo.gi found in directory: %1").arg(path.toString()));
            }
        }
    }

    if (!actualPath.exists() || !actualPath.isFile()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("GameInfo file not found at: %1").arg(actualPath.toString()));
    }

    bool isGi = (actualPath.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
    EngineType engine = isGi ? EngineType::Source2 : EngineType::Source1;

    auto parseResult = GameInfoParser::parse(actualPath, engine);
    if (!parseResult.isSuccess()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(parseResult.error());
    }

    const auto& optInfo = parseResult.value();
    auto identifiedType = GameValidator::identifyGameType(optInfo);
    GameType type = identifiedType.value_or(GameType::Custom);

    Core::Path::FilesystemPath baseDir = optInfo.baseDirectory();
    return createResolved(type, baseDir, optInfo);
}

Core::Async::TaskResult<ResolvedGameInstallation> GameInstallationResolver::resolveGameDirectory(
    GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Directory path is empty or invalid: %1").arg(directory.toString()));
    }
    if (!directory.exists()) {
        return Core::Async::TaskResult<ResolvedGameInstallation>::failure(
            Core::Error::ErrorCode::DirectoryNotFound,
            QStringLiteral("Directory does not exist: %1").arg(directory.toString()));
    }

    const auto* def = GameRegistry::findByType(type);
    if (def && def->isSource2()) {
        return resolveSource2(directory, type);
    }

    if (type == GameType::Custom) {
        return inspectGameInfo(directory);
    }

    return resolveSource1(type, directory);
}

QStringList GameInstallationResolver::listSource2Addons(const Core::Path::FilesystemPath& s2BasePath)
{
    if (!s2BasePath.isValid() || !s2BasePath.exists()) {
        return QStringList();
    }

    QStringList addons;
    QDir gameDir(QDir(s2BasePath.toString()).filePath(QStringLiteral("game")));
    if (!gameDir.exists()) {
        return addons;
    }

    const auto subdirs = gameDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& subdir : subdirs) {
        if (subdir.endsWith(QStringLiteral("_addons"), Qt::CaseInsensitive)) {
            QDir addonDir(gameDir.filePath(subdir));
            const auto specificAddons = addonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& addon : specificAddons) {
                if (!addons.contains(addon)) {
                    addons.append(addon);
                }
            }
        }
    }

    std::sort(addons.begin(), addons.end());
    return addons;
}

} // namespace Domain::Game
