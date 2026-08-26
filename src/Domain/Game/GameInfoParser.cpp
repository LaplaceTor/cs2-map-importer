#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/GameError.h"
#include "Domain/Game/SearchPathResolver.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include <QDir>
#include <QFileInfo>
#include <utility>

namespace Domain::Game {

const Core::KeyValues::KeyValuesNode* GameInfoParser::findSearchPathsNode(
    const Core::KeyValues::KeyValuesNode& rootNode,
    const Core::KeyValues::KeyValuesNode* gameInfoNode,
    const Core::KeyValues::KeyValuesNode* fileSystemNode)
{
    if (fileSystemNode) {
        if (const auto* sp = fileSystemNode->findChild(QStringLiteral("SearchPaths"))) {
            return sp;
        }
    }

    if (gameInfoNode) {
        if (const auto* fs = gameInfoNode->findChild(QStringLiteral("FileSystem"))) {
            if (const auto* sp = fs->findChild(QStringLiteral("SearchPaths"))) {
                return sp;
            }
        }
        if (const auto* sp = gameInfoNode->findChild(QStringLiteral("SearchPaths"))) {
            return sp;
        }
    }

    if (const auto* fs = rootNode.findChild(QStringLiteral("FileSystem"))) {
        if (const auto* sp = fs->findChild(QStringLiteral("SearchPaths"))) {
            return sp;
        }
    }

    return rootNode.findChild(QStringLiteral("SearchPaths"));
}

Core::Path::FilesystemPath GameInfoParser::resolveBaseDirectory(
    const Core::Path::FilesystemPath& modDirectory,
    const Core::KeyValues::KeyValuesNode& rootNode,
    EngineType engine)
{
    if (!modDirectory.isValid() || modDirectory.isEmpty()) {
        return Core::Path::FilesystemPath();
    }

    if (engine == EngineType::Source2) {
        const QString modDirPath = QDir::fromNativeSeparators(modDirectory.toString());
        const QString suffix = QStringLiteral("/game/");
        const int index = modDirPath.lastIndexOf(suffix, -1, Qt::CaseInsensitive);
        if (index != -1) {
            return Core::Path::FilesystemPath(modDirPath.left(index));
        }
        return modDirectory.parentPath();
    }

    const auto* gameInfoNode = rootNode.findChild(QStringLiteral("GameInfo"));
    const auto* fileSystemNode = gameInfoNode ? gameInfoNode->findChild(QStringLiteral("FileSystem"))
                                              : rootNode.findChild(QStringLiteral("FileSystem"));
    const auto* searchPathsNode = findSearchPathsNode(rootNode, gameInfoNode, fileSystemNode);

    if (searchPathsNode) {
        const QString modDirPath = QDir::fromNativeSeparators(modDirectory.toString());
        for (const auto& child : searchPathsNode->children()) {
            if (child.name().compare(QStringLiteral("game+game_write"), Qt::CaseInsensitive) == 0) {
                const QString val = child.value().trimmed();
                if (!val.isEmpty()) {
                    const QString suffix = QDir::fromNativeSeparators(val);
                    const QString suffixWithSlash = QLatin1Char('/') + suffix;

                    if (modDirPath.endsWith(suffixWithSlash, Qt::CaseInsensitive)) {
                        return Core::Path::FilesystemPath(modDirPath.left(modDirPath.size() - suffixWithSlash.size()));
                    }
                    if (modDirPath.endsWith(suffix, Qt::CaseInsensitive)) {
                        return Core::Path::FilesystemPath(modDirPath.left(modDirPath.size() - suffix.size()));
                    }
                }
                break;
            }
        }
    }

    return modDirectory.parentPath();
}

GameInfo GameInfoParser::createFromDocument(
    Core::KeyValues::KeyValuesDocument doc,
    const Core::Path::FilesystemPath& gameInfoPath,
    EngineType engine)
{
    const Core::Path::FilesystemPath modDirectory = gameInfoPath.isValid() ? gameInfoPath.parentPath()
                                                                           : Core::Path::FilesystemPath();
    const Core::KeyValues::KeyValuesNode& rootNode = doc.root();

    const auto* gameInfoNode = rootNode.findChild(QStringLiteral("GameInfo"));
    const auto* fileSystemNode = gameInfoNode ? gameInfoNode->findChild(QStringLiteral("FileSystem"))
                                              : rootNode.findChild(QStringLiteral("FileSystem"));

    GameInfo info;
    info.setGameInfoPath(gameInfoPath);
    info.setModDirectory(modDirectory);

    if (gameInfoNode) {
        info.setGame(gameInfoNode->property(QStringLiteral("game")));
        info.setTitle(gameInfoNode->property(QStringLiteral("title")));
    } else {
        info.setGame(rootNode.property(QStringLiteral("game")));
        info.setTitle(rootNode.property(QStringLiteral("title")));
    }

    if (fileSystemNode) {
        info.setSteamAppId(fileSystemNode->propertyInt(QStringLiteral("SteamAppId")));
        info.setToolsAppId(fileSystemNode->propertyInt(QStringLiteral("ToolsAppId")));
    }

    const Core::Path::FilesystemPath baseDirectory = resolveBaseDirectory(modDirectory, rootNode, engine);
    info.setBaseDirectory(baseDirectory);

    const auto* searchPathsNode = findSearchPathsNode(rootNode, gameInfoNode, fileSystemNode);
    auto searchTargets = SearchPathResolver::resolve(modDirectory, baseDirectory, searchPathsNode);
    info.setSearchTargets(std::move(searchTargets));

    info.setDocument(std::move(doc));
    return info;
}

Core::Async::TaskResult<GameInfo> GameInfoParser::parse(
    const Core::Path::FilesystemPath& gameInfoPath,
    EngineType engine)
{
    if (!gameInfoPath.isValid() || gameInfoPath.isEmpty()) {
        return Core::Async::TaskResult<GameInfo>::failure(
            GameError::gameInfoNotFound(
                QStringLiteral("GameInfo file path is invalid or empty"),
                gameInfoPath.toString()),
            QStringLiteral("GameInfo parsing failed"));
    }
    if (!gameInfoPath.exists()) {
        return Core::Async::TaskResult<GameInfo>::failure(
            GameError::gameInfoNotFound(
                QStringLiteral("GameInfo file does not exist"),
                gameInfoPath.toString()),
            QStringLiteral("GameInfo parsing failed"));
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadResult = doc.loadFromFile(gameInfoPath);
    if (!loadResult.isSuccess()) {
        return Core::Async::TaskResult<GameInfo>::failure(
            loadResult.error(),
            QStringLiteral("Failed to load GameInfo"));
    }

    return Core::Async::TaskResult<GameInfo>::success(createFromDocument(std::move(doc), gameInfoPath, engine));
}

Core::Async::TaskResult<GameInfo> GameInfoParser::parseFromString(
    const QString& content,
    const Core::Path::FilesystemPath& gameInfoPath,
    EngineType engine)
{
    if (!gameInfoPath.isEmpty() && !gameInfoPath.isValid()) {
        return Core::Async::TaskResult<GameInfo>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("GameInfo path hint is invalid"),
            gameInfoPath.toString());
    }

    Core::KeyValues::KeyValuesDocument doc;
    auto loadResult = doc.loadFromString(content);
    if (!loadResult.isSuccess()) {
        return Core::Async::TaskResult<GameInfo>::failure(
            loadResult.error(),
            QStringLiteral("Failed to parse GameInfo content"));
    }

    return Core::Async::TaskResult<GameInfo>::success(createFromDocument(std::move(doc), gameInfoPath, engine));
}

} // namespace Domain::Game
