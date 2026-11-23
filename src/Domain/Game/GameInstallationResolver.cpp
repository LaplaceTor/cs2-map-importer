#include <QCoreApplication>
#include "Domain/Game/GameInstallationResolver.h"
#include "Domain/Game/GameErrors.h"
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

Core::Result<ResolvedGameInstallation> GameInstallationResolver::createResolved(
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

    return Core::Result<ResolvedGameInstallation>::success(std::move(res));
}

Core::Result<ResolvedGameInstallation> GameInstallationResolver::resolveSource1(
    GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QCoreApplication::translate("GameInstallationResolver", "Source 1 directory path is empty or invalid"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 1 directory resolution failed"));
    }
    if (!directory.exists()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::directoryNotFound(
                QCoreApplication::translate("GameInstallationResolver", "Source 1 directory does not exist"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 1 directory resolution failed"));
    }
    if (!directory.isDirectory()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QCoreApplication::translate("GameInstallationResolver", "Source 1 path is not a directory"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 1 directory resolution failed"));
    }

    auto infoResult = GameValidator::validateDirectory(directory, type);
    if (!infoResult.isSuccess()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            infoResult.error(),
            QCoreApplication::translate("GameInstallationResolver", "Source 1 directory resolution failed"));
    }

    return createResolved(type, directory, infoResult.value());
}

Core::Result<ResolvedGameInstallation> GameInstallationResolver::resolveSource2(
    const Core::Path::FilesystemPath& directory,
    GameType type)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QCoreApplication::translate("GameInstallationResolver", "Source 2 directory path is empty or invalid"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 2 directory resolution failed"));
    }
    if (!directory.exists()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::directoryNotFound(
                QCoreApplication::translate("GameInstallationResolver", "Source 2 directory does not exist"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 2 directory resolution failed"));
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
            return Core::Result<ResolvedGameInstallation>::failure(
                Core::Error::Error::invalidFile(
                    QCoreApplication::translate("GameInstallationResolver", "Target file is not a Source 2 gameinfo.gi"),
                    directory.toString()),
                QCoreApplication::translate("GameInstallationResolver", "Source 2 directory resolution failed"));
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
        return Core::Result<ResolvedGameInstallation>::failure(
            GameErrors::gameInfoNotFound(
                QCoreApplication::translate("GameInstallationResolver", "Could not locate gameinfo.gi in Source 2 structure"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Source 2 directory resolution failed"));
    }

    auto parseResult = GameInfoParser::parse(giPath, EngineType::Source2);
    if (!parseResult.isSuccess()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            parseResult.error(),
            QCoreApplication::translate("GameInstallationResolver", "Failed to parse Source 2 gameinfo.gi"));
    }

    const auto& optInfo = parseResult.value();
    auto identifiedType = GameValidator::tryIdentifyGameType(optInfo);
    GameType resolvedType = identifiedType.value_or(GameType::Custom);

    if (type != GameType::Unknown && type != GameType::Custom) {
        auto valRes = GameValidator::validateGameInfo(optInfo, type);
        if (!valRes.isSuccess()) {
            return Core::Result<ResolvedGameInstallation>::failure(
                valRes.error(),
                QCoreApplication::translate("GameInstallationResolver", "Source 2 gameinfo validation failed"));
        }
        resolvedType = type;
    }

    return createResolved(resolvedType, candidateBaseDir, optInfo);
}

Core::Result<ResolvedGameInstallation> GameInstallationResolver::inspectGameInfo(
    const Core::Path::FilesystemPath& path)
{
    if (!path.isValid() || path.isEmpty()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QCoreApplication::translate("GameInstallationResolver", "GameInfo path is empty or invalid"),
                path.toString()),
            QCoreApplication::translate("GameInstallationResolver", "GameInfo inspection failed"));
    }
    if (!path.exists()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::fileNotFound(
                QCoreApplication::translate("GameInstallationResolver", "GameInfo path does not exist"),
                path.toString()),
            QCoreApplication::translate("GameInstallationResolver", "GameInfo inspection failed"));
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
                return Core::Result<ResolvedGameInstallation>::failure(
                    GameErrors::gameInfoNotFound(
                        QCoreApplication::translate("GameInstallationResolver", "No gameinfo.txt or gameinfo.gi found in directory"),
                        path.toString()),
                    QCoreApplication::translate("GameInstallationResolver", "GameInfo inspection failed"));
            }
        }
    }

    if (!actualPath.exists() || !actualPath.isFile()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            GameErrors::gameInfoNotFound(
                QCoreApplication::translate("GameInstallationResolver", "GameInfo file not found"),
                actualPath.toString()),
            QCoreApplication::translate("GameInstallationResolver", "GameInfo inspection failed"));
    }

    bool isGi = (actualPath.extension().compare(QStringLiteral("gi"), Qt::CaseInsensitive) == 0);
    EngineType engine = isGi ? EngineType::Source2 : EngineType::Source1;

    auto parseResult = GameInfoParser::parse(actualPath, engine);
    if (!parseResult.isSuccess()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            parseResult.error(),
            QCoreApplication::translate("GameInstallationResolver", "Failed to parse GameInfo file"));
    }

    const auto& optInfo = parseResult.value();
    auto identifiedType = GameValidator::tryIdentifyGameType(optInfo);
    GameType type = identifiedType.value_or(GameType::Custom);

    Core::Path::FilesystemPath baseDir = optInfo.baseDirectory();
    return createResolved(type, baseDir, optInfo);
}

