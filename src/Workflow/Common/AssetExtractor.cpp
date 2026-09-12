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

QString normalizeRelativePath(const QString& relativePath) {
    QString normalized = relativePath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    return normalized;
}

/**
 * @brief Extracts one entry from a pack archive into destFile using the provided pool.
 * A missing archive file counts as a benign miss; a present but unparseable
 * archive is a real failure and is surfaced.
 */
Core::Result<LookupHit> extractEntryFromPack(Domain::Package::PackArchivePool& pool, const Core::Path::FilesystemPath& packPath, const QString& entryPath, const Core::Path::FilesystemPath& destFile) {
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

/**
 * @brief Searches a single directory target for the entry: loose file first,
 *        then the target's own pak01_dir.vpk.
 */
Core::Result<LookupHit> extractFromDirectoryTarget(Domain::Package::PackArchivePool& pool, const Domain::Game::SearchTarget& target, const QString& entryPath, const Core::Path::FilesystemPath& destFile) {
    const Core::Path::FilesystemPath looseFile = target.path() / entryPath;
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

    return extractEntryFromPack(pool, target.path() / QStringLiteral("pak01_dir.vpk"), entryPath, destFile);
}

/**
 * @brief Extracts companion files (e.g. model vertex data) from the winning
 *        target. Best-effort: misses and failures are logged, never fatal.
 */
void extractCompanions(Domain::Package::PackArchivePool& pool, const Domain::Game::SearchTarget& winner, bool winnerFromPack, const QString& relativeAssetPath, const std::vector<QString>& companionExtensions, const Core::Path::FilesystemPath& destContentDir, const Core::Async::CancellationToken& token, Core::Logging::TaskLoggingContext* taskCtx) {
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
            ? extractEntryFromPack(pool, winner.path(), companionRelative, companionDest)
            : extractFromDirectoryTarget(pool, winner, companionRelative, companionDest);

        if (outcome.isFailure()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("AssetExtractor", "Companion extraction failed for '%1': %2")
                                     .arg(companionRelative, outcome.message()));
            }
            continue;
        }
        if (!outcome.value().found && taskCtx) {
            taskCtx->debug(QCoreApplication::translate("AssetExtractor", "Companion '%1' not present in target '%2'")
                               .arg(companionRelative, winner.pathString()));
        }
    }
}

} // namespace

namespace Workflow::Common {

Core::Result<AssetExtraction> AssetExtractor::extract(
    const std::vector<Domain::Game::SearchTarget>& targets,
    const QString& relativeAssetPath,
    const Core::Path::FilesystemPath& destContentDir,
    const AssetExtractOptions& options,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    return runGuarded([&]() -> Core::Result<AssetExtraction> {
        if (relativeAssetPath.isEmpty()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("AssetExtractor", "relative asset path is empty"));
        }
        if (destContentDir.isEmpty() || !destContentDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("AssetExtractor", "destination content directory is empty or invalid"));
        }

        Domain::Package::PackArchivePool localPool;
        Domain::Package::PackArchivePool& pool = options.archivePool ? *options.archivePool : localPool;

        const QString entryPath = normalizeRelativePath(relativeAssetPath);
        const Core::Path::FilesystemPath destFile = destContentDir / entryPath;

        for (const auto& target : targets) {
            if (token.isCancelled()) {
                return Core::Result<AssetExtraction>::cancelled(
                    QCoreApplication::translate("AssetExtractor", "Asset extraction cancelled"));
            }

            Core::Result<LookupHit> outcome = target.isVpk()
                ? extractEntryFromPack(pool, target.path(), entryPath, destFile)
                : extractFromDirectoryTarget(pool, target, entryPath, destFile);

            if (outcome.isFailure()) {
                return Core::Result<AssetExtraction>::failure(
                    outcome.error(),
                    QCoreApplication::translate("AssetExtractor", "Asset extraction failed while searching '%1'").arg(target.pathString()));
            }
            if (!outcome.value().found) {
                if (taskCtx) {
                    taskCtx->debug(QCoreApplication::translate("AssetExtractor", "Asset '%1' not found in target '%2'")
                                       .arg(entryPath, target.pathString()));
                }
                continue;
            }

            if (taskCtx) {
                taskCtx->info(QCoreApplication::translate("AssetExtractor", "Extracted '%1' from '%2'")
                                  .arg(entryPath, target.pathString()));
            }
            extractCompanions(pool, target, outcome.value().fromPack, entryPath, options.companionExtensions, destContentDir, token, taskCtx);

            AssetExtraction extraction;
            extraction.extractedFilePath = destFile;
            extraction.sourceTargetPath = target.path();
            extraction.fromPack = outcome.value().fromPack;
            return Core::Result<AssetExtraction>::success(std::move(extraction));
        }

        return Core::Result<AssetExtraction>::skipped(
            QCoreApplication::translate("AssetExtractor", "Asset '%1' was not found in any search target").arg(entryPath));
    });
}

} // namespace Workflow::Common
