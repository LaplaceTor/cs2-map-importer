#include <QCoreApplication>
#include "Domain/Package/BspPackExtractor.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <unordered_set>

#include <QDir>
#include <QFile>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include "Core/Error/ErrorCode.h"
#include "Domain/Package/Internal/BspPakZipReader.h"

namespace Domain::Package {

Core::Result<std::size_t> BspPackExtractor::extractAll(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& destDir,
    const BspExtractOptions& options) {
    if (options.token.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), 0);
    }
    if (destDir.isEmpty() || !destDir.isValid()) {
        return Core::Result<std::size_t>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspPackExtractor", "destination directory is empty or invalid"));
    }

    auto openRes = Internal::BspPakZipReader::open(bspPath, options.token);
    if (openRes.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(openRes.message(), 0);
    }
    if (openRes.isSkipped()) {
        return Core::Result<std::size_t>::skipped(openRes.message());
    }
    if (openRes.isFailure()) {
        return Core::Result<std::size_t>::failure(openRes.error(), openRes.message(), 0);
    }

    const auto& reader = openRes.value();
    const auto& entries = reader.entries();
    if (entries.empty()) {
        return Core::Result<std::size_t>::skipped(
            QCoreApplication::translate("BspPackExtractor", "BSP contains no embedded files"));
    }

    if (options.token.isCancelled()) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), 0);
    }

    // Phase 1: Pre-create unique directories single-threaded
    std::unordered_set<QString> uniqueDirs;
    for (const auto& entry : entries) {
        const qsizetype lastSlash = entry.rawPath.lastIndexOf(u'/');
        if (lastSlash > 0) {
            uniqueDirs.insert(entry.rawPath.left(lastSlash));
        }
    }

    QDir targetBaseDir(destDir.toString());
    if (!targetBaseDir.exists()) {
        targetBaseDir.mkpath(QStringLiteral("."));
    }
    for (const QString& subDir : uniqueDirs) {
        auto subOpt = destDir.resolveBelow(subDir);
        if (subOpt.has_value()) {
            QDir(subOpt->toString()).mkpath(QStringLiteral("."));
        }
    }

    // Phase 2: Bounded concurrent in-memory extraction via a local QThreadPool
    // (local pool avoids deadlocking if extractAll itself runs on QThreadPool::globalInstance()).
    const int idealThreads = std::max(1, QThread::idealThreadCount());
    const int maxWorkers = std::clamp(idealThreads, 2, 8);
    const int numWorkers = std::min<int>(
        maxWorkers, static_cast<int>(entries.size()));

    std::atomic<std::size_t> nextIndex{0};
    std::atomic<std::size_t> successCount{0};
    std::atomic<bool> cancelRequested{false};

    auto workerFunc = [&]() {
        while (true) {
            if (cancelRequested.load(std::memory_order_relaxed)) {
                break;
            }
            if (options.token.isCancelled()) {
                cancelRequested.store(true, std::memory_order_relaxed);
                break;
            }

            const std::size_t idx = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if (idx >= entries.size()) {
                break;
            }

            const auto& entry = entries[idx];
            auto targetPathOpt = destDir.resolveBelow(entry.rawPath);
            if (!targetPathOpt.has_value()) {
                continue;
            }

            auto dataRes = reader.readEntry(idx);
            if (dataRes.isFailure()) {
                continue;
            }
            const auto& data = dataRes.value();

            QFile outFile(targetPathOpt->toString());
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                continue;
            }

            if (!Core::Path::FilesystemPath::verifyFileWithinBase(outFile, destDir)) {
                outFile.close();
                outFile.remove();
                continue;
            }

            bool writeOk = true;
            if (!data.empty()) {
                const qint64 totalBytes = static_cast<qint64>(data.size());
                const qint64 written = outFile.write(
                    reinterpret_cast<const char*>(data.data()),
                    totalBytes);
                writeOk = (written == totalBytes);
            }
            outFile.close();
            if (!writeOk) {
                outFile.remove();
                continue;
            }

            const std::size_t currentSuccess = successCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (options.onProgress) {
                options.onProgress(currentSuccess, entries.size());
            }
        }
    };

    if (numWorkers <= 1) {
        workerFunc();
    } else {
        QThreadPool localPool;
        localPool.setMaxThreadCount(numWorkers);
        for (int w = 0; w < numWorkers; ++w) {
            localPool.start(workerFunc);
        }
        localPool.waitForDone();
    }

    const std::size_t extracted = successCount.load(std::memory_order_relaxed);
    if (cancelRequested.load(std::memory_order_relaxed)) {
        return Core::Result<std::size_t>::cancelled(
            QCoreApplication::translate("BspPackExtractor", "BSP embedded file extraction cancelled"), extracted);
    }

    return Core::Result<std::size_t>::success(extracted);
}

} // namespace Domain::Package
