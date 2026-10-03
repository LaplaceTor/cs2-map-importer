#pragma once

#include "Application/Async/TaskHandle.h"
#include "Application/Soundscape/SoundscapeConvertDTOs.h"
#include "Core/Async/CancellationToken.h"
#include "Core/Result/Result.h"
#include <functional>

class QObject;

namespace Application::Soundscape {

class SoundscapeConvertService {
public:
    SoundscapeConvertService() = default;

    /**
     * @brief Converts in-memory soundscape script content and returns generated soundevents and asset references.
     */
    static Core::Result<ConvertSoundscapeResult> convertContent(const QString& content,
                                                               const QString& baseName,
                                                               const SoundscapeConvertOptions& options = {});

    /**
     * @brief Converts a single soundscape script file and writes the resulting .vsndevts file to targetPath.
     */
    static Core::Result<ConvertSoundscapeResult> convertFile(
        const Core::Path::FilesystemPath& sourceFile,
        const Core::Path::FilesystemPath& targetFile,
        const SoundscapeConvertOptions& options = {},
        const Core::Path::FilesystemPath& expectedBaseDir = {});

    /**
     * @brief Discovers and converts all soundscape files for a map based on the request.
     */
    static Core::Result<ConvertSoundscapeResult> convertMapSoundscapes(
        const ConvertSoundscapeRequest& request,
        const Core::Async::CancellationToken& token = {});

    /**
     * @brief Asynchronously converts all soundscapes for a map as a workflow task
     * (dedicated workflow log directory + visible UI task tree entry).
     */
    static Async::TaskHandle convertMapSoundscapesAsync(
        const ConvertSoundscapeRequest& request,
        std::function<void(const Core::Result<ConvertSoundscapeResult>&)> callback = {},
        QObject* context = nullptr);
};

} // namespace Application::Soundscape

