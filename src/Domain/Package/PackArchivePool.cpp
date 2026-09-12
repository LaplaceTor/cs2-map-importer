#include <QCoreApplication>
#include "Domain/Package/PackArchivePool.h"

#include <utility>

namespace Domain::Package {

PackArchivePool::PackArchivePool(PackArchivePool&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_archives = std::move(other.m_archives);
}

PackArchivePool& PackArchivePool::operator=(PackArchivePool&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_archives = std::move(other.m_archives);
    }
    return *this;
}

Core::Result<std::shared_ptr<PackArchive>> PackArchivePool::getOrOpen(const Core::Path::FilesystemPath& archivePath) {
    if (archivePath.isEmpty() || !archivePath.isValid()) {
        return Core::Result<std::shared_ptr<PackArchive>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("PackArchivePool", "pack archive path is empty or invalid"));
    }

    const QString normalizedKey = archivePath.toString().toLower();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_archives.constFind(normalizedKey);
        if (it != m_archives.constEnd()) {
            return Core::Result<std::shared_ptr<PackArchive>>::success(it.value());
        }
    }

    auto opened = PackArchive::open(archivePath);
    if (opened.isFailure()) {
        return Core::Result<std::shared_ptr<PackArchive>>::failure(opened.error());
    }

    auto sharedArchive = std::make_shared<PackArchive>(std::move(opened.value()));

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_archives.insert(normalizedKey, sharedArchive);
        return Core::Result<std::shared_ptr<PackArchive>>::success(it.value());
    }
}

bool PackArchivePool::contains(const Core::Path::FilesystemPath& archivePath) const {
    if (archivePath.isEmpty()) {
        return false;
    }
    const QString normalizedKey = archivePath.toString().toLower();
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_archives.contains(normalizedKey);
}

std::size_t PackArchivePool::size() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<std::size_t>(m_archives.size());
}

void PackArchivePool::clear() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_archives.clear();
}

} // namespace Domain::Package
