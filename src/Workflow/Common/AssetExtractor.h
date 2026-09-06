#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Game/SearchTarget.h"
#include "Workflow/Common/CancellationToken.h"

namespace Workflow::Common {

/**
 * @brief Outcome payload of a successful asset extraction.
 */
struct AssetExtraction {
    /** File path where the extracted asset landed inside the destination directory. */
    Core::Path::FilesystemPath extractedFilePath;
    /** Search target the asset was found in. */
    Core::Path::FilesystemPath sourceTargetPath;
    /** True when the asset came from inside a pack archive rather than a loose folder. */
    bool fromPack = false;
};

struct AssetExtractOptions {
    /**
     * @brief Additional file extensions (without dot) extracted alongside the
     *        asset from the winning target, e.g. model companion files
     *        ("vvd", "phy", ...). Companions are best-effort: missing or
     *        failing companions are logged as warnings, never failures.
     */
    std::vector<QString> companionExtensions;
};

/**
 * @brief Use case: locate an asset by its game-relative path across search
 *        targets and extract it into a destination content directory.
 *
 * Mirrors the legacy FileExtractFromVPK behaviour: directory targets are
 * searched for loose files first (then their pak01_dir.vpk), VPK targets are
 * searched through the pack archive. The first hit wins and companion files
 * are pulled from the winning target only.
 *
 * Result semantics: Success = extracted; Skipped = present in no search
 * target (benign, message names the asset); Failure = I/O or pack errors;
 * Cancelled = token triggered.
 */
class AssetExtractor {
public:
    static Core::Result<AssetExtraction> extract(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeAssetPath,
        const Core::Path::FilesystemPath& destContentDir,
        const AssetExtractOptions& options = {},
        const CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Workflow::Common
