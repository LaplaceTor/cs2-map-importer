#pragma once

#include <vector>
#include <QString>
#include <QStringList>

#include "Core/Path/FilesystemPath.h"

namespace Workflow::Soundscape {

struct SoundscapeConvertOptions {
    QString mixgroup;

    bool operator==(const SoundscapeConvertOptions& other) const = default;
};

struct SoundscapeFileStats {
    QString sourceFilePath;
    QString targetFilePath;
    int soundscapeCount = 0;
    int soundEventCount = 0;
    int referencedAssetCount = 0;

    bool operator==(const SoundscapeFileStats& other) const = default;
};

struct SoundscapeWorkflowOptions {
    Core::Path::FilesystemPath s1ScriptsDir;
    Core::Path::FilesystemPath s2ContentDir;
    QString mapName;
    std::vector<Core::Path::FilesystemPath> specificSoundscapeFiles;
    QString mixgroup;

    bool operator==(const SoundscapeWorkflowOptions& other) const = default;
};

struct SoundscapeWorkflowResult {
    bool succeeded = false;
    int totalSoundscapesConverted = 0;
    int totalSoundEventsGenerated = 0;
    QStringList generatedFiles;
    QStringList uniqueRawSoundAssets;
    std::vector<SoundscapeFileStats> fileStats;

    bool operator==(const SoundscapeWorkflowResult& other) const = default;
};

} // namespace Workflow::Soundscape
