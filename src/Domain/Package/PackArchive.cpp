#include "Domain/Package/PackArchive.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <variant>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLatin1Char>

#include "Core/Error/ExecutionGuard.h"
#include "Domain/Package/Internal/BspPakZipReader.h"

namespace {

constexpr std::uint32_t kVpkSignature = 0x55AA1234U;
constexpr std::uint16_t kVpkDirArchiveIndex = 0x7FFFU;
constexpr std::uint16_t kVpkEntryTerminator = 0xFFFFU;

#pragma pack(push, 1)
struct VpkHeaderV1 {
    std::uint32_t signature;
    std::uint32_t version;
    std::uint32_t treeSize;
};

struct VpkHeaderV2Ext {
    std::uint32_t fileDataSectionSize;
    std::uint32_t archiveMd5SectionSize;
    std::uint32_t otherMd5SectionSize;
    std::uint32_t signatureSectionSize;
};

struct VpkDirectoryEntry {
    std::uint32_t crc32;
    std::uint16_t preloadBytes;
    std::uint16_t archiveIndex;
    std::uint32_t entryOffset;
    std::uint32_t entryLength;
    std::uint16_t terminator;
};
#pragma pack(pop)

static_assert(sizeof(VpkHeaderV1) == 12);
static_assert(sizeof(VpkHeaderV2Ext) == 16);
static_assert(sizeof(VpkDirectoryEntry) == 18);

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

bool isTraversalOrAbsolutePath(const QString& normalized) {
    return normalized == QStringLiteral("..")
        || normalized.startsWith(QStringLiteral("../"))
        || normalized.contains(QStringLiteral("/../"))
        || normalized.endsWith(QStringLiteral("/.."))
        || QDir::isAbsolutePath(normalized);
}

bool isAsciiDigit(QChar c) noexcept {
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

QString resolveRedirectedDirVpkPath(const QString& inputPath) {
    if (inputPath.size() >= 8 && inputPath.endsWith(QStringLiteral(".vpk"), Qt::CaseInsensitive)) {
        const qsizetype suffixStart = inputPath.size() - 8;
        if (inputPath[suffixStart] == QLatin1Char('_')
            && isAsciiDigit(inputPath[suffixStart + 1])
            && isAsciiDigit(inputPath[suffixStart + 2])
            && isAsciiDigit(inputPath[suffixStart + 3])) {
            const QString dirCandidate = inputPath.left(suffixStart) + QStringLiteral("_dir.vpk");
            const QFileInfo dirInfo(dirCandidate);
            if (dirInfo.exists() && dirInfo.isFile()) {
                return dirCandidate;
            }
        }
    }
    return inputPath;
}

QString deriveVpkBasePrefix(const QString& effectivePath) {
    QString prefix = effectivePath;
    if (prefix.endsWith(QStringLiteral(".vpk"), Qt::CaseInsensitive)) {
        prefix.chop(4);
    }
    if (prefix.endsWith(QStringLiteral("_dir"), Qt::CaseInsensitive)) {
        prefix.chop(4);
    }
    return prefix;
}

bool readExactFromFile(QFile& file, char* dest, qint64 byteCount) {
    qint64 totalRead = 0;
    while (totalRead < byteCount) {
        const qint64 chunk = file.read(dest + totalRead, byteCount - totalRead);
        if (chunk <= 0) {
            return false;
        }
        totalRead += chunk;
    }
    return true;
}

struct VpkEntryRecord {
    QString rawPath;
    std::uint16_t archiveIndex = 0;
    std::uint32_t entryOffset = 0;
    std::uint32_t entryLength = 0;
    std::vector<std::byte> preloadData;
};

class VpkReader {
public:
    static Core::Result<VpkReader> open(
        const Core::Path::FilesystemPath& archivePath,
        const Core::Async::CancellationToken& token) {
        const QString rawPath = archivePath.toString();
        const QString redirectedPath = resolveRedirectedDirVpkPath(rawPath);

        if (redirectedPath != rawPath) {
            auto dirResult = openSingleFile(redirectedPath, token);
            if (dirResult.isSuccess() || dirResult.isCancelled()) {
                return dirResult;
            }
        }

        return openSingleFile(rawPath, token);
    }

    std::vector<QString> listEntries() const {
        std::vector<QString> result;
        result.reserve(m_entries.size());
        for (const auto& entry : m_entries) {
            result.push_back(entry.rawPath);
        }
        return result;
    }

    bool hasEntry(const QString& normalizedPath) const {
        return m_lowerPathToIndex.constFind(normalizedPath.toLower()) != m_lowerPathToIndex.constEnd();
    }

    Core::Result<std::vector<std::byte>> readEntry(
        const QString& normalizedPath,
        const QString& originalEntryPath) const {
        const auto it = m_lowerPathToIndex.constFind(normalizedPath.toLower());
        if (it == m_lowerPathToIndex.constEnd()) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::EntryNotFound,
                QCoreApplication::translate("PackArchive", "entry not found in pack archive"),
                originalEntryPath);
        }

        const VpkEntryRecord& record = m_entries[it.value()];
        const std::size_t preloadSize = record.preloadData.size();
        const std::size_t externalSize = static_cast<std::size_t>(record.entryLength);

        if (externalSize == 0) {
            return Core::Result<std::vector<std::byte>>::success(record.preloadData);
        }

        QString sourceFilePath;
        std::uint64_t readOffset = 0;
        if (record.archiveIndex == kVpkDirArchiveIndex) {
            sourceFilePath = m_dirFilePath;
            readOffset = m_dataSectionOffset + static_cast<std::uint64_t>(record.entryOffset);
        } else {
            sourceFilePath = volumePath(record.archiveIndex);
            readOffset = static_cast<std::uint64_t>(record.entryOffset);
        }

        // Open a local read-only QFile on the stack so concurrent workers sharing
        // this PackArchive via PackArchivePool never contend on file handles/cursors.
        QFile dataFile(sourceFilePath);
        if (!dataFile.open(QIODevice::ReadOnly)) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive volume"),
                sourceFilePath);
        }

        const qint64 volumeSize = dataFile.size();
        if (volumeSize < 0
            || readOffset > static_cast<std::uint64_t>(volumeSize)
            || static_cast<std::uint64_t>(externalSize) > static_cast<std::uint64_t>(volumeSize) - readOffset) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("PackArchive", "pack entry range exceeds archive bounds"),
                originalEntryPath);
        }

        if (!dataFile.seek(static_cast<qint64>(readOffset))) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::ReadFailed,
                QCoreApplication::translate("PackArchive", "failed to seek pack archive entry"),
                sourceFilePath);
        }

        std::vector<std::byte> out;
        out.resize(preloadSize + externalSize);
        if (preloadSize > 0) {
            std::memcpy(out.data(), record.preloadData.data(), preloadSize);
        }

        if (!readExactFromFile(
                dataFile,
                reinterpret_cast<char*>(out.data() + preloadSize),
                static_cast<qint64>(externalSize))) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("PackArchive", "failed to read pack archive entry"),
                sourceFilePath);
        }

        return Core::Result<std::vector<std::byte>>::success(std::move(out));
    }

