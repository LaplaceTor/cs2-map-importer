#pragma once

#include <optional>
#include <QString>
#include "Core/Async/CancellationToken.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Game/SearchTarget.h"

namespace Domain::Asset {

/**
 * @brief Abstract prober interface for querying asset existence across different candidate backends.
 * Decouples pure location strategy from archive pools and disk filesystems.
 */
class IAssetSourceProber {
public:
    virtual ~IAssetSourceProber() = default;

    /**
     * @brief Checks whether the asset exists natively in CS2 (deduplication check).
     */
    virtual bool isNativeCs2Asset(const QString& entryPath) const = 0;

    /**
     * @brief Checks whether a loose file exists on disk within the given directory target.
     */
    virtual Core::Result<bool> hasLooseFile(
        const Domain::Game::SearchTarget& target,
        const QString& entryPath,
        const Core::Async::CancellationToken& token = {}) = 0;

    /**
     * @brief Queries the fast VPK index for the entry path.
     * @return FilesystemPath to the winning VPK archive if indexed, or std::nullopt.
     */
    virtual std::optional<Core::Path::FilesystemPath> queryVpkIndex(const QString& entryPath) const = 0;

    /**
     * @brief Checks whether the entry exists inside the specified pack archive (VPK).
     */
    virtual Core::Result<bool> hasPackEntry(
        const Core::Path::FilesystemPath& packPath,
        const QString& entryPath,
        const Core::Async::CancellationToken& token = {}) = 0;
};

} // namespace Domain::Asset
