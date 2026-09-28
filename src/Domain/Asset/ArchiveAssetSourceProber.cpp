#include "Domain/Asset/ArchiveAssetSourceProber.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"
#include "Domain/Package/VpkIndex.h"

namespace Domain::Asset {

ArchiveAssetSourceProber::ArchiveAssetSourceProber(
    Domain::Package::PackArchivePool& pool,
    const Domain::Package::VpkIndex* vpkIndex,
    const Domain::Package::VpkIndex* cs2Index)
    : m_pool(pool)
    , m_vpkIndex(vpkIndex)
    , m_cs2Index(cs2Index) {}

bool ArchiveAssetSourceProber::isNativeCs2Asset(const QString& entryPath) const {
    return m_cs2Index && m_cs2Index->hasCs2NativeAsset(entryPath);
}

bool ArchiveAssetSourceProber::hasLooseFile(
    const Domain::Game::SearchTarget& target,
    const QString& entryPath,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled() || !target.isDirectory()) {
        return false;
    }
    return (target.path() / entryPath).exists();
}

std::optional<Core::Path::FilesystemPath> ArchiveAssetSourceProber::queryVpkIndex(const QString& entryPath) const {
    if (!m_vpkIndex) {
        return std::nullopt;
    }
    return m_vpkIndex->findVpkForEntry(entryPath);
}

bool ArchiveAssetSourceProber::hasPackEntry(
    const Core::Path::FilesystemPath& packPath,
    const QString& entryPath,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled() || !packPath.exists()) {
        return false;
    }
    auto archiveRes = m_pool.getOrOpen(packPath, token);
    if (archiveRes.isFailure() || archiveRes.isCancelled()) {
        return false;
    }
    return archiveRes.value()->hasEntry(entryPath, token);
}

} // namespace Domain::Asset