private:
    QString volumePath(std::uint16_t archiveIndex) const {
        return QStringLiteral("%1_%2.vpk")
            .arg(m_basePrefix)
            .arg(archiveIndex, 3, 10, QLatin1Char('0'));
    }

    static Core::Result<VpkReader> openSingleFile(
        const QString& filePath,
        const Core::Async::CancellationToken& token) {
        if (token.isCancelled()) {
            return Core::Result<VpkReader>::cancelled(
                QCoreApplication::translate("PackArchive", "pack archive open cancelled"));
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        const qint64 fileSize = file.size();
        if (fileSize < static_cast<qint64>(sizeof(VpkHeaderV1))) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        VpkHeaderV1 hdr1{};
        if (!readExactFromFile(file, reinterpret_cast<char*>(&hdr1), sizeof(hdr1))) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        if (hdr1.signature != kVpkSignature || (hdr1.version != 1U && hdr1.version != 2U)) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        qint64 headerSize = static_cast<qint64>(sizeof(VpkHeaderV1));
        if (hdr1.version == 2U) {
            headerSize += static_cast<qint64>(sizeof(VpkHeaderV2Ext));
            if (fileSize < headerSize) {
                return Core::Result<VpkReader>::failure(
                    Core::Error::ErrorCode::ArchiveOpenFailed,
                    QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                    filePath);
            }
            VpkHeaderV2Ext hdr2{};
            if (!readExactFromFile(file, reinterpret_cast<char*>(&hdr2), sizeof(hdr2))) {
                return Core::Result<VpkReader>::failure(
                    Core::Error::ErrorCode::ArchiveOpenFailed,
                    QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                    filePath);
            }
        }

        if (hdr1.treeSize == 0U) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        const std::uint64_t dataSectionOffset =
            static_cast<std::uint64_t>(headerSize) + static_cast<std::uint64_t>(hdr1.treeSize);
        if (dataSectionOffset > static_cast<std::uint64_t>(fileSize)) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        QByteArray treeBuf(static_cast<qsizetype>(hdr1.treeSize), Qt::Uninitialized);
        if (!readExactFromFile(file, treeBuf.data(), static_cast<qint64>(hdr1.treeSize))) {
            return Core::Result<VpkReader>::failure(
                Core::Error::ErrorCode::ArchiveOpenFailed,
                QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                filePath);
        }

        VpkReader reader;
        reader.m_dirFilePath = filePath;
        reader.m_basePrefix = deriveVpkBasePrefix(filePath);
        reader.m_dataSectionOffset = dataSectionOffset;

        const char* cursor = treeBuf.constData();
        const char* const end = cursor + treeBuf.size();

        auto readCString = [&](QByteArray& out) -> bool {
            if (cursor >= end) {
                return false;
            }
            const void* nul = std::memchr(cursor, '\0', static_cast<std::size_t>(end - cursor));
            if (!nul) {
                return false;
            }
            const char* nulPtr = static_cast<const char*>(nul);
            out = QByteArray(cursor, static_cast<qsizetype>(nulPtr - cursor));
            cursor = nulPtr + 1;
            return true;
        };

        while (true) {
            if (token.isCancelled()) {
                return Core::Result<VpkReader>::cancelled(
                    QCoreApplication::translate("PackArchive", "pack archive open cancelled"));
            }

            QByteArray extBytes;
            if (!readCString(extBytes)) {
                return Core::Result<VpkReader>::failure(
                    Core::Error::ErrorCode::ArchiveOpenFailed,
                    QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                    filePath);
            }
            if (extBytes.isEmpty()) {
                break;
            }

            const QString extStr = (extBytes == " ") ? QString() : QString::fromUtf8(extBytes);

            while (true) {
                QByteArray dirBytes;
                if (!readCString(dirBytes)) {
                    return Core::Result<VpkReader>::failure(
                        Core::Error::ErrorCode::ArchiveOpenFailed,
                        QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                        filePath);
                }
                if (dirBytes.isEmpty()) {
                    break;
                }

                const QString dirStr = (dirBytes == " ") ? QString() : QString::fromUtf8(dirBytes);

                while (true) {
                    QByteArray fileBytes;
                    if (!readCString(fileBytes)) {
                        return Core::Result<VpkReader>::failure(
                            Core::Error::ErrorCode::ArchiveOpenFailed,
                            QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                            filePath);
                    }
                    if (fileBytes.isEmpty()) {
                        break;
                    }

                    if (static_cast<std::size_t>(end - cursor) < sizeof(VpkDirectoryEntry)) {
                        return Core::Result<VpkReader>::failure(
                            Core::Error::ErrorCode::ArchiveOpenFailed,
                            QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                            filePath);
                    }

                    VpkDirectoryEntry dirEntry{};
                    std::memcpy(&dirEntry, cursor, sizeof(dirEntry));
                    cursor += sizeof(dirEntry);

                    if (dirEntry.terminator != kVpkEntryTerminator) {
                        return Core::Result<VpkReader>::failure(
                            Core::Error::ErrorCode::ArchiveOpenFailed,
                            QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                            filePath);
                    }

                    if (static_cast<std::size_t>(end - cursor) < dirEntry.preloadBytes) {
                        return Core::Result<VpkReader>::failure(
                            Core::Error::ErrorCode::ArchiveOpenFailed,
                            QCoreApplication::translate("PackArchive", "failed to open pack archive"),
                            filePath);
                    }

                    std::vector<std::byte> preloadData;
                    if (dirEntry.preloadBytes > 0U) {
                        preloadData.resize(dirEntry.preloadBytes);
                        std::memcpy(preloadData.data(), cursor, dirEntry.preloadBytes);
                        cursor += dirEntry.preloadBytes;
                    }

                    const QString fileStr = QString::fromUtf8(fileBytes);
                    QString rawPath = dirStr.isEmpty()
                        ? fileStr
                        : (dirStr + QLatin1Char('/') + fileStr);
                    if (!extStr.isEmpty()) {
                        rawPath += QLatin1Char('.') + extStr;
                    }
                    rawPath = normalizeEntryPath(rawPath);
                    if (rawPath.isEmpty()) {
                        continue;
                    }

                    const QString lowerKey = rawPath.toLower();
                    if (!reader.m_lowerPathToIndex.contains(lowerKey)) {
                        reader.m_lowerPathToIndex.insert(lowerKey, reader.m_entries.size());
                        VpkEntryRecord record;
                        record.rawPath = std::move(rawPath);
                        record.archiveIndex = dirEntry.archiveIndex;
                        record.entryOffset = dirEntry.entryOffset;
                        record.entryLength = dirEntry.entryLength;
                        record.preloadData = std::move(preloadData);
                        reader.m_entries.push_back(std::move(record));
                    }
                }
            }
        }

        return Core::Result<VpkReader>::success(std::move(reader));
    }

    QString m_dirFilePath;
    QString m_basePrefix;
    std::uint64_t m_dataSectionOffset = 0;
    std::vector<VpkEntryRecord> m_entries;
    QHash<QString, std::size_t> m_lowerPathToIndex;
};

} // namespace

