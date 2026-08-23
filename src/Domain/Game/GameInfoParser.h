#pragma once

#include "Domain/Game/GameInfo.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include <optional>
#include <QString>

namespace Domain::Game {

class GameInfoParser {
public:
    static std::optional<GameInfo> parse(
        const Core::Path::FilesystemPath& gameInfoPath,
        QString* errorMessage = nullptr);

    static std::optional<GameInfo> parseFromString(
        const QString& content,
        const Core::Path::FilesystemPath& gameInfoPath = Core::Path::FilesystemPath(),
        QString* errorMessage = nullptr);

    static Core::Path::FilesystemPath resolveBaseDirectory(
        const Core::Path::FilesystemPath& modDirectory,
        const Core::KeyValues::KeyValuesNode& rootNode);

private:
    static GameInfo createFromDocument(
        Core::KeyValues::KeyValuesDocument doc,
        const Core::Path::FilesystemPath& gameInfoPath);

    static const Core::KeyValues::KeyValuesNode* findSearchPathsNode(
        const Core::KeyValues::KeyValuesNode& rootNode,
        const Core::KeyValues::KeyValuesNode* gameInfoNode,
        const Core::KeyValues::KeyValuesNode* fileSystemNode);
};

} // namespace Domain::Game

