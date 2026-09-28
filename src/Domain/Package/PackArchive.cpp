#include <QCoreApplication>
#include "Domain/Package/PackArchive.h"

#include <exception>
#include <utility>

// bsppp/PakLump.h registers the .bsp open factory; including it here makes
// vpkpp::PackFile::open transparently handle BSP embedded packs.
#include <bsppp/PakLump.h>
#include <vpkpp/PackFile.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "Core/Error/Exception.h"

namespace {

/**
 * @brief Exception boundary: translates Core/third-party/std exceptions into
 *        structured Result failures instead of letting them escape Domain.
 */
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

/**
 * @brief Normalizes a caller-provided entry path to pack-relative form
 *        ('/' separators, no leading slash).
 */
QString normalizeEntryPath(const QString& entryPath) {
    QString normalized = entryPath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    return normalized;
}

} // namespace

namespace Domain::Package {

PackArchive::~PackArchive() = default;

PackArchive::PackArchive(PackArchive&& other) noexcept = default;

PackArchive& PackArchive::operator=(PackArchive&& other) noexcept = default;

Core::Result<PackArchive> PackArchive::open(const Core::Path::FilesystemPath& archivePath, const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        return Core::Result<PackArchive>::cancelled(
            QCoreApplication::translate("PackArchive", "pack archive open cancelled"));
    }
    if (archivePath.isEmpty() || !archivePath.isValid()) {
        return Core::Result<PackArchive>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("PackArchive", "pack archive path is empty or invalid"));
    }
    if (!archivePath.exists()) {
        return Core::Result<PackArchive>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("PackArchive", "pack archive file not found"),
            archivePath.toString());
    }

    if (token.isCancelled()) {
        return Core::Result<PackArchive>::cancelled(
            QCoreApplication::translate("PackArchive", "pack archive open cancelled"));
    }

    // vpkpp interprets std::string paths in the system's native narrow encoding;
    // non-ASCII paths are a known sourcepp limitation and fail cleanly here.
    auto packFile = vpkpp::PackFile::open(archivePath.toString().toStdString());
    if (token.isCancelled()) {
        return Core::Result<PackArchive>::cancelled(
            QCoreApplication::translate("PackArchive", "pack archive open cancelled"));
    }
    if (!packFile) {
        return Core::Result<PackArchive>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("PackArchive", "file is not a supported pack archive or failed to parse"),
            archivePath.toString());
    }

    PackArchive archive;
    archive.m_packFile = std::move(packFile);
    return Core::Result<PackArchive>::success(std::move(archive));
}

bool PackArchive::isOpen() const noexcept {
    return m_packFile != nullptr;
}

Core::Result<std::vector<QString>> PackArchive::listEntries() const {
    return runGuarded([&]() -> Core::Result<std::vector<QString>> {
        if (!m_packFile) {
            return Core::Result<std::vector<QString>>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }

        std::vector<QString> entries;
        m_packFile->runForAllEntries([&entries](const std::string& path, const vpkpp::Entry&) {
            entries.push_back(QString::fromStdString(path));
        });
        return Core::Result<std::vector<QString>>::success(std::move(entries));
    });
}

bool PackArchive::hasEntry(const QString& entryPath, const Core::Async::CancellationToken& token) const {
    if (token.isCancelled() || !m_packFile) {
        return false;
    }
    return m_packFile->hasEntry(normalizeEntryPath(entryPath).toStdString());
}

Core::Result<std::vector<std::byte>> PackArchive::readEntry(const QString& entryPath) const {
    return runGuarded([&]() -> Core::Result<std::vector<std::byte>> {
        if (!m_packFile) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }

        auto data = m_packFile->readEntry(normalizeEntryPath(entryPath).toStdString());
        if (!data) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QCoreApplication::translate("PackArchive", "entry not found in pack archive"),
                entryPath);
        }
        return Core::Result<std::vector<std::byte>>::success(std::move(*data));
    });
}

Core::Result<void> PackArchive::extractEntryToFile(
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile,
    const Core::Async::CancellationToken& token) const {
    return runGuarded([&]() -> Core::Result<void> {
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        if (!m_packFile) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }
        if (destFile.isEmpty() || !destFile.isValid()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "destination file path is empty or invalid"));
        }

        const QString normalized = normalizeEntryPath(entryPath);
        if (!m_packFile->hasEntry(normalized.toStdString())) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QCoreApplication::translate("PackArchive", "entry not found in pack archive"),
                entryPath);
        }

        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }

        auto dataOpt = m_packFile->readEntry(normalized.toStdString());
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        if (!dataOpt) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QCoreApplication::translate("PackArchive", "entry not found in pack archive"),
                entryPath);
        }

        QFileInfo destInfo(destFile.toString());
        QDir parentDir = destInfo.dir();
        if (!parentDir.exists() && !QDir().mkpath(parentDir.absolutePath())) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::WriteFailed,
                QCoreApplication::translate("PackArchive", "failed to create destination directory"),
                destFile.toString());
        }

        QFile outFile(destFile.toString());
        if (!outFile.open(QIODevice::WriteOnly)) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::WriteFailed,
                QCoreApplication::translate("PackArchive", "failed to open destination file for writing"),
                destFile.toString());
        }

        const auto& data = *dataOpt;
        constexpr qint64 ChunkSize = 64 * 1024;
        qint64 totalWritten = 0;
        const qint64 totalBytes = static_cast<qint64>(data.size());

        while (totalWritten < totalBytes) {
            if (token.isCancelled()) {
                outFile.close();
                outFile.remove();
                return Core::Result<void>::cancelled(
                    QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
            }

            qint64 toWrite = std::min(ChunkSize, totalBytes - totalWritten);
            qint64 written = outFile.write(reinterpret_cast<const char*>(data.data() + totalWritten), toWrite);
            if (written != toWrite) {
                outFile.close();
                outFile.remove();
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("PackArchive", "failed to extract entry to destination file"),
                    destFile.toString());
            }
            totalWritten += written;
        }

        return Core::Result<void>::success();
    });
}

Core::Result<void> PackArchive::extractAllToDirectory(
    const Core::Path::FilesystemPath& destDir,
    const Core::Async::CancellationToken& token) const {
    return runGuarded([&]() -> Core::Result<void> {
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        if (!m_packFile) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }
        if (destDir.isEmpty() || !destDir.isValid()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "destination directory path is empty or invalid"));
        }

        std::vector<QString> allEntries;
        m_packFile->runForAllEntries([&allEntries](const std::string& path, const vpkpp::Entry&) {
            allEntries.push_back(QString::fromStdString(path));
        });

        for (const QString& entry : allEntries) {
            if (token.isCancelled()) {
                return Core::Result<void>::cancelled(
                    QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
            }
            auto extractRes = extractEntryToFile(entry, destDir / entry, token);
            if (extractRes.isCancelled()) {
                return extractRes;
            }
            if (extractRes.isFailure()) {
                return extractRes;
            }
        }
        return Core::Result<void>::success();
    });
}

} // namespace Domain::Package
