#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace Application::Environment {

/**
 * @brief Plain UI-facing Application Contract DTO.
 *
 * Exposes only Qt / primitive value types so UI ViewModels never need to depend on or inspect
 * Domain/Core implementation types like FilesystemPath, GameType, or GameInfo.
 */
struct GameInstallationInfo {
    QString gameId;
    QString displayName;
    QString gameTitle;
    QString basePath;
    QString gameInfoPath;
    bool isValid = false;
    bool isSource2 = false;
};

/**
 * @brief Plain UI-facing detection result DTO.
 */
struct DetectionResult {
    std::vector<GameInstallationInfo> installations;
    QStringList warnings;
};

} // namespace Application::Environment