namespace Domain::Package {

struct PackArchive::Impl {
    std::variant<VpkReader, Internal::BspPakZipReader> backend;
};

PackArchive::~PackArchive() = default;

PackArchive::PackArchive(PackArchive&& other) noexcept = default;

PackArchive& PackArchive::operator=(PackArchive&& other) noexcept = default;

Core::Result<PackArchive> PackArchive::open(
    const Core::Path::FilesystemPath& archivePath,
    const Core::Async::CancellationToken& token) {
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

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Opening pack archive"),
        .targetPath = archivePath.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<PackArchive> {
        if (archivePath.extension().compare(QStringLiteral("bsp"), Qt::CaseInsensitive) == 0) {
            auto bspRes = Internal::BspPakZipReader::open(archivePath, token);
            if (bspRes.isCancelled()) {
                return Core::Result<PackArchive>::cancelled(bspRes.message());
            }
            if (bspRes.isFailure()) {
                return Core::Result<PackArchive>::failure(bspRes.error());
            }

            auto impl = std::make_unique<Impl>();
            if (bspRes.isSuccess()) {
                impl->backend = std::move(bspRes.value());
            } else {
                // Skipped (no PAKFILE or empty PAKFILE): match prior PakLump empty archive behavior.
                impl->backend = Internal::BspPakZipReader{};
            }

            PackArchive archive;
            archive.m_impl = std::move(impl);
            return Core::Result<PackArchive>::success(std::move(archive));
        }

        auto vpkRes = VpkReader::open(archivePath, token);
        if (vpkRes.isCancelled()) {
            return Core::Result<PackArchive>::cancelled(vpkRes.message());
        }
        if (vpkRes.isFailure()) {
            return Core::Result<PackArchive>::failure(vpkRes.error());
        }

        auto impl = std::make_unique<Impl>();
        impl->backend = std::move(vpkRes.value());

        PackArchive archive;
        archive.m_impl = std::move(impl);
        return Core::Result<PackArchive>::success(std::move(archive));
    }, ctx);
}