Core::Result<ResolvedGameInstallation> GameInstallationResolver::resolveGameDirectory(
    GameType type,
    const Core::Path::FilesystemPath& directory)
{
    if (!directory.isValid() || directory.isEmpty()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::invalidPath(
                QCoreApplication::translate("GameInstallationResolver", "Directory path is empty or invalid"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Game directory resolution failed"));
    }
    if (!directory.exists()) {
        return Core::Result<ResolvedGameInstallation>::failure(
            Core::Error::Error::directoryNotFound(
                QCoreApplication::translate("GameInstallationResolver", "Directory does not exist"),
                directory.toString()),
            QCoreApplication::translate("GameInstallationResolver", "Game directory resolution failed"));
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
    QDir addonsDir(QDir(s2BasePath.toString()).filePath(QStringLiteral("content/csgo_addons")));
    if (!addonsDir.exists()) {
        return addons;
    }

    const auto subdirs = addonsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& addon : subdirs) {
        if (!addons.contains(addon)) {
            addons.append(addon);
        }
    }

    std::sort(addons.begin(), addons.end());
    return addons;
}

namespace {

QString slugifyCategorySegment(const QString& raw)
{
    QString result;
    result.reserve(raw.size());
    bool lastWasUnderscore = false;

    for (const QChar c : raw.trimmed().toLower()) {
        const ushort u = c.unicode();
        const bool isSeparatorOrIllegal =
            u < 32
            || c.isSpace()
            || c == QLatin1Char('<') || c == QLatin1Char('>')
            || c == QLatin1Char(':') || c == QLatin1Char('"')
            || c == QLatin1Char('/') || c == QLatin1Char('\\')
            || c == QLatin1Char('|') || c == QLatin1Char('?')
            || c == QLatin1Char('*');

        if (isSeparatorOrIllegal || c == QLatin1Char('_')) {
            if (!lastWasUnderscore && !result.isEmpty()) {
                result.append(QLatin1Char('_'));
                lastWasUnderscore = true;
            }
        } else {
            result.append(c);
            lastWasUnderscore = false;
        }
    }

    while (!result.isEmpty()
           && (result.startsWith(QLatin1Char('.')) || result.startsWith(QLatin1Char('_')))) {
        result.remove(0, 1);
    }
    while (!result.isEmpty()
           && (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char('_')))) {
        result.chop(1);
    }

    if (result.isEmpty() || result == QStringLiteral(".") || result == QStringLiteral("..")) {
        return {};
    }
    return result;
}

} // namespace

QString GameInstallationResolver::resolveIndexCategory(
    const QString& gameId,
    const GameInfo* gameInfo)
{
    const QString trimmedId = gameId.trimmed();
    if (const auto* def = GameRegistry::findById(trimmedId)) {
        if (def->type != GameType::Custom && def->type != GameType::Unknown) {
            return def->id;
        }
    }

    if (gameInfo) {
        QString rawName = gameInfo->game().trimmed();
        if (rawName.isEmpty()) {
            rawName = gameInfo->modDirectory().fileName().trimmed();
        }
        const QString slug = slugifyCategorySegment(rawName);
        if (!slug.isEmpty()) {
            if (slug.startsWith(QStringLiteral("custom_"))) {
                return slug;
            }
            return QStringLiteral("custom_") + slug;
        }
    }

    const QString fallbackSlug = slugifyCategorySegment(trimmedId);
    return fallbackSlug.isEmpty() ? QStringLiteral("custom") : fallbackSlug;
}

Core::Result<std::vector<Core::Path::FilesystemPath>> GameInstallationResolver::resolveCs2VpkTargets(
    const Core::Path::FilesystemPath& cs2BasePath)
{
    if (cs2BasePath.isEmpty() || !cs2BasePath.isValid()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("GameInstallationResolver", "CS2 base path is invalid or empty"));
    }

    const Core::Path::FilesystemPath giPath = cs2BasePath / QStringLiteral("game/csgo/gameinfo.gi");
    if (!giPath.exists()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("GameInstallationResolver", "CS2 gameinfo.gi not found at %1").arg(giPath.toString()));
    }

    auto parseRes = GameInfoParser::parse(giPath, EngineType::Source2);
    if (parseRes.isFailure()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            parseRes.error(),
            QCoreApplication::translate("GameInstallationResolver", "Failed to parse CS2 gameinfo.gi"));
    }

    const auto& doc = parseRes.value().document();
    const auto& rootNode = doc.root();

    const auto* fileSystemNode = rootNode.findChild(QStringLiteral("FileSystem"));
    const auto* searchPathsNode = fileSystemNode ? fileSystemNode->findChild(QStringLiteral("SearchPaths"))
                                                 : rootNode.findChild(QStringLiteral("SearchPaths"));

    std::vector<Core::Path::FilesystemPath> vpkPaths;

    if (searchPathsNode) {
        // Collect Game entries defined in gameinfo.gi SearchPaths (e.g. csgo, csgo_imported, csgo_core, core)
        for (const auto& child : searchPathsNode->children()) {
            if (child.name().compare(QStringLiteral("Game"), Qt::CaseInsensitive) == 0) {
                const QString val = child.value().trimmed();
                if (val.isEmpty()) {
                    continue;
                }
                const Core::Path::FilesystemPath gameDir = cs2BasePath / QStringLiteral("game") / val;
                const Core::Path::FilesystemPath vpkCandidate = gameDir / QStringLiteral("pak01_dir.vpk");
                if (vpkCandidate.exists()) {
                    vpkPaths.push_back(vpkCandidate);
                }
            }
        }
    }

    // Fallback: if no Game keys matched, check default csgo and core
    if (vpkPaths.empty()) {
        const Core::Path::FilesystemPath csgoVpk = cs2BasePath / QStringLiteral("game/csgo/pak01_dir.vpk");
        if (csgoVpk.exists()) {
            vpkPaths.push_back(csgoVpk);
        }
        const Core::Path::FilesystemPath coreVpk = cs2BasePath / QStringLiteral("game/core/pak01_dir.vpk");
        if (coreVpk.exists()) {
            vpkPaths.push_back(coreVpk);
        }
    }

    return Core::Result<std::vector<Core::Path::FilesystemPath>>::success(std::move(vpkPaths));
}

