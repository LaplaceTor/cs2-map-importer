#pragma once

#include "Application/Soundscape/SoundscapeConvertDTOs.h"
#include "Domain/Audio/SoundscapeToSoundEventConverter.h"
#include "Core/Result/Result.h"
#include "Core/Logging/TaskLoggingContext.h"
#include <functional>

namespace Application::Soundscape {

class SoundscapeConvertService {
public:
    SoundscapeConvertService() = default;

    /**
     * @brief Converts in-memory soundscape script content and returns generated soundevents and asset references.
     */
    Core::Result<ConvertSoundscapeResult> convertContent(const QString& content,
                                                       const QString& baseName,
                                                       const Domain::Audio::ConversionOptions& options = {});

    /**
     * @brief Converts a single soundscape script file and writes the resulting .vsndevts file to targetPath.
     */
    Core::Result<ConvertSoundscapeResult> convertFile(const Core::Path::FilesystemPath& sourceFile,
                                                    const Core::Path::FilesystemPath& targetFile,
                                                    const Domain::Audio::ConversionOptions& options = {});

    /**
     * @brief Discovers and converts all soundscape files for a map based on the request.
     */
    Core::Result<ConvertSoundscapeResult> convertMapSoundscapes(const ConvertSoundscapeRequest& request,
                                                              Core::Logging::TaskLoggingContext* loggingCtx = nullptr);

    /**
     * @brief Asynchronously converts all soundscapes for a map using AsyncTaskRunner.
     */
    void convertMapSoundscapesAsync(const ConvertSoundscapeRequest& request,
                                   Core::Logging::TaskLoggingContext* loggingCtx,
                                   std::function<void(const Core::Result<ConvertSoundscapeResult>&)> callback);
};

} // namespace Application::Soundscape