bool PackArchive::isOpen() const noexcept {
    return m_impl != nullptr;
}

Core::Result<std::vector<QString>> PackArchive::listEntries() const {
    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Listing pack entries")
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<std::vector<QString>> {
        if (!m_impl) {
            return Core::Result<std::vector<QString>>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }

        std::vector<QString> entries;
        if (const auto* vpk = std::get_if<VpkReader>(&m_impl->backend)) {
            entries = vpk->listEntries();
        } else {
            const auto& bsp = std::get<Internal::BspPakZipReader>(m_impl->backend);
            const auto& bspEntries = bsp.entries();
            entries.reserve(bspEntries.size());
            for (const auto& entry : bspEntries) {
                entries.push_back(entry.rawPath);
            }
        }
        return Core::Result<std::vector<QString>>::success(std::move(entries));
    }, ctx);
}

Core::Result<bool> PackArchive::hasEntry(
    const QString& entryPath,
    const Core::Async::CancellationToken& token) const {
    if (token.isCancelled()) {
        return Core::Result<bool>::cancelled(
            QCoreApplication::translate("PackArchive", "check pack entry cancelled"));
    }
    if (!m_impl) {
        return Core::Result<bool>::failure(
            Core::Error::ErrorCode::InvalidState,
            QCoreApplication::translate("PackArchive", "pack archive is not open"));
    }
    if (entryPath.isEmpty()) {
        return Core::Result<bool>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("PackArchive", "entry path is empty"));
    }

    const QString normalized = normalizeEntryPath(entryPath);
    if (isTraversalOrAbsolutePath(normalized)) {
        return Core::Result<bool>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("PackArchive", "entry path is invalid or traverses outside directory"),
            entryPath);
    }

    bool found = false;
    if (const auto* vpk = std::get_if<VpkReader>(&m_impl->backend)) {
        found = vpk->hasEntry(normalized);
    } else {
        found = std::get<Internal::BspPakZipReader>(m_impl->backend).hasEntry(normalized);
    }
    return Core::Result<bool>::success(found);
}

