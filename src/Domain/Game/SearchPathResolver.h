#pragma once

#include "Domain/Game/SearchTarget.h"
#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/Path/FilesystemPath.h"
#include <vector>

namespace Domain::Game {

class SearchPathResolver {
public:
    static std::vector<SearchTarget> resolve(
        const Core::Path::FilesystemPath& modDirectory,
        const Core::Path::FilesystemPath& baseDirectory,
        const Core::KeyValues::KeyValuesNode* searchPathsNode);
};

} // namespace Domain::Game

