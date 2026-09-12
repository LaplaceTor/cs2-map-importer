#include <QCoreApplication>
#include "Domain/Package/BspPackExtractor.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include <bsppp/BSP.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>

#include "Core/Error/ErrorCode.h"

namespace Domain::Package {

Core::Result<std::size_t> BspPackExtractor::extractAll(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& destDir,
    const BspExtractOptions& options) {
    if (bspPath.isEmpty() || !bspPath.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspPackExtractor", "BSP file path is empty or invalid"));
    }
    if (!bspPath.exists()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("BspPackExtractor", "BSP file not found"),
            bspPath.toString());
    }
    if (destDir.isEmpty() || !destDir.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspPackExtractor", "destination directory is empty or invalid"));
    }

    if (options.isCancelled && options.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), 0);
    }

    bsppp::BSP reader{bspPath.toString().toStdString()};
    if (!reader) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspPackExtractor", "BSP file failed to load or has an invalid signature"),
            bspPath.toString());
    }

    if (!reader.hasLump(bsppp::BSPLump::PAKFILE)) {
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    auto pakDataOpt = reader.getLumpData(bsppp::BSPLump::PAKFILE);
    if (!pakDataOpt || pakDataOpt->empty()) {
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    const auto& pakData = *pakDataOpt;

    // Open read-only memory stream to scan Central Directory
    void* memStream = mz_stream_mem_create();
    mz_stream_mem_set_buffer(memStream, const_cast<std::byte*>(pakData.data()), static_cast<int32_t>(pakData.size()));
    if (mz_stream_mem_open(memStream, nullptr, MZ_OPEN_MODE_READ) != MZ_OK) {
        mz_stream_mem_delete(&memStream);
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    void* zipHandle = mz_zip_create();
    if (mz_zip_open(zipHandle, memStream, MZ_OPEN_MODE_READ) != MZ_OK) {
        mz_zip_delete(&zipHandle);
        mz_stream_mem_close(memStream);
        mz_stream_mem_delete(&memStream);
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    struct EntryInfo {
        int64_t cdPos;
        QString relativePath;
        int64_t uncompSize;
    };

    std::vector<EntryInfo> entriesToExtract;
    std::unordered_set<QString> uniqueDirs;

    int32_t err = mz_zip_goto_first_entry(zipHandle);
    while (err == MZ_OK) {
        if (mz_zip_entry_is_dir(zipHandle) != MZ_OK) {
            mz_zip_file* fileInfo = nullptr;
            if (mz_zip_entry_get_info(zipHandle, &fileInfo) == MZ_OK && fileInfo && fileInfo->filename) {
                QString rawPath = QString::fromUtf8(fileInfo->filename);
                rawPath.replace(u'\\', u'/');
                while (rawPath.startsWith(u'/')) {
                    rawPath.remove(0, 1);
                }
                if (!rawPath.isEmpty() && !rawPath.contains(QStringLiteral(".."))) {
                    int lastSlash = rawPath.lastIndexOf(u'/');
                    if (lastSlash > 0) {
                        uniqueDirs.insert(rawPath.left(lastSlash));
                    }
                    entriesToExtract.push_back(EntryInfo{
                        .cdPos = mz_zip_get_entry(zipHandle),
                        .relativePath = rawPath,
                        .uncompSize = fileInfo->uncompressed_size
                    });
                }
            }
        }
        err = mz_zip_goto_next_entry(zipHandle);
    }

    mz_zip_close(zipHandle);
    mz_zip_delete(&zipHandle);
    mz_stream_mem_close(memStream);
    mz_stream_mem_delete(&memStream);

    if (entriesToExtract.empty()) {
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    if (options.isCancelled && options.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), 0);
    }

    // Phase 1: Pre-create unique directories single-threaded
    QDir targetBaseDir(destDir.toString());
    if (!targetBaseDir.exists()) {
        targetBaseDir.mkpath(QStringLiteral("."));
    }
    for (const QString& subDir : uniqueDirs) {
        targetBaseDir.mkpath(subDir);
    }

    // Phase 2: Multi-threaded in-memory extraction
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    const unsigned int maxWorkers = std::clamp(hardwareThreads, 2u, 16u);
    const unsigned int numWorkers = std::min<unsigned int>(
        maxWorkers, static_cast<unsigned int>(entriesToExtract.size()));

    std::atomic<std::size_t> nextIndex{0};
    std::atomic<std::size_t> successCount{0};
    std::atomic<bool> cancelRequested{false};

    auto workerFunc = [&]() {
        void* localMemStream = mz_stream_mem_create();
        mz_stream_mem_set_buffer(localMemStream, const_cast<std::byte*>(pakData.data()), static_cast<int32_t>(pakData.size()));
        if (mz_stream_mem_open(localMemStream, nullptr, MZ_OPEN_MODE_READ) != MZ_OK) {
            mz_stream_mem_delete(&localMemStream);
            return;
        }

        void* localZip = mz_zip_create();
        if (mz_zip_open(localZip, localMemStream, MZ_OPEN_MODE_READ) != MZ_OK) {
            mz_zip_delete(&localZip);
            mz_stream_mem_close(localMemStream);
            mz_stream_mem_delete(&localMemStream);
            return;
        }

        std::vector<std::byte> buffer;

        while (true) {
            if (cancelRequested.load(std::memory_order_relaxed)) {
                break;
            }
            if (options.isCancelled && options.isCancelled()) {
                cancelRequested.store(true, std::memory_order_relaxed);
                break;
            }

            std::size_t idx = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if (idx >= entriesToExtract.size()) {
                break;
            }

            const auto& entry = entriesToExtract[idx];

            if (mz_zip_goto_entry(localZip, entry.cdPos) != MZ_OK) {
                continue;
            }

            if (mz_zip_entry_read_open(localZip, 0, nullptr) != MZ_OK) {
                continue;
            }

            buffer.resize(static_cast<std::size_t>(std::max<int64_t>(0, entry.uncompSize)));
            int32_t bytesRead = 0;
            if (entry.uncompSize > 0) {
                bytesRead = mz_zip_entry_read(localZip, buffer.data(), static_cast<int32_t>(buffer.size()));
            }
            mz_zip_entry_close(localZip);

            if (bytesRead < 0 || (entry.uncompSize > 0 && bytesRead != entry.uncompSize)) {
                continue;
            }

            const QString fullTarget = destDir.toString() + u'/' + entry.relativePath;
            QFile outFile(fullTarget);
            if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (entry.uncompSize > 0) {
                    outFile.write(reinterpret_cast<const char*>(buffer.data()), static_cast<qint64>(entry.uncompSize));
                }
                outFile.close();
                auto currentSuccess = successCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (options.onProgress) {
                    options.onProgress(currentSuccess, entriesToExtract.size());
                }
            }
        }

        mz_zip_close(localZip);
        mz_zip_delete(&localZip);
        mz_stream_mem_close(localMemStream);
        mz_stream_mem_delete(&localMemStream);
    };

    std::vector<std::thread> workers;
    workers.reserve(numWorkers);
    for (unsigned int w = 0; w < numWorkers; ++w) {
        workers.emplace_back(workerFunc);
    }
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::size_t extracted = successCount.load(std::memory_order_relaxed);

    if (cancelRequested.load(std::memory_order_relaxed)) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), extracted);
    }

    return Core::Result<std::size_t>::success(extracted);
}

} // namespace Domain::Package