Core::Result<std::vector<Core::Path::FilesystemPath>> GameInstallationResolver::resolveSource1VpkTargets(
    const QString& gameId,
    const Core::Path::FilesystemPath& gameInfoPathOrDir,
    QString* outCacheCategory)
{
    if (gameInfoPathOrDir.isEmpty() || !gameInfoPathOrDir.isValid()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("GameInstallationResolver", "Game path is invalid or empty"));
    }

    Core::Path::FilesystemPath targetGameInfoPath;
    if (gameInfoPathOrDir.isFile()) {
        targetGameInfoPath = gameInfoPathOrDir;
    } else {
        // Try locating gameinfo.txt
        const Core::Path::FilesystemPath candidate = gameInfoPathOrDir / QStringLiteral("gameinfo.txt");
        if (candidate.exists()) {
            targetGameInfoPath = candidate;
        } else {
            // Check known game definition
            const auto* def = GameRegistry::findById(gameId);
            if (def) {
                targetGameInfoPath = GameValidator::expectedGameInfoPath(gameInfoPathOrDir, def->type);
            }
        }
    }

    if (!targetGameInfoPath.exists()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("GameInstallationResolver", "gameinfo.txt not found at %1").arg(targetGameInfoPath.toString()));
    }

    auto parseRes = GameInfoParser::parse(targetGameInfoPath, EngineType::Source1);
    if (parseRes.isFailure()) {
        return Core::Result<std::vector<Core::Path::FilesystemPath>>::failure(
            parseRes.error(),
            QCoreApplication::translate("GameInstallationResolver", "Failed to parse gameinfo.txt"));
    }

    const auto& searchTargets = parseRes.value().searchTargets();
    std::vector<Core::Path::FilesystemPath> vpkPaths;

    for (const auto& target : searchTargets) {
        if (target.isVpk() && target.path().exists()) {
            vpkPaths.push_back(target.path());
        } else if (target.isDirectory()) {
            const Core::Path::FilesystemPath vpkCandidate = target.path() / QStringLiteral("pak01_dir.vpk");
            if (vpkCandidate.exists()) {
                vpkPaths.push_back(vpkCandidate);
            }
        }
    }

    if (outCacheCategory) {
        *outCacheCategory = resolveIndexCategory(gameId, &parseRes.value());
    }

    return Core::Result<std::vector<Core::Path::FilesystemPath>>::success(std::move(vpkPaths));
}

} // namespace Domain::Game
