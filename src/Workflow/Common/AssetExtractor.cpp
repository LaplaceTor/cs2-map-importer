#include <QCoreApplication>
#include "Workflow/Common/AssetExtractor.h"

#include <utility>

#include <QFileInfo>

#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/FileSystem.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"

namespace {

struct LookupHit {
    bool found = false;
    bool fromPack = false;
};

Core::Result<LookupHit> extractEntryFromPack(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& packPath,
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile,
    const Core::Async::CancellationToken& token,
    const Core::Path::FilesystemPath& expectedBaseDir = {}) {
    if (token.isCancelled()) {
        return Core::Result<LookupHit>::cancelled(
            QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
    }

    if (!packPath.exists()) {
        return Core::Result<LookupHit>::success({});
    }

    auto archiveRes = pool.getOrOpen(packPath, token);
    if (archiveRes.isCancelled()) {
        return Core::Result<LookupHit>::cancelled(archiveRes.message());
    }
    if (archiveRes.isFailure()) {
        return Core::Result<LookupHit>::failure(archiveRes.error());
    }
    auto archive = archiveRes.value();
    auto hasRes = archive->hasEntry(entryPath, token);
    if (hasRes.isCancelled()) {
        return Core::Result<LookupHit>::cancelled(hasRes.message());
    }
    if (hasRes.isFailure()) {
        return Core::Result<LookupHit>::failure(hasRes.error());
    }
    if (!hasRes.value()) {
        return Core::Result<LookupHit>::success({});
    }

    auto extracted = archive->extractEntryToFile(entryPath, destFile, token, expectedBaseDir);
    if (extracted.isCancelled()) {
        return Core::Result<LookupHit>::cancelled(extracted.message());
    }
    if (extracted.isFailure()) {
        return Core::Result<LookupHit>::failure(extracted.error());
    }
    return Core::Result<LookupHit>::success(LookupHit{true, true});
}

Core::Result<LookupHit> extractFromDirectoryTarget(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& targetDir,
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile,
    const Core::Async::CancellationToken& token,
    const Core::Path::FilesystemPath& expectedBaseDir = {}) {
    if (token.isCancelled()) {
        return Core::Result<LookupHit>::cancelled(
            QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
    }

    const Core::Path::FilesystemPath looseFile = targetDir / entryPath;
    if (looseFile.exists()) {
        if (!expectedBaseDir.isEmpty() && !destFile.isSubpathOf(expectedBaseDir)) {
            return Core::Result<LookupHit>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AssetExtractor", "Destination path escapes base directory"),
                destFile.toString());
        }

        Core::Error::ExecutionContext copyCtx{
            .stage = QStringLiteral("Copying loose file"),
            .resourcePath = entryPath,
            .targetPath = destFile.toString()
        };
        auto copyRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
            Core::FileSystem::FileSystem::copy(looseFile.toString(), destFile.toString(), true, token);
            return Core::Result<void>::success();
        }, copyCtx);

        if (copyRes.isCancelled()) {
            return Core::Result<LookupHit>::cancelled(copyRes.message());
        }
        if (copyRes.isFailure()) {
            return Core::Result<LookupHit>::failure(copyRes.error());
        }
        return Core::Result<LookupHit>::success(LookupHit{true, false});
    }

    return extractEntryFromPack(pool, targetDir / QStringLiteral("pak01_dir.vpk"), entryPath, destFile, token, expectedBaseDir);
}

Core::Result<void> extractCompanions(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& winnerPath,
    bool winnerFromPack,
    const QString& relativeAssetPath,
    const std::vector<QString>& companionExtensions,
    const Core::Path::FilesystemPath& destContentDir,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (companionExtensions.empty()) {
        return Core::Result<void>::success();
    }

    const QFileInfo assetInfo(relativeAssetPath);
    const QString baseName = assetInfo.baseName();
    const QString assetDir = assetInfo.path();
    for (const QString& extension : companionExtensions) {
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
        }

        QString companionRelative = baseName + u'.' + extension;
        if (!assetDir.isEmpty() && assetDir != QStringLiteral(".")) {
            companionRelative = assetDir + u'/' + companionRelative;
        }

        auto companionDestOpt = destContentDir.resolveBelow(companionRelative);
        if (!companionDestOpt.has_value()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("AssetExtractor", "Companion path '%1' traverses outside destination directory")
                                     .arg(companionRelative));
            }
            continue;
        }
        const Core::Path::FilesystemPath companionDest = *companionDestOpt;
        Core::Result<LookupHit> outcome = winnerFromPack
            ? extractEntryFromPack(pool, winnerPath, companionRelative, companionDest, token, destContentDir)
            : extractFromDirectoryTarget(pool, winnerPath, companionRelative, companionDest, token, destContentDir);

        if (outcome.isCancelled()) {
            return Core::Result<void>::cancelled(outcome.message());
        }

        if (outcome.isFailure()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("AssetExtractor", "Companion extraction failed for '%1': %2")
                                     .arg(companionRelative, outcome.message()));
            }
            continue;
        }
        if (!outcome.value().found && taskCtx) {
            taskCtx->debug(QCoreApplication::translate("AssetExtractor", "Companion '%1' not present in target '%2'")
                               .arg(companionRelative, winnerPath.toString()));
        }
    }
    return Core::Result<void>::success();
}

} // namespace

