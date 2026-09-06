#include "Workflow/Common/AssetExtractor.h"

#include <exception>
#include <utility>

#include <QFileInfo>

#include "Core/Error/Exception.h"
#include "Core/FileSystem/FileSystem.h"
#include "Domain/Package/PackArchive.h"

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
 * @brief Extracts one entry from a pack archive into destFile.
 * A missing archive file counts as a benign miss; a present but unparseable
 * archive is a real failure and is surfaced.
 */
Core::Result<LookupHit> extractEntryFromPack(const Core::Path::FilesystemPath& packPath, const QString& entryPath, const Core::Path::FilesystemPath& destFile) {
    if (!packPath.exists()) {
        return Core::Result<LookupHit>::success({});
    }

    auto archive = Domain::Package::PackArchive::open(packPath);
    if (archive.isFailure()) {
        return Core::Result<LookupHit>::failure(archive.error());
    }
    if (!archive.value().hasEntry(entryPath)) {
        return Core::Result<LookupHit>::success({});
    }

    auto extracted = archive.value().extractEntryToFile(entryPath, destFile);
    if (extracted.isFailure()) {
        return Core::Result<LookupHit>::failure(extracted.error());
    }
    return Core::Result<LookupHit>::success(LookupHit{true, true});
}

/**
 * @brief Searches a single directory target for the entry: loose file first,
 *        then the target's own pak01_dir.vpk.
 */
Core::Result<LookupHit> extractFromDirectoryTarget(const Domain::Game::SearchTarget& target, const QString& entryPath, const Core::Path::FilesystemPath& destFile) {
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

    return extractEntryFromPack(target.path() / QStringLiteral("pak01_dir.vpk"), entryPath, destFile);
}

/**
 * @brief Extracts companion files (e.g. model vertex data) from the winning
 *        target. Best-effort: misses and failures are logged, never fatal.
 */
void extractCompanions(const Domain::Game::SearchTarget& winner, bool winnerFromPack, const QString& relativeAssetPath, const std::vector<QString>& companionExtensions, const Core::Path::FilesystemPath& destContentDir, const Workflow::Common::CancellationToken& token, Core::Logging::TaskLoggingContext* taskCtx) {
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
            ? extractEntryFromPack(winner.path(), companionRelative, companionDest)
            : extractFromDirectoryTarget(winner, companionRelative, companionDest);

        if (outcome.isFailure()) {
            if (taskCtx) {
                taskCtx->warning(QStringLiteral("Companion extraction failed for '%1': %2")
                                     .arg(companionRelative, outcome.message()));
            }
            continue;
        }
        if (!outcome.value().found && taskCtx) {
            taskCtx->debug(QStringLiteral("Companion '%1' not present in target '%2'")
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
    const CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    return runGuarded([&]() -> Core::Result<AssetExtraction> {
        if (relativeAssetPath.isEmpty()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QStringLiteral("relative asset path is empty"));
        }
        if (destContentDir.isEmpty() || !destContentDir.isValid()) {
            return Core::Result<AssetExtraction>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QStringLiteral("destination content directory is empty or invalid"));
        }

        const QString entryPath = normalizeRelativePath(relativeAssetPath);
        const Core::Path::FilesystemPath destFile = destContentDir / entryPath;

        for (const auto& target : targets) {
            if (token.isCancelled()) {
                return Core::Result<AssetExtraction>::cancelled(
                    QStringLiteral("Asset extraction cancelled"));
            }

            Core::Result<LookupHit> outcome = target.isVpk()
                ? extractEntryFromPack(target.path(), entryPath, destFile)
                : extractFromDirectoryTarget(target, entryPath, destFile);

            if (outcome.isFailure()) {
                return Core::Result<AssetExtraction>::failure(
                    outcome.error(),
                    QStringLiteral("Asset extraction failed while searching '%1'").arg(target.pathString()));
            }
            if (!outcome.value().found) {
                if (taskCtx) {
                    taskCtx->debug(QStringLiteral("Asset '%1' not found in target '%2'")
                                       .arg(entryPath, target.pathString()));
                }
                continue;
            }

            if (taskCtx) {
                taskCtx->info(QStringLiteral("Extracted '%1' from '%2'")
                                  .arg(entryPath, target.pathString()));
            }
            extractCompanions(target, outcome.value().fromPack, entryPath, options.companionExtensions, destContentDir, token, taskCtx);

            AssetExtraction extraction;
            extraction.extractedFilePath = destFile;
            extraction.sourceTargetPath = target.path();
            extraction.fromPack = outcome.value().fromPack;
            return Core::Result<AssetExtraction>::success(std::move(extraction));
        }

        return Core::Result<AssetExtraction>::skipped(
            QStringLiteral("Asset '%1' was not found in any search target").arg(entryPath));
    });
}

} // namespace Workflow::Common
