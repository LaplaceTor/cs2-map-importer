#pragma once

#include <QString>

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Soundscape/SoundscapeConvertOptions.h"

namespace Workflow::Soundscape {

/**
 * @brief Workflow orchestrating Source 1 soundscape parsing, conversion to CS2 soundevents,
 *        and KV3 .vsndevts output for single scripts or map-wide batches.
 */
class SoundscapeConvertWorkflow {
public:
    /**
     * @brief Converts in-memory soundscape script content and returns generated statistics and asset references.
     */
    static Core::Result<SoundscapeWorkflowResult> convertContent(
        const QString& content,
        const QString& baseName,
        const SoundscapeConvertOptions& options = {});

    /**
     * @brief Converts a single soundscape script file and writes the resulting .vsndevts file to targetFile.
     */
    static Core::Result<SoundscapeWorkflowResult> convertFile(
        const Core::Path::FilesystemPath& sourceFile,
        const Core::Path::FilesystemPath& targetFile,
        const SoundscapeConvertOptions& options = {},
        const Core::Path::FilesystemPath& expectedBaseDir = {});

    /**
     * @brief Discovers and converts all soundscape files for a map according to the workflow options.
     */
    static Core::Result<SoundscapeWorkflowResult> execute(
        const SoundscapeWorkflowOptions& options,
        const Workflow::Common::ImportContext& context = Workflow::Common::ImportContext{});
};

} // namespace Workflow::Soundscape
