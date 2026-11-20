#include "Workflow/Soundscape/SoundscapeConvertWorkflow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Path/PathUtils.h"
#include "Domain/Audio/SoundEventKv3Writer.h"
#include "Domain/Audio/SoundscapeParser.h"
#include "Domain/Audio/SoundscapeToSoundEventConverter.h"

namespace Workflow::Soundscape {

namespace {

Domain::Audio::ConversionOptions toDomainOptions(const SoundscapeConvertOptions& options) {
    Domain::Audio::ConversionOptions domainOpts;
    domainOpts.mixgroup = options.mixgroup;
    return domainOpts;
}

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

Core::Result<SoundscapeWorkflowResult> SoundscapeConvertWorkflow::convertContent(
    const QString& content,
    const QString& baseName,
    const SoundscapeConvertOptions& options)
{
    auto parseRes = Domain::Audio::SoundscapeParser::parseString(content);
    if (!parseRes.isSuccess()) {
        return Core::Result<SoundscapeWorkflowResult>::failure(
            parseRes.error(),
            QCoreApplication::translate("SoundscapeConvertWorkflow", "Failed to parse soundscape content: %1").arg(baseName));
    }

    const auto& definitions = parseRes.value();
    auto convRes = Domain::Audio::SoundscapeToSoundEventConverter::convertBatch(definitions, toDomainOptions(options));

    SoundscapeWorkflowResult result;
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

    return Core::Result<SoundscapeWorkflowResult>::success(std::move(result));
}

Core::Result<SoundscapeWorkflowResult> SoundscapeConvertWorkflow::convertFile(
    const Core::Path::FilesystemPath& sourceFile,
    const Core::Path::FilesystemPath& targetFile,
    const SoundscapeConvertOptions& options,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    if (!sourceFile.exists()) {
        return Core::Result<SoundscapeWorkflowResult>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("SoundscapeConvertWorkflow", "Source soundscape file does not exist"),
            sourceFile.toString());
    }

    auto parseRes = Domain::Audio::SoundscapeParser::parseFile(sourceFile);
    if (!parseRes.isSuccess()) {
        return Core::Result<SoundscapeWorkflowResult>::failure(
            parseRes.error(),
            QCoreApplication::translate("SoundscapeConvertWorkflow", "Failed to parse soundscape file: %1").arg(sourceFile.toString()));
    }

    const auto& definitions = parseRes.value();
    auto convRes = Domain::Audio::SoundscapeToSoundEventConverter::convertBatch(definitions, toDomainOptions(options));

    auto writeRes = Domain::Audio::SoundEventKv3Writer::writeToFile(targetFile, convRes.soundEvents, expectedBaseDir);
    if (!writeRes.isSuccess()) {
        return Core::Result<SoundscapeWorkflowResult>::failure(
            writeRes.error(),
            QCoreApplication::translate("SoundscapeConvertWorkflow", "Failed to write target .vsndevts file: %1").arg(targetFile.toString()));
    }

    SoundscapeWorkflowResult result;
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

