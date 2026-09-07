#pragma once

#include <cstddef>
#include <functional>

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"

namespace Domain::Package {

struct BspExtractOptions {
    std::function<bool()> isCancelled;
    std::function<void(std::size_t current, std::size_t total)> onProgress;
};

class BspPackExtractor {
public:
    /**
     * @brief High-performance in-memory multi-threaded extraction of all files
     *        embedded in a BSP's PAKFILE lump.
     *
     * Result semantics:
     * - Success: All files extracted (payload = extracted file count).
     * - Skipped: BSP has no PAKFILE lump or it contains 0 files.
     * - Cancelled: Extraction was cancelled mid-flight via isCancelled (payload = partial count).
     * - Failure: Missing file, invalid path, or corrupted lump.
     */
    static Core::Result<std::size_t> extractAll(
        const Core::Path::FilesystemPath& bspPath,
        const Core::Path::FilesystemPath& destDir,
        const BspExtractOptions& options = {});
};

} // namespace Domain::Package

