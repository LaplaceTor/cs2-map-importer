#include "Application/Soundscape/SoundscapeConvertService.h"
#include "Application/Async/AsyncTaskRunner.h"
#include "Application/Execution/ExecutionGuard.h"
#include "Domain/Audio/SoundscapeParser.h"
#include "Domain/Audio/SoundscapeToSoundEventConverter.h"
#include "Domain/Audio/SoundEventKv3Writer.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Path/PathUtils.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace Application::Soundscape {

namespace {

QString deriveOutputFileName(const QString& sourceFileName) {
    const QString fileName = Core::Path::PathUtils::filename(sourceFileName);
    const int dotIdx = fileName.lastIndexOf(QLatin1Char('.'));
    QString base = (dotIdx != -1) ? fileName.left(dotIdx) : fileName;

    if (base.startsWith(QStringLiteral("soundscapes_"), Qt::CaseInsensitive)) {
        base.replace(0, 12, QStringLiteral("soundevents_"));
    } else if (base.startsWith(QStringLiteral("soundscape_"), Qt::CaseInsensitive)) {
        base.replace(0, 11, QStringLiteral("soundevents_"));
    } else if (!base.startsWith(QStringLiteral("soundevents_"), Qt::CaseInsensitive)) {
        base = QStringLiteral("soundevents_") + base;
    }

    return base + QStringLiteral(".vsndevts");
}

} // namespace

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertContent(
    const QString& content,
    const QString& baseName,
    const Domain::Audio::ConversionOptions& options)
{
    return Execution::ExecutionGuard::guard([&]() -> Core::Result<ConvertSoundscapeResult> {
        auto parseRes = Domain::Audio::SoundscapeParser::parseString(content);
        if (!parseRes.isSuccess()) {
            return Core::Result<ConvertSoundscapeResult>::failure(
                parseRes.error(),
                QStringLiteral("Failed to parse soundscape content: %1").arg(baseName));
        }

        const auto& definitions = parseRes.value();
        auto convRes = Domain::Audio::SoundscapeToSoundEventConverter::convertBatch(definitions, options);

        ConvertSoundscapeResult result;
        result.succeeded = true;
        result.totalSoundscapesConverted = static_cast<int>(definitions.size());
        result.totalSoundEventsGenerated = static_cast<int>(convRes.soundEvents.size());

        for (const auto& rawAsset : convRes.uniqueRawSoundAssets) {
            result.uniqueRawSoundAssets.append(rawAsset);
        }
        result.uniqueRawSoundAssets.sort();

        SoundscapeFileStats stats;
        stats.sourceFilePath = baseName;
        stats.soundscapeCount = static_cast<int>(definitions.size());
        stats.soundEventCount = static_cast<int>(convRes.soundEvents.size());
        stats.referencedAssetCount = convRes.uniqueRawSoundAssets.size();
        result.fileStats.push_back(stats);

        return Core::Result<ConvertSoundscapeResult>::success(result);
    }, QStringLiteral("Failed to convert soundscape content: %1").arg(baseName));
}

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertFile(
    const Core::Path::FilesystemPath& sourceFile,
    const Core::Path::FilesystemPath& targetFile,
    const Domain::Audio::ConversionOptions& options)
{
    return Execution::ExecutionGuard::guard([&]() -> Core::Result<ConvertSoundscapeResult> {
        if (!sourceFile.exists()) {
            return Core::Result<ConvertSoundscapeResult>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QStringLiteral("Source soundscape file does not exist"),
                sourceFile.toString());
        }

        auto parseRes = Domain::Audio::SoundscapeParser::parseFile(sourceFile);
        if (!parseRes.isSuccess()) {
            return Core::Result<ConvertSoundscapeResult>::failure(
                parseRes.error(),
                QStringLiteral("Failed to parse soundscape file: %1").arg(sourceFile.toString()));
        }

        const auto& definitions = parseRes.value();
        auto convRes = Domain::Audio::SoundscapeToSoundEventConverter::convertBatch(definitions, options);

        auto writeRes = Domain::Audio::SoundEventKv3Writer::writeToFile(targetFile, convRes.soundEvents);
        if (!writeRes.isSuccess()) {
            return Core::Result<ConvertSoundscapeResult>::failure(
                writeRes.error(),
                QStringLiteral("Failed to write target .vsndevts file: %1").arg(targetFile.toString()));
        }

        ConvertSoundscapeResult result;
        result.succeeded = true;
        result.totalSoundscapesConverted = static_cast<int>(definitions.size());
        result.totalSoundEventsGenerated = static_cast<int>(convRes.soundEvents.size());
        result.generatedFiles.append(targetFile.toString());

        for (const auto& rawAsset : convRes.uniqueRawSoundAssets) {
            result.uniqueRawSoundAssets.append(rawAsset);
        }
        result.uniqueRawSoundAssets.sort();

        SoundscapeFileStats stats;
        stats.sourceFilePath = sourceFile.toString();
        stats.targetFilePath = targetFile.toString();
        stats.soundscapeCount = static_cast<int>(definitions.size());
        stats.soundEventCount = static_cast<int>(convRes.soundEvents.size());
        stats.referencedAssetCount = convRes.uniqueRawSoundAssets.size();
        result.fileStats.push_back(stats);

        return Core::Result<ConvertSoundscapeResult>::success(result);
    }, QStringLiteral("Failed to convert soundscape file: %1").arg(sourceFile.toString()));
}

