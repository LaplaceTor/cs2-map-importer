#include "Domain/Package/PackArchivePool.h"

#include <chrono>
#include <utility>

#include <QCoreApplication>

#include "Core/Error/Exception.h"

namespace Domain::Package {

PackArchivePool::PackArchivePool(PackArchivePool&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_archives = std::move(other.m_archives);
    m_opening = std::move(other.m_opening);
}

PackArchivePool& PackArchivePool::operator=(PackArchivePool&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_archives = std::move(other.m_archives);
        m_opening = std::move(other.m_opening);
    }
    return *this;
}

Core::Result<std::shared_ptr<PackArchive>> PackArchivePool::getOrOpen(
    const Core::Path::FilesystemPath& archivePath,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        return Core::Result<std::shared_ptr<PackArchive>>::cancelled(
            QCoreApplication::translate("PackArchivePool", "archive open cancelled"));
    }

    if (archivePath.isEmpty() || !archivePath.isValid()) {
        return Core::Result<std::shared_ptr<PackArchive>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("PackArchivePool", "pack archive path is empty or invalid"));
    }

    const QString normalizedKey = archivePath.toString().toLower();

    while (true) {
        if (token.isCancelled()) {
            return Core::Result<std::shared_ptr<PackArchive>>::cancelled(
                QCoreApplication::translate("PackArchivePool", "archive open cancelled"));
        }

        std::shared_ptr<OpeningEntry> entry;
        bool isInitiator = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_archives.constFind(normalizedKey);
            if (it != m_archives.constEnd()) {
                return Core::Result<std::shared_ptr<PackArchive>>::success(it.value());
            }

            auto openIt = m_opening.constFind(normalizedKey);
            if (openIt != m_opening.constEnd()) {
                entry = openIt.value();
                isInitiator = false;
            } else {
                entry = std::make_shared<OpeningEntry>();
                m_opening.insert(normalizedKey, entry);
                isInitiator = true;
            }
        }

        if (isInitiator) {
            Core::Result<std::shared_ptr<PackArchive>> finalResult;
            try {
                auto opened = PackArchive::open(archivePath, token);
                if (opened.isCancelled()) {
                    finalResult = Core::Result<std::shared_ptr<PackArchive>>::cancelled(opened.message());
                } else if (opened.isFailure()) {
                    finalResult = Core::Result<std::shared_ptr<PackArchive>>::failure(opened.error());
                } else {
                    auto sharedArchive = std::make_shared<PackArchive>(std::move(opened.value()));
                    finalResult = Core::Result<std::shared_ptr<PackArchive>>::success(sharedArchive);
                }
            } catch (const Core::Error::Exception& ex) {
                finalResult = Core::Result<std::shared_ptr<PackArchive>>::failure(ex.error());
            } catch (const std::exception& ex) {
                finalResult = Core::Result<std::shared_ptr<PackArchive>>::failure(
                    Core::Error::ErrorCode::OperationFailed, QString::fromUtf8(ex.what()));
            } catch (...) {
                finalResult = Core::Result<std::shared_ptr<PackArchive>>::failure(
                    Core::Error::ErrorCode::Unknown,
                    QCoreApplication::translate("PackArchivePool", "unknown exception while opening pack archive"));
            }

            // Publish result to waiters on this entry
            {
                std::lock_guard<std::mutex> entryLock(entry->mutex);
                entry->result = finalResult;
                entry->done = true;
            }
            entry->cv.notify_all();

            // Commit to global cache and remove from opening map
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_opening.find(normalizedKey);
                if (it != m_opening.end() && it.value() == entry) {
                    m_opening.erase(it);
                    if (finalResult.isSuccess()) {
                        m_archives.insert(normalizedKey, finalResult.value());
                    }
                }
            }

            return finalResult;
        } else {
            // Wait for initiator to finish opening
            std::unique_lock<std::mutex> entryLock(entry->mutex);
            while (!entry->done) {
                if (token.isCancelled()) {
                    return Core::Result<std::shared_ptr<PackArchive>>::cancelled(
                        QCoreApplication::translate("PackArchivePool", "archive open cancelled"));
                }
                entry->cv.wait_for(entryLock, std::chrono::milliseconds(50));
            }

            if (token.isCancelled()) {
                return Core::Result<std::shared_ptr<PackArchive>>::cancelled(
                    QCoreApplication::translate("PackArchivePool", "archive open cancelled"));
            }

            // If the initiator was cancelled by its own token, but our token is NOT cancelled,
            // retry to either find it in m_archives or become the new initiator.
            if (entry->result.isCancelled()) {
                continue;
            }

            return entry->result;
        }
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
    m_opening.clear();
}

} // namespace Domain::Package