Core::Result<std::vector<std::byte>> PackArchive::readEntry(const QString& entryPath) const {
    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Reading pack entry"),
        .resourcePath = entryPath
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<std::vector<std::byte>> {
        if (!m_impl) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }
        if (entryPath.isEmpty()) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("PackArchive", "entry path is empty"));
        }

        const QString normalized = normalizeEntryPath(entryPath);
        if (isTraversalOrAbsolutePath(normalized)) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "entry path is invalid or traverses outside directory"),
                entryPath);
        }

        if (const auto* vpk = std::get_if<VpkReader>(&m_impl->backend)) {
            return vpk->readEntry(normalized, entryPath);
        }
        return std::get<Internal::BspPakZipReader>(m_impl->backend).readEntry(normalized);
    }, ctx);
}

Core::Result<void> PackArchive::extractEntryToFile(
    const QString& entryPath,
    const Core::Path::FilesystemPath& destFile,
    const Core::Async::CancellationToken& token,
    const Core::Path::FilesystemPath& expectedBaseDir) const {
    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Extracting pack entry to file"),
        .resourcePath = entryPath,
        .targetPath = destFile.toString()
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        if (!m_impl) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("PackArchive", "pack archive is not open"));
        }
        if (destFile.isEmpty() || !destFile.isValid()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "destination file path is empty or invalid"));
        }
        if (entryPath.isEmpty()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate("PackArchive", "entry path is empty"));
        }

        const QString normalized = normalizeEntryPath(entryPath);
        if (isTraversalOrAbsolutePath(normalized)) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "entry path is invalid or traverses outside directory"),
                entryPath);
        }

        auto dataRes = (std::get_if<VpkReader>(&m_impl->backend) != nullptr)
            ? std::get<VpkReader>(m_impl->backend).readEntry(normalized, entryPath)
            : std::get<Internal::BspPakZipReader>(m_impl->backend).readEntry(normalized);
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        if (dataRes.isFailure()) {
            return Core::Result<void>::failure(dataRes.error());
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

        if (!expectedBaseDir.isEmpty() && !Core::Path::FilesystemPath::verifyFileWithinBase(outFile, expectedBaseDir)) {
            outFile.close();
            outFile.remove();
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "destination file escapes base directory via reparse point"),
                destFile.toString());
        }

        const auto& data = dataRes.value();
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

            const qint64 toWrite = std::min(ChunkSize, totalBytes - totalWritten);
            const qint64 written = outFile.write(reinterpret_cast<const char*>(data.data() + totalWritten), toWrite);
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
    }, ctx);
}

Core::Result<void> PackArchive::extractAllToDirectory(
    const Core::Path::FilesystemPath& destDir,
    const Core::Async::CancellationToken& token) const {
    if (token.isCancelled()) {
        return Core::Result<void>::cancelled(
            QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
    }
    if (!m_impl) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidState,
            QCoreApplication::translate("PackArchive", "pack archive is not open"));
    }
    if (destDir.isEmpty() || !destDir.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("PackArchive", "destination directory path is empty or invalid"));
    }

    auto entriesRes = listEntries();
    if (entriesRes.isFailure()) {
        return Core::Result<void>::failure(entriesRes.error());
    }

    for (const QString& entry : entriesRes.value()) {
        if (token.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("PackArchive", "pack extraction cancelled"));
        }
        auto destFileOpt = destDir.resolveBelow(entry);
        if (!destFileOpt.has_value()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate("PackArchive", "archive entry traverses outside destination directory"),
                entry);
        }
        auto extractRes = extractEntryToFile(entry, *destFileOpt, token, destDir);
        if (extractRes.isCancelled() || extractRes.isFailure()) {
            return extractRes;
        }
    }
    return Core::Result<void>::success();
}

} // namespace Domain::Package
