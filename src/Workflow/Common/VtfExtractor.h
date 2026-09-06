#pragma once

#include <vector>

#include <QString>

#include "Core/Logging/TaskLoggingContext.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Game/SearchTarget.h"
#include "Domain/Material/VtfConverter.h"
#include "Workflow/Common/AssetExtractor.h"
#include "Workflow/Common/CancellationToken.h"

namespace Workflow::Common {

/**
 * @brief Use case: locate a VTF texture across search targets, decode it and
 *        write the image into a destination directory using the specified format.
 *
 * The intermediate VTF file is unpacked into a RAII temporary directory that
 * is cleaned up automatically. The returned extraction's extractedFilePath
 * points at the produced image file.
 *
 * Result semantics follow AssetExtractor::extract, with the image encoding step
 * adding OperationFailed as a possible failure reason.
 */
class VtfExtractor {
public:
    static Core::Result<AssetExtraction> extract(
        const std::vector<Domain::Game::SearchTarget>& targets,
        const QString& relativeVtfPath,
        const Core::Path::FilesystemPath& destImageDir,
        Domain::Material::ImageFileFormat targetFormat = Domain::Material::ImageFileFormat::Png,
        const CancellationToken& token = {},
        Core::Logging::TaskLoggingContext* taskCtx = nullptr);
};

} // namespace Workflow::Common
