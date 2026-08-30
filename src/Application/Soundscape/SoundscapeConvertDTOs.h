#pragma once

#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <QStringList>
#include <vector>

namespace Application::Soundscape {

struct SoundscapeFileStats {
    QString sourceFilePath;
    QString targetFilePath;
    int soundscapeCount = 0;
    int soundEventCount = 0;
    int referencedAssetCount = 0;
};

struct ConvertSoundscapeRequest {
    Core::Path::FilesystemPath s1ScriptsDir;
    Core::Path::FilesystemPath s2ContentDir;
    QString mapName;
    std::vector<Core::Path::FilesystemPath> specificSoundscapeFiles;
    QString mixgroup;
};

struct ConvertSoundscapeResult {
    bool succeeded = false;
    int totalSoundscapesConverted = 0;
    int totalSoundEventsGenerated = 0;
    QStringList generatedFiles;
    QStringList uniqueRawSoundAssets;
    std::vector<SoundscapeFileStats> fileStats;
};

} // namespace Application::Soundscape

