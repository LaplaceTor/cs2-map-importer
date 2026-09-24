#pragma once

#include <memory>
#include <QString>
#include "Core/Path/FilesystemPath.h"

namespace Domain::Package {
class VpkIndex;
}

namespace Application::Common {

/**
 * @brief Base request DTO containing common environment parameters for asset imports (Map, Model, Particle).
 */
struct BaseImportRequest {
    QString source1GameDir;
    QString cs2BaseDir;
    QString addonName;
    QString s1GameInfoDir;
    QString gameId;

    bool operator==(const BaseImportRequest& other) const = default;
};

/**
 * @brief Verified and trimmed environment parameters ready for workflow options mapping.
 *
 * Tool paths (source1import.exe, resourcecompiler.exe) are automatically derived from cs2BaseDir.
 */
struct ValidatedBaseImport {
    QString source1GameDir;
    QString s1GameInfoDir;
    QString cs2BaseDir;
    QString addonName;
    Core::Path::FilesystemPath source1ImportExe;
    Core::Path::FilesystemPath resourceCompilerExe;
    std::shared_ptr<const Domain::Package::VpkIndex> vpkIndex;
    std::shared_ptr<const Domain::Package::VpkIndex> cs2Index;
};

} // namespace Application::Common
