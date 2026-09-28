#pragma once

#include "Domain/Asset/IAssetSourceProber.h"

namespace Domain::Package {
class PackArchivePool;
class VpkIndex;
}

namespace Domain::Asset {

/**
 * @brief Adapter implementing IAssetSourceProber using PackArchivePool, VpkIndex, and filesystem.
 * Handles the low-level retrieval of pack archives and file existence checks.
 */
class ArchiveAssetSourceProber : public IAssetSourceProber {
public:
    ArchiveAssetSourceProber(
        Domain::Package::PackArchivePool& pool,
        const Domain::Package::VpkIndex* vpkIndex = nullptr,
        const Domain::Package::VpkIndex* cs2Index = nullptr);

    bool isNativeCs2Asset(const QString& entryPath) const override;
    bool hasLooseFile(
        const Domain::Game::SearchTarget& target,
        const QString& entryPath,
        const Core::Async::CancellationToken& token = {}) override;
    std::optional<Core::Path::FilesystemPath> queryVpkIndex(const QString& entryPath) const override;
    bool hasPackEntry(
        const Core::Path::FilesystemPath& packPath,
        const QString& entryPath,
        const Core::Async::CancellationToken& token = {}) override;

private:
    Domain::Package::PackArchivePool& m_pool;
    const Domain::Package::VpkIndex* m_vpkIndex = nullptr;
    const Domain::Package::VpkIndex* m_cs2Index = nullptr;
};

} // namespace Domain::Asset