namespace Workflow::Common {

Core::Result<AssetExtraction> AssetExtractor::extractLocated(
    const Domain::Asset::AssetLocation& location,
    const Core::Path::FilesystemPath& destContentDir,
    const AssetExtractOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<AssetExtraction>::cancelled(
            QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Extracting located asset"),
        .resourcePath = location.relativePath,
        .targetPath = location.sourceTargetPath.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<AssetExtraction> {
        if (destContentDir.isEmpty() || !destContentDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AssetExtractor", "destination content directory is empty or invalid"));
        }

        if (location.relativePath.isEmpty()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("AssetExtractor", "asset relative path is empty"));
        }

        auto destFileOpt = destContentDir.resolveBelow(location.relativePath);
        if (!destFileOpt.has_value()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AssetExtractor", "asset path traverses outside destination directory"),
                location.relativePath);
        }

        if (token.isCancelled()) {
            return Core::Result<AssetExtraction>::cancelled(
                QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
        }

        Domain::Package::PackArchivePool localPool;
        Domain::Package::PackArchivePool* archivePool = options.locateOptions.archivePool;
        Domain::Package::PackArchivePool& pool = archivePool ? *archivePool : localPool;

        const Core::Path::FilesystemPath destFile = *destFileOpt;

        if (!location.isInsidePack) {
            const Core::Path::FilesystemPath sourceFile = location.looseFilePath.isValid() && !location.looseFilePath.isEmpty()
                ? location.looseFilePath
                : location.sourceTargetPath / location.relativePath;

            Core::Error::ExecutionContext copyCtx{
                .stage = QStringLiteral("Copying loose asset file"),
                .resourcePath = location.relativePath,
                .targetPath = destFile.toString()
            };
            auto copyRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
                Core::FileSystem::FileSystem::copy(sourceFile.toString(), destFile.toString(), true, token);
                return Core::Result<void>::success();
            }, copyCtx);

            if (copyRes.isCancelled()) {
                return Core::Result<AssetExtraction>::cancelled(copyRes.message());
            }
            if (copyRes.isFailure()) {
                return Core::Result<AssetExtraction>::failure(copyRes.error());
            }

            if (taskCtx) {
                taskCtx->info(QCoreApplication::translate("AssetExtractor", "Extracted '%1' from loose folder '%2'")
                                  .arg(location.relativePath, location.sourceTargetPath.toString()));
            }

            auto companionRes = extractCompanions(pool, location.sourceTargetPath, false, location.relativePath,
                                                  options.companionExtensions, destContentDir, token, taskCtx);
            if (companionRes.isCancelled()) {
                return Core::Result<AssetExtraction>::cancelled(companionRes.message());
            }

            AssetExtraction extraction;
            extraction.extractedFilePath = destFile;
            extraction.sourceTargetPath = location.sourceTargetPath;
            extraction.fromPack = false;
            return Core::Result<AssetExtraction>::success(std::move(extraction));
        }

        // Inside pack archive
        auto outcome = extractEntryFromPack(pool, location.sourceTargetPath, location.relativePath, destFile, token, destContentDir);
        if (outcome.isCancelled()) {
            return Core::Result<AssetExtraction>::cancelled(outcome.message());
        }
        if (outcome.isFailure()) {
            return Core::Result<AssetExtraction>::failure(
                outcome.error(),
                QCoreApplication::translate("AssetExtractor", "Asset extraction failed while searching '%1'")
                    .arg(location.sourceTargetPath.toString()));
        }

        if (!outcome.value().found) {
            return Core::Result<AssetExtraction>::skipped(
                QCoreApplication::translate("AssetExtractor", "Asset '%1' was not found in winning VPK")
                    .arg(location.relativePath));
        }

        if (taskCtx) {
            taskCtx->info(QCoreApplication::translate("AssetExtractor", "Extracted '%1' from '%2'")
                              .arg(location.relativePath, location.sourceTargetPath.toString()));
        }

        auto companionRes = extractCompanions(pool, location.sourceTargetPath, true, location.relativePath,
                                              options.companionExtensions, destContentDir, token, taskCtx);
        if (companionRes.isCancelled()) {
            return Core::Result<AssetExtraction>::cancelled(companionRes.message());
        }

        AssetExtraction extraction;
        extraction.extractedFilePath = destFile;
        extraction.sourceTargetPath = location.sourceTargetPath;
        extraction.fromPack = true;
        return Core::Result<AssetExtraction>::success(std::move(extraction));
    }, ctx);
}

Core::Result<AssetExtraction> AssetExtractor::extract(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const Core::Path::FilesystemPath& destContentDir,
    const AssetExtractOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<AssetExtraction>::cancelled(
            QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Locating and extracting asset"),
        .resourcePath = relativeAssetPath,
        .targetPath = destContentDir.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<AssetExtraction> {
        auto locateRes = AssetLocator::locate(targets, relativeAssetPath, options.locateOptions, token, taskCtx);
        if (locateRes.isCancelled()) {
            return Core::Result<AssetExtraction>::cancelled(locateRes.message());
        }
        if (locateRes.isSkipped()) {
            return Core::Result<AssetExtraction>::skipped(locateRes.message());
        }
        if (!locateRes.isSuccess()) {
            return Core::Result<AssetExtraction>::failure(locateRes.error(), locateRes.message());
        }

        const auto& locationOpt = locateRes.value();
        if (!locationOpt.has_value()) {
            return Core::Result<AssetExtraction>::skipped(
                QCoreApplication::translate("AssetExtractor", "Asset '%1' was not found in any search target")
                    .arg(relativeAssetPath));
        }

        return extractLocated(
            locationOpt.value(),
            destContentDir,
            options,
            token,
            taskCtx);
    }, ctx);
}

} // namespace Workflow::Common
