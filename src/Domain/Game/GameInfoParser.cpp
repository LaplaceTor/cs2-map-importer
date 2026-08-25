#include "Domain/Game/GameInfoParser.h"
#include "Domain/Game/SearchPathResolver.h"
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
        if (const auto* sp = gameInfoNode->findChild(QStringLiteral("SearchPaths"))) {
            return sp;
        }
        if (const auto* fs = gameInfoNode->findChild(QStringLiteral("FileSystem"))) {
            if (const auto* sp = fs->findChild(QStringLiteral("SearchPaths"))) {
                return sp;
            }
        }
    }
    if (const auto* sp = rootNode.findChild(QStringLiteral("SearchPaths"))) {
        return sp;
    }
    if (const auto* fs = rootNode.findChild(QStringLiteral("FileSystem"))) {
        if (const auto* sp = fs->findChild(QStringLiteral("SearchPaths"))) {
            return sp;
        }
    }
    return nullptr;
}

Core::Path::FilesystemPath GameInfoParser::resolveBaseDirectory(
    const Core::Path::FilesystemPath& modDirectory,
    const Core::KeyValues::KeyValuesNode& rootNode,
    EngineType engine)
{
    const QString modDirPath = modDirectory.toString();
    if (modDirPath.isEmpty()) {
        return Core::Path::FilesystemPath();
    }

    if (engine == EngineType::Source2) {
        // Source 2 layout: mod is in <gameRoot>/game/<modName>
        if (modDirectory.parentPath().fileName().compare(QStringLiteral("game"), Qt::CaseInsensitive) == 0) {
            return modDirectory.parentPath().parentPath();
        }
        return modDirectory.parentPath();
    }

    // Source 1 layout
    const auto* gameInfoNode = rootNode.findChild(QStringLiteral("GameInfo"));
    const auto* fileSystemNode = gameInfoNode ? gameInfoNode->findChild(QStringLiteral("FileSystem"))
                                              : rootNode.findChild(QStringLiteral("FileSystem"));
    const auto* searchPathsNode = findSearchPathsNode(rootNode, gameInfoNode, fileSystemNode);

    if (searchPathsNode) {
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

std::optional<GameInfo> GameInfoParser::parse(
    const Core::Path::FilesystemPath& gameInfoPath,
    EngineType engine,
    QString* errorMessage)
{
    if (!gameInfoPath.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid gameinfo path provided.");
        }
        return std::nullopt;
    }

    Core::KeyValues::KeyValuesDocument doc;
    if (!doc.loadFromFile(gameInfoPath, errorMessage)) {
        return std::nullopt;
    }

    return createFromDocument(std::move(doc), gameInfoPath, engine);
}

std::optional<GameInfo> GameInfoParser::parse(
    const Core::Path::FilesystemPath& gameInfoPath,
    QString* errorMessage)
{
    return parse(gameInfoPath, EngineType::Source1, errorMessage);
}

std::optional<GameInfo> GameInfoParser::parseFromString(
    const QString& content,
    const Core::Path::FilesystemPath& gameInfoPath,
    EngineType engine,
    QString* errorMessage)
{
    Core::KeyValues::KeyValuesDocument doc;
    if (!doc.loadFromString(content, errorMessage)) {
        return std::nullopt;
    }

    return createFromDocument(std::move(doc), gameInfoPath, engine);
}

std::optional<GameInfo> GameInfoParser::parseFromString(
    const QString& content,
    const Core::Path::FilesystemPath& gameInfoPath,
    QString* errorMessage)
{
    return parseFromString(content, gameInfoPath, EngineType::Source1, errorMessage);
}

} // namespace Domain::Game

