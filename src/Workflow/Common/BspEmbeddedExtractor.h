#pragma once

#include <cstddef>

#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Workflow/Common/CancellationToken.h"

namespace Workflow::Common {

/**
 * @brief Use case: extract all files embedded in a BSP pack lump into a
 *        destination directory, preserving the pack-relative structure.
 *
 * Replaces the legacy vpkeditcli `--file-tree` ASCII parsing: the BSP's
 * embedded pack is enumerated directly via bsppp/vpkpp. Individual entry
 * failures are logged as warnings and do not abort the extraction.
 *
 * Result semantics: Success = extraction finished (payload = extracted file
 * count); Skipped = the BSP contains no embedded files; Failure = the BSP
 * could not be opened or the destination is invalid; Cancelled = token
 * triggered mid-extraction (payload = partial count).
 */
class BspEmbeddedExtractor {
public:
    static Core::Result<std::size_t> extract(
        const Core::Path::FilesystemPath& bspPath,
        const Core::Path::FilesystemPath& destDir,
        const CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Workflow::Common
