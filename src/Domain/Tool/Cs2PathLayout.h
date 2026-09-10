#pragma once

#include "Core/Path/FilesystemPath.h"
#include <QString>

namespace Domain::Tool {

/**
 * @brief Standard path layout resolver for Counter-Strike 2 engine tools and directories.
 */
struct Cs2PathLayout {
    static inline Core::Path::FilesystemPath source1ImportExecutable(const Core::Path::FilesystemPath& cs2BaseDir) {
        return cs2BaseDir / QStringLiteral("game/bin/win64/source1import.exe");
    }

    static inline Core::Path::FilesystemPath resourceCompilerExecutable(const Core::Path::FilesystemPath& cs2BaseDir) {
        return cs2BaseDir / QStringLiteral("game/bin/win64/resourcecompiler.exe");
    }

    static inline Core::Path::FilesystemPath gameDirectory(const Core::Path::FilesystemPath& cs2BaseDir) {
        return cs2BaseDir / QStringLiteral("game/csgo");
    }
};

inline Core::Path::FilesystemPath source1ImportExecutable(const Core::Path::FilesystemPath& cs2BaseDir) {
    return Cs2PathLayout::source1ImportExecutable(cs2BaseDir);
}

inline Core::Path::FilesystemPath resourceCompilerExecutable(const Core::Path::FilesystemPath& cs2BaseDir) {
    return Cs2PathLayout::resourceCompilerExecutable(cs2BaseDir);
}

inline Core::Path::FilesystemPath gameDirectory(const Core::Path::FilesystemPath& cs2BaseDir) {
    return Cs2PathLayout::gameDirectory(cs2BaseDir);
}

} // namespace Domain::Tool
