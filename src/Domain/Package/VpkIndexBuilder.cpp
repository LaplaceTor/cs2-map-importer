#include "Domain/Package/VpkIndexBuilder.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSet>

#include "Core/Error/ErrorCode.h"
#include "Core/Hash/Sha256.h"
#include "Domain/Package/PackArchive.h"

namespace Domain::Package {

Core::Result<VpkArchiveIndex> VpkIndexBuilder::buildArchive(
    const Core::Path::FilesystemPath& vpkPath,
    const QString& precomputedSha256,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<VpkArchiveIndex>::cancelled(
            QCoreApplication::translate("VpkIndexBuilder", "VPK archive indexing cancelled"));
    }

    QString sha256;
    if (precomputedSha256.isEmpty()) {
        auto shaRes = Core::Hash::Sha256::computeFileHash(vpkPath, token);
        if (shaRes.isCancelled()) {
            return Core::Result<VpkArchiveIndex>::cancelled(shaRes.message());
        }
        if (shaRes.isFailure()) {
            return Core::Result<VpkArchiveIndex>::failure(
                shaRes.error(),
                QCoreApplication::translate("VpkIndexBuilder", "Failed to compute SHA-256 for VPK: %1")
                    .arg(vpkPath.toString()));
        }
        sha256 = shaRes.value().toLower();
    } else {
        if (!VpkArchiveIndex::isValidSha256Hex(precomputedSha256)) {
            return Core::Result<VpkArchiveIndex>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("VpkIndexBuilder", "Invalid precomputed SHA-256 hash"),
                precomputedSha256);
        }
        sha256 = precomputedSha256.toLower();
    }

    if (taskCtx) {
        taskCtx->info(QCoreApplication::translate("VpkIndexBuilder", "Indexing VPK: %1")
                          .arg(vpkPath.toString()));
    }

    auto archiveRes = PackArchive::open(vpkPath, token);
    if (archiveRes.isCancelled()) {
        return Core::Result<VpkArchiveIndex>::cancelled(archiveRes.message());
    }
    if (archiveRes.isFailure()) {
        return Core::Result<VpkArchiveIndex>::failure(
            archiveRes.error(),
            QCoreApplication::translate("VpkIndexBuilder", "Failed to parse VPK archive: %1")
                .arg(vpkPath.toString()));
    }

    auto entriesRes = archiveRes.value().listEntries();
    if (entriesRes.isCancelled()) {
        return Core::Result<VpkArchiveIndex>::cancelled(entriesRes.message());
    }
    if (entriesRes.isFailure()) {
        return Core::Result<VpkArchiveIndex>::failure(
            entriesRes.error(),
            QCoreApplication::translate("VpkIndexBuilder", "Failed to list entries in VPK archive: %1")
                .arg(vpkPath.toString()));
    }

    const auto& rawEntries = entriesRes.value();
    std::vector<QString> normalizedEntries;
    normalizedEntries.reserve(rawEntries.size());
    QSet<QString> seenEntries;
    seenEntries.reserve(static_cast<qsizetype>(rawEntries.size()));

    for (const QString& rawPath : rawEntries) {
        const QString normalized = VpkIndex::normalizePath(rawPath);
        if (normalized.isEmpty() || seenEntries.contains(normalized)) {
            continue;
        }
        seenEntries.insert(normalized);
        normalizedEntries.push_back(normalized);
    }

    if (token.isCancelled()) {
        return Core::Result<VpkArchiveIndex>::cancelled(
            QCoreApplication::translate("VpkIndexBuilder", "VPK archive indexing cancelled"));
    }

    return Core::Result<VpkArchiveIndex>::success(
        VpkArchiveIndex(std::move(sha256), std::move(normalizedEntries)));
}

Core::Result<VpkIndex> VpkIndexBuilder::build(
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<VpkIndex>::cancelled(
            QCoreApplication::translate("VpkIndexBuilder", "VPK index building cancelled"));
    }

    std::vector<VpkArchiveSource> sources;
    sources.reserve(vpkPaths.size());
    QSet<QString> seenArchiveKeys;
    seenArchiveKeys.reserve(static_cast<qsizetype>(vpkPaths.size()));

    for (const auto& vpkPath : vpkPaths) {
        if (token.isCancelled()) {
            return Core::Result<VpkIndex>::cancelled(
                QCoreApplication::translate("VpkIndexBuilder", "VPK index building cancelled"));
        }

        const QString archiveKey = VpkIndex::normalizeArchivePathKey(vpkPath);
        if (archiveKey.isEmpty() || seenArchiveKeys.contains(archiveKey)) {
            continue;
        }
        seenArchiveKeys.insert(archiveKey);

        if (!vpkPath.exists() || !vpkPath.isFile()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("VpkIndexBuilder", "Skipping missing VPK: %1")
                                     .arg(vpkPath.toString()));
            }
            continue;
        }

        auto archiveIdxRes = buildArchive(vpkPath, {}, token, taskCtx);
        if (archiveIdxRes.isCancelled()) {
            return Core::Result<VpkIndex>::cancelled(archiveIdxRes.message());
        }
        if (archiveIdxRes.isFailure()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("VpkIndexBuilder", "Failed to parse VPK archive: %1")
                                     .arg(vpkPath.toString()));
            }
            continue;
        }

        const QFileInfo fi(vpkPath.toString());
        VpkArchiveSource source;
        source.meta.path = vpkPath;
        source.meta.size = fi.size();
        source.meta.lastModified = fi.lastModified().toMSecsSinceEpoch();
        source.meta.sha256 = archiveIdxRes.value().sha256();
        source.archiveIndex = std::make_shared<const VpkArchiveIndex>(std::move(archiveIdxRes.value()));
        sources.push_back(std::move(source));
    }

    VpkIndex index = VpkIndex::compose(std::move(sources), isCs2);

    if (taskCtx) {
        taskCtx->info(QCoreApplication::translate("VpkIndexBuilder", "VPK index built successfully: %1 archives, %2 total entries")
                          .arg(QString::number(index.vpks().size()), QString::number(index.entryCount())));
    }

    return Core::Result<VpkIndex>::success(std::move(index));
}

} // namespace Domain::Package
