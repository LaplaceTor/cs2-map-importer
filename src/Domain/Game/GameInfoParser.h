#pragma once

#include "Domain/Game/GameInfo.h"
#include "Domain/Game/GameType.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Result/Result.h"
#include <QString>

namespace Domain::Game {

class GameInfoParser {
public:
    static Core::Result<GameInfo> parse(
        const Core::Path::FilesystemPath& gameInfoPath,
        EngineType engine = EngineType::Source1);

    static Core::Result<GameInfo> parseFromString(
        const QString& content,
        const Core::Path::FilesystemPath& gameInfoPath = Core::Path::FilesystemPath(),
        EngineType engine = EngineType::Source1);

    static Core::Path::FilesystemPath resolveBaseDirectory(
        const Core::Path::FilesystemPath& modDirectory,
        const Core::KeyValues::KeyValuesNode& rootNode,
        EngineType engine = EngineType::Source1);

private:
    static GameInfo createFromDocument(
        Core::KeyValues::KeyValuesDocument doc,
        const Core::Path::FilesystemPath& gameInfoPath,
        EngineType engine = EngineType::Source1);

    static const Core::KeyValues::KeyValuesNode* findSearchPathsNode(
        const Core::KeyValues::KeyValuesNode& rootNode,
        const Core::KeyValues::KeyValuesNode* gameInfoNode,
        const Core::KeyValues::KeyValuesNode* fileSystemNode);
};

} // namespace Domain::Game