    return Core::Result<SoundscapeWorkflowResult>::success(std::move(result));
}

Core::Result<SoundscapeWorkflowResult> SoundscapeConvertWorkflow::execute(
    const SoundscapeWorkflowOptions& options,
    const Workflow::Common::ImportContext& context)
{
    if (context.isCancelled()) {
        context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "Soundscape conversion cancelled by user"));
        return Core::Result<SoundscapeWorkflowResult>::cancelled();
    }

    context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "Starting soundscape conversion for map '%1'").arg(options.mapName));

    std::vector<Core::Path::FilesystemPath> candidateFiles;

    if (!options.specificSoundscapeFiles.empty()) {
        candidateFiles = options.specificSoundscapeFiles;
    } else {
        if (!Core::FileSystem::FileSystem::exists(options.s1ScriptsDir.toString())) {
            context.warning(QCoreApplication::translate("SoundscapeConvertWorkflow", "Scripts directory not found: %1").arg(options.s1ScriptsDir.toString()));
            return Core::Result<SoundscapeWorkflowResult>::skipped(
                QCoreApplication::translate("SoundscapeConvertWorkflow", "No scripts directory found to convert"));
        }

        const QString scriptsDirPath = options.s1ScriptsDir.toString();
        QDir scriptsDir(scriptsDirPath);

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
        context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "No soundscape files found to convert for map '%1'").arg(options.mapName));
        return Core::Result<SoundscapeWorkflowResult>::skipped(
            QCoreApplication::translate("SoundscapeConvertWorkflow", "No soundscape files found"));
    }

    Core::Path::FilesystemPath targetSoundeventsDir = options.s2ContentDir.isEmpty()
        ? Core::Path::FilesystemPath(QStringLiteral("soundevents"))
        : options.s2ContentDir / QStringLiteral("soundevents");

    Core::Error::ExecutionContext dirCtx{
        .stage = QStringLiteral("Creating soundevents directory"),
        .targetPath = targetSoundeventsDir.toString()
    };
    auto createDirRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
        Core::FileSystem::FileSystem::createDirectory(targetSoundeventsDir.toString());
        return Core::Result<void>::success();
    }, dirCtx);
    if (!createDirRes.isSuccess()) {
        return Core::Result<SoundscapeWorkflowResult>::failure(createDirRes.error(), createDirRes.message());
    }

    SoundscapeConvertOptions convertOptions;
    if (!options.mixgroup.isEmpty()) {
        convertOptions.mixgroup = options.mixgroup;
    } else if (!options.mapName.isEmpty()) {
        convertOptions.mixgroup = options.mapName;
    }

    SoundscapeWorkflowResult aggregatedResult;
    aggregatedResult.succeeded = true;
    QSet<QString> uniqueAssets;

    const std::size_t totalFiles = candidateFiles.size();
    for (std::size_t i = 0; i < totalFiles; ++i) {
        if (context.isCancelled()) {
            context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "Soundscape conversion cancelled by user"));
            return Core::Result<SoundscapeWorkflowResult>::cancelled();
        }

        const auto& sourceFile = candidateFiles[i];
        const QString sourceFileName = Core::Path::PathUtils::filename(sourceFile.toString());
        const QString outputFileName = deriveOutputFileName(sourceFileName);
        auto targetFileOpt = targetSoundeventsDir.resolveBelow(outputFileName);
        if (!targetFileOpt.has_value()) {
            context.error(QCoreApplication::translate("SoundscapeConvertWorkflow", "Target file path '%1' traverses outside destination directory")
                              .arg(outputFileName));
            return Core::Result<SoundscapeWorkflowResult>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("SoundscapeConvertWorkflow", "Target file path traverses outside destination directory"),
                outputFileName);
        }
        const Core::Path::FilesystemPath targetFile = *targetFileOpt;

        context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "Converting %1 -> %2").arg(sourceFileName, outputFileName));

        auto fileRes = convertFile(sourceFile, targetFile, convertOptions, targetSoundeventsDir);
        if (!fileRes.isSuccess()) {
            context.error(QCoreApplication::translate("SoundscapeConvertWorkflow", "Failed to convert %1: %2").arg(sourceFileName, fileRes.message()));
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

        const double progress = (static_cast<double>(i + 1) / static_cast<double>(totalFiles)) * 100.0;
        context.updateProgress(progress);
    }

    for (const auto& asset : uniqueAssets) {
        aggregatedResult.uniqueRawSoundAssets.append(asset);
    }
    aggregatedResult.uniqueRawSoundAssets.sort();

    context.info(QCoreApplication::translate("SoundscapeConvertWorkflow", "Soundscape conversion completed: %1 soundscapes converted, %2 soundevents generated, %3 files written, %4 unique raw sound assets referenced.")
                     .arg(aggregatedResult.totalSoundscapesConverted)
                     .arg(aggregatedResult.totalSoundEventsGenerated)
                     .arg(aggregatedResult.generatedFiles.size())
                     .arg(aggregatedResult.uniqueRawSoundAssets.size()));

    return Core::Result<SoundscapeWorkflowResult>::success(
        std::move(aggregatedResult),
        QCoreApplication::translate("SoundscapeConvertWorkflow", "Soundscape conversion completed successfully"));
}

} // namespace Workflow::Soundscape
