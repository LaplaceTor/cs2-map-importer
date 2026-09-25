#include <QCoreApplication>
#include "Workflow/Common/AssetExtractor.h"

#include <exception>
#include <utility>

#include <QFileInfo>

#include "Core/Error/Exception.h"
#include "Core/FileSystem/FileSystem.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"

namespace {

struct LookupHit {
    bool found = false;
    bool fromPack = false;
};

template<typename Fn>
auto runGuarded(Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const Core::Error::Exception& ex) {
        return decltype(fn())::failure(ex.error());
    } catch (const std::exception& ex) {
        return decltype(fn())::failure(
            Core::Error::ErrorCode::OperationFailed,
            QString::fromUtf8(ex.what()));
    }
}

Core::Result<LookupHit> extractEntryFromPack(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& packPath,
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile) {
    if (!packPath.exists()) {
        return Core::Result<LookupHit>::success({});
    }

    auto archiveRes = pool.getOrOpen(packPath);
    if (archiveRes.isFailure()) {
        return Core::Result<LookupHit>::failure(archiveRes.error());
    }
    auto archive = archiveRes.value();
    if (!archive->hasEntry(entryPath)) {
        return Core::Result<LookupHit>::success({});
    }

    auto extracted = archive->extractEntryToFile(entryPath, destFile);
    if (extracted.isFailure()) {
        return Core::Result<LookupHit>::failure(extracted.error());
    }
    return Core::Result<LookupHit>::success(LookupHit{true, true});
}

Core::Result<LookupHit> extractFromDirectoryTarget(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& targetDir,
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile) {
    const Core::Path::FilesystemPath looseFile = targetDir / entryPath;
    if (looseFile.exists()) {
        try {
            Core::FileSystem::FileSystem::copy(looseFile.toString(), destFile.toString(), true);
        } catch (const Core::Error::Exception& ex) {
            return Core::Result<LookupHit>::failure(ex.error());
        } catch (const std::exception& ex) {
            return Core::Result<LookupHit>::failure(
                Core::Error::ErrorCode::WriteFailed,
                QString::fromUtf8(ex.what()));
        }
        return Core::Result<LookupHit>::success(LookupHit{true, false});
    }

    return extractEntryFromPack(pool, targetDir / QStringLiteral("pak01_dir.vpk"), entryPath, destFile);
}

void extractCompanions(
    Domain::Package::PackArchivePool& pool,
    const Core::Path::FilesystemPath& winnerPath,
    bool winnerFromPack,
    const QString& relativeAssetPath,
    const std::vector<QString>& companionExtensions,
    const Core::Path::FilesystemPath& destContentDir,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (companionExtensions.empty()) {
        return;
    }

    const QFileInfo assetInfo(relativeAssetPath);
    const QString baseName = assetInfo.baseName();
    const QString assetDir = assetInfo.path();
    for (const QString& extension : companionExtensions) {
        if (token.isCancelled()) {
            return;
        }

        QString companionRelative = baseName + u'.' + extension;
        if (!assetDir.isEmpty() && assetDir != QStringLiteral(".")) {
            companionRelative = assetDir + u'/' + companionRelative;
        }

        const Core::Path::FilesystemPath companionDest = destContentDir / companionRelative;
        Core::Result<LookupHit> outcome = winnerFromPack
            ? extractEntryFromPack(pool, winnerPath, companionRelative, companionDest)
            : extractFromDirectoryTarget(pool, winnerPath, companionRelative, companionDest);

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
}

} // namespace

namespace Workflow::Common {

Core::Result<AssetExtraction> AssetExtractor::extractLocated(
    const Domain::Asset::AssetLocation& location,
    const Core::Path::FilesystemPath& destContentDir,
    const std::vector<QString>& companionExtensions,
    Domain::Package::PackArchivePool* archivePool,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    return runGuarded([&]() -> Core::Result<AssetExtraction> {
        if (destContentDir.isEmpty() || !destContentDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AssetExtractor", "destination content directory is empty or invalid"));
        }

        if (token.isCancelled()) {
            return Core::Result<AssetExtraction>::cancelled(
                QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
        }

        Domain::Package::PackArchivePool localPool;
        Domain::Package::PackArchivePool& pool = archivePool ? *archivePool : localPool;

        const Core::Path::FilesystemPath destFile = destContentDir / location.relativePath;

        if (!location.isInsidePack) {
            const Core::Path::FilesystemPath sourceFile = location.looseFilePath.isValid() && !location.looseFilePath.isEmpty()
                ? location.looseFilePath
                : location.sourceTargetPath / location.relativePath;

            try {
                Core::FileSystem::FileSystem::copy(sourceFile.toString(), destFile.toString(), true);
            } catch (const Core::Error::Exception& ex) {
                return Core::Result<AssetExtraction>::failure(ex.error());
            } catch (const std::exception& ex) {
                return Core::Result<AssetExtraction>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QString::fromUtf8(ex.what()));
            }

            if (taskCtx) {
                taskCtx->info(QCoreApplication::translate("AssetExtractor", "Extracted '%1' from loose folder '%2'")
                                  .arg(location.relativePath, location.sourceTargetPath.toString()));
            }

            extractCompanions(pool, location.sourceTargetPath, false, location.relativePath,
                              companionExtensions, destContentDir, token, taskCtx);

            AssetExtraction extraction;
            extraction.extractedFilePath = destFile;
            extraction.sourceTargetPath = location.sourceTargetPath;
            extraction.fromPack = false;
            return Core::Result<AssetExtraction>::success(std::move(extraction));
        }

        // Inside pack archive
        auto outcome = extractEntryFromPack(pool, location.sourceTargetPath, location.relativePath, destFile);
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

        extractCompanions(pool, location.sourceTargetPath, true, location.relativePath,
                          companionExtensions, destContentDir, token, taskCtx);

        AssetExtraction extraction;
        extraction.extractedFilePath = destFile;
        extraction.sourceTargetPath = location.sourceTargetPath;
        extraction.fromPack = true;
        return Core::Result<AssetExtraction>::success(std::move(extraction));
    });
}

Core::Result<AssetExtraction> AssetExtractor::extract(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const Core::Path::FilesystemPath& destContentDir,
    const AssetExtractOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    auto locateRes = Domain::Asset::AssetLocator::locate(targets, relativeAssetPath, options.locateOptions, token, taskCtx);
    if (!locateRes.isSuccess()) {
        return Core::Result<AssetExtraction>::failure(locateRes.error(), locateRes.message());
    }
    if (locateRes.isSkipped()) {
        return Core::Result<AssetExtraction>::skipped(locateRes.message());
    }
    if (locateRes.isCancelled()) {
        return Core::Result<AssetExtraction>::cancelled(locateRes.message());
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
        options.companionExtensions,
        options.locateOptions.archivePool,
        token,
        taskCtx);
}

} // namespace Workflow::Common