Core::Result<ConvertSoundscapeResult> SoundscapeConvertService::convertMapSoundscapes(
    const ConvertSoundscapeRequest& request,
    Core::Logging::TaskLoggingContext* loggingCtx)
{
    return Execution::ExecutionGuard::guard([&]() -> Core::Result<ConvertSoundscapeResult> {
        if (loggingCtx) {
            loggingCtx->info(QStringLiteral("Starting soundscape conversion for map '%1'").arg(request.mapName));
        }

        std::vector<Core::Path::FilesystemPath> candidateFiles;

        if (!request.specificSoundscapeFiles.empty()) {
            candidateFiles = request.specificSoundscapeFiles;
        } else {
            if (!Core::FileSystem::FileSystem::exists(request.s1ScriptsDir.toString())) {
                if (loggingCtx) {
                    loggingCtx->warning(QStringLiteral("Scripts directory not found: %1").arg(request.s1ScriptsDir.toString()));
                }
                return Core::Result<ConvertSoundscapeResult>::skipped(QStringLiteral("No scripts directory found to convert"));
            }

            const QString scriptsDirPath = request.s1ScriptsDir.toString();
            QDir scriptsDir(scriptsDirPath);

            // Filter for soundscape files
            QStringList filters;
            filters << QStringLiteral("soundscapes_*.txt")
                    << QStringLiteral("soundscapes_*.vsc")
                    << QStringLiteral("soundscapes.txt")
                    << QStringLiteral("soundscapes.vsc");

            const QFileInfoList entries = scriptsDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
            for (const auto& entry : entries) {
                const QString fn = entry.fileName().toLower();
                if (fn == QStringLiteral("soundscapes_manifest.txt") || fn == QStringLiteral("soundscaples_manifest.txt")) {
                    continue;
                }
                candidateFiles.push_back(Core::Path::FilesystemPath(entry.absoluteFilePath()));
            }
        }

        if (candidateFiles.empty()) {
            if (loggingCtx) {
                loggingCtx->info(QStringLiteral("No soundscape files found to convert for map '%1'").arg(request.mapName));
            }
            return Core::Result<ConvertSoundscapeResult>::skipped(QStringLiteral("No soundscape files found"));
        }

        Core::Path::FilesystemPath targetSoundeventsDir = request.s2ContentDir.isEmpty()
            ? Core::Path::FilesystemPath(QStringLiteral("soundevents"))
            : request.s2ContentDir / QStringLiteral("soundevents");

        Core::FileSystem::FileSystem::createDirectory(targetSoundeventsDir.toString());

        Domain::Audio::ConversionOptions options;
        if (!request.mixgroup.isEmpty()) {
            options.mixgroup = request.mixgroup;
        } else if (!request.mapName.isEmpty()) {
            options.mixgroup = request.mapName;
        }

        ConvertSoundscapeResult aggregatedResult;
        aggregatedResult.succeeded = true;
        QSet<QString> uniqueAssets;

        for (const auto& sourceFile : candidateFiles) {
            if (loggingCtx && loggingCtx->state() == Core::Logging::TaskState::Cancelled) {
                loggingCtx->info(QStringLiteral("Soundscape conversion cancelled by user"));
                return Core::Result<ConvertSoundscapeResult>::cancelled();
            }

            const QString sourceFileName = Core::Path::PathUtils::filename(sourceFile.toString());
            const QString outputFileName = deriveOutputFileName(sourceFileName);
            const Core::Path::FilesystemPath targetFile = targetSoundeventsDir / outputFileName;

            if (loggingCtx) {
                loggingCtx->info(QStringLiteral("Converting %1 -> %2").arg(sourceFileName, outputFileName));
            }

            auto fileRes = convertFile(sourceFile, targetFile, options);
            if (!fileRes.isSuccess()) {
                if (loggingCtx) {
                    loggingCtx->error(QStringLiteral("Failed to convert %1: %2").arg(sourceFileName, fileRes.message()));
                }
                return fileRes;
            }

            const auto& fResult = fileRes.value();
            aggregatedResult.totalSoundscapesConverted += fResult.totalSoundscapesConverted;
            aggregatedResult.totalSoundEventsGenerated += fResult.totalSoundEventsGenerated;
            aggregatedResult.generatedFiles.append(fResult.generatedFiles);

            for (const auto& stat : fResult.fileStats) {
                aggregatedResult.fileStats.push_back(stat);
            }
            for (const auto& raw : fResult.uniqueRawSoundAssets) {
                uniqueAssets.insert(raw);
            }
        }

        for (const auto& asset : uniqueAssets) {
            aggregatedResult.uniqueRawSoundAssets.append(asset);
        }
        aggregatedResult.uniqueRawSoundAssets.sort();

        if (loggingCtx) {
            loggingCtx->info(QStringLiteral("Soundscape conversion completed: %1 soundscapes converted, %2 soundevents generated, %3 files written, %4 unique raw sound assets referenced.")
                .arg(aggregatedResult.totalSoundscapesConverted)
                .arg(aggregatedResult.totalSoundEventsGenerated)
                .arg(aggregatedResult.generatedFiles.size())
                .arg(aggregatedResult.uniqueRawSoundAssets.size()));
        }

        return Core::Result<ConvertSoundscapeResult>::success(
            aggregatedResult,
            QStringLiteral("Soundscape conversion completed successfully"));
    }, QStringLiteral("Soundscape conversion failed for map '%1'").arg(request.mapName));
}

void SoundscapeConvertService::convertMapSoundscapesAsync(
    const ConvertSoundscapeRequest& request,
    Core::Logging::TaskLoggingContext* loggingCtx,
    std::function<void(const Core::Result<ConvertSoundscapeResult>&)> callback)
{
    const QString taskName = QStringLiteral("Convert Soundscapes: %1").arg(request.mapName.isEmpty() ? QStringLiteral("All") : request.mapName);
    quint64 parentId = loggingCtx ? loggingCtx->taskId() : 0;

    Async::AsyncTaskRunner::runTask<ConvertSoundscapeResult>(
        taskName,
        nullptr,
        [this, request](std::shared_ptr<Core::Logging::TaskLoggingContext> ctx) -> Core::Result<ConvertSoundscapeResult> {
            return convertMapSoundscapes(request, ctx.get());
        },
        std::move(callback),
        QThreadPool::globalInstance(),
        parentId);
}

} // namespace Application::Soundscape
