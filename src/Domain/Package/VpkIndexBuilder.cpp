#include "Domain/Package/VpkIndexBuilder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <vpkpp/PackFile.h>

#include "Core/Hash/Sha256.h"

namespace Domain::Package {

Core::Result<VpkIndex> VpkIndexBuilder::build(
    const std::vector<Core::Path::FilesystemPath>& vpkPaths,
    bool isCs2,
    const Core::Async::CancellationToken& token,
    Core::Logging::TaskLoggingContext* taskCtx) {
    if (token.isCancelled()) {
        return Core::Result<VpkIndex>::cancelled(
            QCoreApplication::translate("VpkIndexBuilder", "VPK index building cancelled"));
    }

    VpkIndex index;
    index.setIsCs2Index(isCs2);
    index.setGeneratedTimestamp(QDateTime::currentMSecsSinceEpoch());

    std::vector<VpkArchiveMeta> vpkMetas;
    vpkMetas.reserve(vpkPaths.size());

    QHash<QString, uint16_t> entryToVpk;
    QSet<QString> cs2NativeStems;

    uint16_t currentId = 0;

    for (const auto& vpkPath : vpkPaths) {
        if (token.isCancelled()) {
            return Core::Result<VpkIndex>::cancelled(
                QCoreApplication::translate("VpkIndexBuilder", "VPK index building cancelled"));
        }

        if (!vpkPath.exists() || !vpkPath.isFile()) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("VpkIndexBuilder", "Skipping missing VPK: %1")
                                     .arg(vpkPath.toString()));
            }
            continue;
        }

        if (taskCtx) {
            taskCtx->info(QCoreApplication::translate("VpkIndexBuilder", "Indexing VPK: %1")
                              .arg(vpkPath.toString()));
        }

        // 1. Compute SHA-256
        auto shaRes = Core::Hash::Sha256::computeFileHash(vpkPath);
        if (shaRes.isFailure()) {
            return Core::Result<VpkIndex>::failure(
                shaRes.error(),
                QCoreApplication::translate("VpkIndexBuilder", "Failed to compute SHA-256 for VPK: %1").arg(vpkPath.toString()));
        }

        QFileInfo fi(vpkPath.toString());
        VpkArchiveMeta meta;
        meta.id = currentId;
        meta.path = vpkPath;
        meta.size = fi.size();
        meta.lastModified = fi.lastModified().toMSecsSinceEpoch();
        meta.sha256 = shaRes.value();
        vpkMetas.push_back(meta);

        // 2. Open via sourcepp
        auto packFile = vpkpp::PackFile::open(vpkPath.toString().toStdString());
        if (!packFile) {
            if (taskCtx) {
                taskCtx->warning(QCoreApplication::translate("VpkIndexBuilder", "Failed to parse VPK with sourcepp: %1")
                                     .arg(vpkPath.toString()));
            }
            ++currentId;
            continue;
        }

        // 3. Collect entries
        packFile->runForAllEntries([&](const std::string& path, const vpkpp::Entry&) {
            const QString rawPath = QString::fromStdString(path);
            const QString normalized = VpkIndex::normalizePath(rawPath);
            if (normalized.isEmpty()) {
                return;
            }

            // First occurrence wins (vpkPaths is ordered from highest to lowest priority)
            if (!entryToVpk.contains(normalized)) {
                entryToVpk.insert(normalized, currentId);
            }

            if (isCs2) {
                const QString stem = VpkIndex::extractAssetStem(normalized);
                if (!stem.isEmpty()) {
                    cs2NativeStems.insert(stem);
                }
            }
        });

        ++currentId;
    }

    index.setVpks(std::move(vpkMetas));
    index.setEntries(std::move(entryToVpk));
    index.setCs2NativeStems(std::move(cs2NativeStems));

    if (taskCtx) {
        taskCtx->info(QCoreApplication::translate("VpkIndexBuilder", "VPK index built successfully: %1 archives, %2 total entries")
                          .arg(QString::number(index.vpks().size()), QString::number(index.entryCount())));
    }

    return Core::Result<VpkIndex>::success(std::move(index));
}

} // namespace Domain::Package
