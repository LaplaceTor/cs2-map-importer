#include <QCoreApplication>
#include "Application/Soundscape/SoundscapeConvertService.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/Logging/TaskLoggingContext.h"
#include "Workflow/Common/ImportContext.h"
#include "Workflow/Soundscape/SoundscapeConvertWorkflow.h"

namespace Application::Soundscape {

namespace {

Workflow::Soundscape::SoundscapeConvertOptions toWorkflowConvertOptions(
    const SoundscapeConvertOptions& options)
{
    Workflow::Soundscape::SoundscapeConvertOptions wfOpts;
    wfOpts.mixgroup = options.mixgroup;
    return wfOpts;
}

Workflow::Soundscape::SoundscapeWorkflowOptions toWorkflowOptions(
    const ConvertSoundscapeRequest& request)
{
    Workflow::Soundscape::SoundscapeWorkflowOptions wfOpts;
    wfOpts.s1ScriptsDir = request.s1ScriptsDir;
    wfOpts.s2ContentDir = request.s2ContentDir;
    wfOpts.mapName = request.mapName;
    wfOpts.specificSoundscapeFiles = request.specificSoundscapeFiles;
    wfOpts.mixgroup = request.mixgroup;
    return wfOpts;
}

ConvertSoundscapeResult toApplicationResult(
    const Workflow::Soundscape::SoundscapeWorkflowResult& wfResult)
{
    ConvertSoundscapeResult appResult;
    appResult.succeeded = wfResult.succeeded;
    appResult.totalSoundscapesConverted = wfResult.totalSoundscapesConverted;
    appResult.totalSoundEventsGenerated = wfResult.totalSoundEventsGenerated;
    appResult.generatedFiles = wfResult.generatedFiles;
    appResult.uniqueRawSoundAssets = wfResult.uniqueRawSoundAssets;
    appResult.fileStats.reserve(wfResult.fileStats.size());
    for (const auto& stat : wfResult.fileStats) {
        SoundscapeFileStats appStat;
        appStat.sourceFilePath = stat.sourceFilePath;
        appStat.targetFilePath = stat.targetFilePath;
        appStat.soundscapeCount = stat.soundscapeCount;
        appStat.soundEventCount = stat.soundEventCount;
        appStat.referencedAssetCount = stat.referencedAssetCount;
        appResult.fileStats.push_back(std::move(appStat));
    }
    return appResult;
}

Core::Result<ConvertSoundscapeResult> mapWorkflowResult(
    const Core::Result<Workflow::Soundscape::SoundscapeWorkflowResult>& wfRes)
{
    if (wfRes.isSuccess()) {
        return Core::Result<ConvertSoundscapeResult>::success(
            toApplicationResult(wfRes.value()), wfRes.message());
    }
    if (wfRes.isCancelled()) {
        return Core::Result<ConvertSoundscapeResult>::cancelled(wfRes.message());
    }
    if (wfRes.isSkipped()) {
        return Core::Result<ConvertSoundscapeResult>::skipped(wfRes.message());
    }
    return Core::Result<ConvertSoundscapeResult>::failure(wfRes.error(), wfRes.message());
}

} // namespace

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertContent(
    const QString& content,
    const QString& baseName,
    const SoundscapeConvertOptions& options)
{
    return Core::Error::ExecutionGuard::guard<ConvertSoundscapeResult>([&]() {
        return mapWorkflowResult(Workflow::Soundscape::SoundscapeConvertWorkflow::convertContent(
            content, baseName, toWorkflowConvertOptions(options)));
    }, QCoreApplication::translate("SoundscapeConvertService", "Failed to convert soundscape content: %1").arg(baseName));
}

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertFile(
    const Core::Path::FilesystemPath& sourceFile,
    const Core::Path::FilesystemPath& targetFile,
    const SoundscapeConvertOptions& options,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    return Core::Error::ExecutionGuard::guard<ConvertSoundscapeResult>([&]() {
        return mapWorkflowResult(Workflow::Soundscape::SoundscapeConvertWorkflow::convertFile(
            sourceFile, targetFile, toWorkflowConvertOptions(options), expectedBaseDir));
    }, QCoreApplication::translate("SoundscapeConvertService", "Failed to convert soundscape file: %1").arg(sourceFile.toString()));
}

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertMapSoundscapes(
    const ConvertSoundscapeRequest& request,
    const Core::Async::CancellationToken& token)
{
    return Core::Error::ExecutionGuard::guard<ConvertSoundscapeResult>([&]() {
        Workflow::Common::ImportContext context(nullptr, token);
        return mapWorkflowResult(Workflow::Soundscape::SoundscapeConvertWorkflow::execute(
            toWorkflowOptions(request), context));
    }, QCoreApplication::translate("SoundscapeConvertService", "Soundscape conversion failed for map '%1'").arg(request.mapName));
}

Async::TaskHandle SoundscapeConvertService::convertMapSoundscapesAsync(
    const ConvertSoundscapeRequest& request,
    std::function<void(const Core::Result<ConvertSoundscapeResult>&)> callback,
    QObject* context)
{
    const QString mapLabel = request.mapName.isEmpty()
        ? QCoreApplication::translate("SoundscapeConvertService", "All")
        : request.mapName;
    const QString taskName = QCoreApplication::translate("SoundscapeConvertService", "Convert Soundscapes: %1").arg(mapLabel);

    return Async::AsyncTaskRunner::runWorkflowTask<ConvertSoundscapeResult>(
        taskName,
        request.mapName,
        context,
        [request](std::shared_ptr<Core::Logging::TaskLoggingContext> ctx,
                  Core::Async::CancellationToken token) -> Core::Result<ConvertSoundscapeResult> {
            Workflow::Common::ImportContext importCtx(ctx.get(), std::move(token));
            return mapWorkflowResult(Workflow::Soundscape::SoundscapeConvertWorkflow::execute(
                toWorkflowOptions(request), importCtx));
        },
        std::move(callback),
        QThreadPool::globalInstance());
}

} // namespace Application::Soundscape
