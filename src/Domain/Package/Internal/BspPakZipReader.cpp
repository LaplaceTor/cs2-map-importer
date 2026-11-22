#include "Domain/Package/Internal/BspPakZipReader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

#include <QCoreApplication>
#include <QFile>
#include <QStringView>
#include <QtZlib/zlib.h>
#ifdef crc32
#undef crc32
#endif

#include "Core/Error/ErrorCode.h"
#include "Domain/Package/Internal/Lzma1Decoder.h"

namespace Domain::Package::Internal {
namespace {

constexpr std::uint32_t kVbspSignature = 0x50534256U; // "VBSP" little-endian
constexpr std::size_t kBspLumpCount = 64;
constexpr std::size_t kPakFileLumpIndex = 40;
constexpr std::size_t kLumpRecordSize = 16;
constexpr std::uint32_t kContagionBspVersion = 27;
constexpr std::uint32_t kL4d2BspVersion = 21;
constexpr std::uint32_t kMaxNormalHeaderEndOffset = 1024;

constexpr std::uint32_t kZipEocdSignature = 0x06054B50U;
constexpr std::uint32_t kZip64LocatorSignature = 0x07064B50U;
constexpr std::uint32_t kZip64EocdSignature = 0x06064B50U;
constexpr std::uint32_t kZipCdHeaderSignature = 0x02014B50U;
constexpr std::uint32_t kZipLocalHeaderSignature = 0x04034B50U;

constexpr std::uint16_t kZipMethodStore = 0;
constexpr std::uint16_t kZipMethodDeflate = 8;
constexpr std::uint16_t kZipMethodLzma = 14;

constexpr std::uint64_t kMaxUncompressedEntryBytes = 512ULL * 1024ULL * 1024ULL;

struct BspLump {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
    std::uint32_t version = 0;
    std::uint32_t uncompressedLength = 0;
};

struct ZStreamGuard {
    z_stream stream{};
    bool initialized = false;

    ~ZStreamGuard() {
        if (initialized) {
            inflateEnd(&stream);
        }
    }
};

std::uint16_t readU16LE(std::span<const std::byte> data, std::size_t offset) noexcept {
    const auto b0 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(data[offset]));
    const auto b1 = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(data[offset + 1]));
    return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

std::uint32_t readU32LE(std::span<const std::byte> data, std::size_t offset) noexcept {
    const auto b0 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[offset]));
    const auto b1 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[offset + 1]));
    const auto b2 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[offset + 2]));
    const auto b3 = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[offset + 3]));
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

std::uint64_t readU64LE(std::span<const std::byte> data, std::size_t offset) noexcept {
    const auto lo = static_cast<std::uint64_t>(readU32LE(data, offset));
    const auto hi = static_cast<std::uint64_t>(readU32LE(data, offset + 4));
    return lo | (hi << 32);
}

QString normalizeZipPath(QString path) {
    path.replace(u'\\', u'/');
    while (path.startsWith(u'/')) {
        path.remove(0, 1);
    }
    return path;
}

bool hasParentTraversalSegment(const QString& path) {
    qsizetype start = 0;
    while (start <= path.size()) {
        const qsizetype slash = path.indexOf(u'/', start);
        const qsizetype len = (slash < 0) ? (path.size() - start) : (slash - start);
        if (QStringView(path).sliced(start, len) == QLatin1String("..")) {
            return true;
        }
        if (slash < 0) {
            break;
        }
        start = slash + 1;
    }
    return false;
}

bool readExactFromFile(QFile& file, std::span<std::byte> dest) {
    std::size_t totalRead = 0;
    while (totalRead < dest.size()) {
        const qint64 chunk = file.read(
            reinterpret_cast<char*>(dest.data() + totalRead),
            static_cast<qint64>(dest.size() - totalRead));
        if (chunk <= 0) {
            return false;
        }
        totalRead += static_cast<std::size_t>(chunk);
    }
    return true;
}

Core::Result<std::vector<std::byte>> inflateDeflate(
    std::span<const std::byte> compressed,
    std::uint64_t uncompressedSize,
    const QString& entryPath) {
    if (uncompressedSize == 0 && compressed.empty()) {
        return Core::Result<std::vector<std::byte>>::success(std::vector<std::byte>{});
    }
    if (compressed.size() > (std::numeric_limits<uInt>::max)()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "Deflate stream exceeds maximum supported size"),
            entryPath);
    }

    std::vector<std::byte> out;
    std::byte zeroByteScratch{0};
    Bytef* outPtr = nullptr;
    uInt outAvail = 0;

    if (uncompressedSize > 0) {
        out.resize(static_cast<std::size_t>(uncompressedSize));
        outPtr = reinterpret_cast<Bytef*>(out.data());
        outAvail = static_cast<uInt>(out.size());
    } else {
        outPtr = reinterpret_cast<Bytef*>(&zeroByteScratch);
        outAvail = 1;
    }

    ZStreamGuard guard;
    guard.stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
    guard.stream.avail_in = static_cast<uInt>(compressed.size());
    guard.stream.next_out = outPtr;
    guard.stream.avail_out = outAvail;

    if (inflateInit2(&guard.stream, -MAX_WBITS) != Z_OK) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "Failed to initialize deflate stream"),
            entryPath);
    }
    guard.initialized = true;

    const int ret = inflate(&guard.stream, Z_FINISH);
    if (ret != Z_STREAM_END || guard.stream.total_out != uncompressedSize) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "Deflate decompression failed or truncated entry"),
            entryPath);
    }

    return Core::Result<std::vector<std::byte>>::success(std::move(out));
}

struct CentralDirectorySlice {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

Core::Result<CentralDirectorySlice> locateCentralDirectory(std::span<const std::byte> pakBytes) {
    const std::size_t size = pakBytes.size();
    if (size < 22) {
        return Core::Result<CentralDirectorySlice>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "BSP PAKFILE lump is smaller than minimum ZIP EOCD size"));
    }

    const std::size_t maxSearch = std::min<std::size_t>(size, 22ULL + 65535ULL);
    const std::size_t minPos = size - maxSearch;

    for (std::size_t pos = size - 22 + 1; pos > minPos;) {
        --pos;
        if (readU32LE(pakBytes, pos) != kZipEocdSignature) {
            continue;
        }

        const std::uint16_t commentLen = readU16LE(pakBytes, pos + 20);
        if (pos + 22 + commentLen > size) {
            continue;
        }

        const std::uint16_t diskNumber = readU16LE(pakBytes, pos + 4);
        const std::uint16_t cdStartDisk = readU16LE(pakBytes, pos + 6);
        if (diskNumber != 0 || cdStartDisk != 0) {
            continue;
        }

        std::uint64_t cdSize = readU32LE(pakBytes, pos + 12);
        std::uint64_t cdOffset = readU32LE(pakBytes, pos + 16);

        if ((cdSize == 0xFFFFFFFFULL || cdOffset == 0xFFFFFFFFULL) && pos >= 20) {
            const std::size_t locPos = pos - 20;
            if (readU32LE(pakBytes, locPos) == kZip64LocatorSignature) {
                const std::uint64_t zip64EocdOffset = readU64LE(pakBytes, locPos + 8);
                if (zip64EocdOffset + 56 <= locPos &&
                    readU32LE(pakBytes, static_cast<std::size_t>(zip64EocdOffset)) == kZip64EocdSignature) {
                    const auto z64 = static_cast<std::size_t>(zip64EocdOffset);
                    cdSize = readU64LE(pakBytes, z64 + 40);
                    cdOffset = readU64LE(pakBytes, z64 + 48);
                }
            }
        }

        if (cdOffset > pos || cdSize > pos - cdOffset) {
            continue;
        }

        if (cdSize > 0 && (cdOffset + 4 > pos || readU32LE(pakBytes, static_cast<std::size_t>(cdOffset)) != kZipCdHeaderSignature)) {
            continue;
        }

        return Core::Result<CentralDirectorySlice>::success(CentralDirectorySlice{
            .offset = cdOffset,
            .size = cdSize,
        });
    }

    return Core::Result<CentralDirectorySlice>::failure(
        Core::Error::ErrorCode::CorruptedData,
        QCoreApplication::translate("BspPakZipReader", "ZIP End of Central Directory not found in BSP PAKFILE"));
}

} // namespace

Core::Result<BspPakZipReader> BspPakZipReader::open(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Async::CancellationToken& token) {
    if (token.isCancelled()) {
        return Core::Result<BspPakZipReader>::cancelled(
            QCoreApplication::translate("BspPakZipReader", "BSP pack open cancelled"));
    }
    if (bspPath.isEmpty() || !bspPath.isValid()) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspPakZipReader", "BSP file path is empty or invalid"));
    }
    if (!bspPath.exists()) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("BspPakZipReader", "BSP file not found"),
            bspPath.toString());
    }

    QFile file(bspPath.toString());
    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("BspPakZipReader", "Failed to open BSP file"),
            bspPath.toString());
    }

    std::array<std::byte, 8> prefixBytes{};
    if (!readExactFromFile(file, prefixBytes)) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspPakZipReader", "BSP file header is truncated"),
            bspPath.toString());
    }

    const std::uint32_t signature = readU32LE(prefixBytes, 0);
    if (signature != kVbspSignature) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspPakZipReader", "Invalid VBSP signature"),
            bspPath.toString());
    }

    const std::uint32_t version = readU32LE(prefixBytes, 4);
    if (version == kContagionBspVersion) {
        std::array<std::byte, 4> contagionSkip{};
        if (!readExactFromFile(file, contagionSkip)) {
            return Core::Result<BspPakZipReader>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("BspPakZipReader", "BSP file header is truncated"),
                bspPath.toString());
        }
    }

    std::array<std::byte, kBspLumpCount * kLumpRecordSize> lumpTableBytes{};
    if (!readExactFromFile(file, lumpTableBytes)) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("BspPakZipReader", "BSP lump directory is truncated"),
            bspPath.toString());
    }

    std::array<BspLump, kBspLumpCount> lumps{};
    for (std::size_t i = 0; i < kBspLumpCount; ++i) {
        const std::size_t base = i * kLumpRecordSize;
        lumps[i].offset = readU32LE(lumpTableBytes, base);
        lumps[i].length = readU32LE(lumpTableBytes, base + 4);
        lumps[i].version = readU32LE(lumpTableBytes, base + 8);
        lumps[i].uncompressedLength = readU32LE(lumpTableBytes, base + 12);
    }

    // L4D2 BSP (version == 21) stores lump fields as (version, offset, length, uncompressedLength).
    // Because the BSP header itself is > 1024 bytes, all offsets <= 1024 identify L4D2 layout.
    if (version == kL4d2BspVersion) {
        const bool isL4d2Layout = std::all_of(lumps.begin(), lumps.end(), [](const BspLump& lump) {
            return lump.offset <= kMaxNormalHeaderEndOffset;
        });
        if (isL4d2Layout) {
            for (auto& lump : lumps) {
                const std::uint32_t oldOffset = lump.offset;
                const std::uint32_t oldLength = lump.length;
                const std::uint32_t oldVersion = lump.version;
                lump.offset = oldLength;
                lump.length = oldVersion;
                lump.version = oldOffset;
            }
        }
    }

    const BspLump& pakLump = lumps[kPakFileLumpIndex];
    if (pakLump.offset == 0 || pakLump.length == 0) {
        return Core::Result<BspPakZipReader>::skipped(
            QCoreApplication::translate("BspPakZipReader", "BSP contains no embedded files"));
    }

    const qint64 fileSize = file.size();
    const std::uint64_t pakEnd = static_cast<std::uint64_t>(pakLump.offset) + pakLump.length;
    if (fileSize < 0 || pakEnd > static_cast<std::uint64_t>(fileSize)) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "BSP PAKFILE lump exceeds file size"),
            bspPath.toString());
    }

    if (token.isCancelled()) {
        return Core::Result<BspPakZipReader>::cancelled(
            QCoreApplication::translate("BspPakZipReader", "BSP pack open cancelled"));
    }

    if (!file.seek(static_cast<qint64>(pakLump.offset))) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("BspPakZipReader", "Failed to seek to BSP PAKFILE lump"),
            bspPath.toString());
    }

    BspPakZipReader reader;
    reader.m_pakBytes.resize(pakLump.length);
    if (!readExactFromFile(file, reader.m_pakBytes)) {
        return Core::Result<BspPakZipReader>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("BspPakZipReader", "Failed to read BSP PAKFILE lump"),
            bspPath.toString());
    }
    file.close();

    if (token.isCancelled()) {
        return Core::Result<BspPakZipReader>::cancelled(
            QCoreApplication::translate("BspPakZipReader", "BSP pack open cancelled"));
    }

    if (Lzma1Decoder::hasValveLzmaMagic(reader.m_pakBytes)) {
        auto decompRes = Lzma1Decoder::decompressValveLump(reader.m_pakBytes);
        if (decompRes.isFailure()) {
            return Core::Result<BspPakZipReader>::failure(decompRes.error());
        }
        reader.m_pakBytes = std::move(decompRes.value());
    }

    if (reader.m_pakBytes.empty()) {
        return Core::Result<BspPakZipReader>::skipped(
            QCoreApplication::translate("BspPakZipReader", "BSP contains no embedded files"));
    }

    auto cdSliceRes = locateCentralDirectory(reader.m_pakBytes);
    if (cdSliceRes.isFailure()) {
        return Core::Result<BspPakZipReader>::failure(cdSliceRes.error());
    }

    const CentralDirectorySlice cdSlice = cdSliceRes.value();
    std::size_t cursor = static_cast<std::size_t>(cdSlice.offset);
    const std::size_t cdEnd = cursor + static_cast<std::size_t>(cdSlice.size);
    const std::span<const std::byte> pakSpan(reader.m_pakBytes);

    while (cursor < cdEnd) {
        if (token.isCancelled()) {
            return Core::Result<BspPakZipReader>::cancelled(
                QCoreApplication::translate("BspPakZipReader", "BSP pack open cancelled"));
        }
        if (cdEnd - cursor < 46 || readU32LE(pakSpan, cursor) != kZipCdHeaderSignature) {
            return Core::Result<BspPakZipReader>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("BspPakZipReader", "Corrupted ZIP Central Directory record in BSP PAKFILE"),
                bspPath.toString());
        }

        const std::uint16_t compressionMethod = readU16LE(pakSpan, cursor + 10);
        const std::uint32_t crc32 = readU32LE(pakSpan, cursor + 16);
        std::uint64_t compressedSize = readU32LE(pakSpan, cursor + 20);
        std::uint64_t uncompressedSize = readU32LE(pakSpan, cursor + 24);
        const std::uint16_t nameLen = readU16LE(pakSpan, cursor + 28);
        const std::uint16_t extraLen = readU16LE(pakSpan, cursor + 30);
        const std::uint16_t commentLen = readU16LE(pakSpan, cursor + 32);
        std::uint64_t localHeaderOffset = readU32LE(pakSpan, cursor + 42);

        const std::size_t tailLen = static_cast<std::size_t>(nameLen) + extraLen + commentLen;
        if (cdEnd - cursor - 46 < tailLen) {
            return Core::Result<BspPakZipReader>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("BspPakZipReader", "Truncated ZIP Central Directory entry in BSP PAKFILE"),
                bspPath.toString());
        }

        if (uncompressedSize == 0xFFFFFFFFULL || compressedSize == 0xFFFFFFFFULL || localHeaderOffset == 0xFFFFFFFFULL) {
            std::size_t extraCursor = cursor + 46 + nameLen;
            const std::size_t extraEnd = extraCursor + extraLen;
            while (extraEnd - extraCursor >= 4) {
                const std::uint16_t headerId = readU16LE(pakSpan, extraCursor);
                const std::uint16_t dataSize = readU16LE(pakSpan, extraCursor + 2);
                extraCursor += 4;
                if (extraEnd - extraCursor < dataSize) {
                    break;
                }
                if (headerId == 0x0001U) {
                    std::size_t fieldPos = extraCursor;
                    if (uncompressedSize == 0xFFFFFFFFULL && fieldPos + 8 <= extraCursor + dataSize) {
                        uncompressedSize = readU64LE(pakSpan, fieldPos);
                        fieldPos += 8;
                    }
                    if (compressedSize == 0xFFFFFFFFULL && fieldPos + 8 <= extraCursor + dataSize) {
                        compressedSize = readU64LE(pakSpan, fieldPos);
                        fieldPos += 8;
                    }
                    if (localHeaderOffset == 0xFFFFFFFFULL && fieldPos + 8 <= extraCursor + dataSize) {
                        localHeaderOffset = readU64LE(pakSpan, fieldPos);
                    }
                    break;
                }
                extraCursor += dataSize;
            }
        }

        QString rawPath = QString::fromUtf8(
            reinterpret_cast<const char*>(pakSpan.data() + cursor + 46),
            static_cast<qsizetype>(nameLen));
        cursor += 46 + tailLen;

        if (rawPath.contains(u'\0')) {
            continue;
        }
        rawPath = normalizeZipPath(std::move(rawPath));
        if (rawPath.isEmpty() || rawPath.endsWith(u'/') || hasParentTraversalSegment(rawPath)) {
            continue;
        }

        const QString lowerPath = rawPath.toLower();
        if (reader.m_lowerPathToIndex.contains(lowerPath)) {
            continue;
        }

        const std::size_t entryIdx = reader.m_entries.size();
        reader.m_lowerPathToIndex.insert(lowerPath, entryIdx);
        reader.m_entries.push_back(BspZipEntry{
            .rawPath = std::move(rawPath),
            .compressionMethod = compressionMethod,
            .crc32 = crc32,
            .compressedSize = compressedSize,
            .uncompressedSize = uncompressedSize,
            .localHeaderOffset = localHeaderOffset,
        });
    }

    return Core::Result<BspPakZipReader>::success(std::move(reader));
}

bool BspPakZipReader::hasEntry(const QString& entryPath) const {
    if (entryPath.isEmpty()) {
        return false;
    }
    const QString normalized = normalizeZipPath(entryPath);
    if (normalized.isEmpty() || hasParentTraversalSegment(normalized)) {
        return false;
    }
    return m_lowerPathToIndex.contains(normalized.toLower());
}

Core::Result<std::vector<std::byte>> BspPakZipReader::readEntry(const QString& entryPath) const {
    if (entryPath.isEmpty()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("BspPakZipReader", "Entry path is empty"));
    }

    const QString normalized = normalizeZipPath(entryPath);
    if (hasParentTraversalSegment(normalized)) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspPakZipReader", "Entry path traverses outside archive"),
            entryPath);
    }

    const auto it = m_lowerPathToIndex.constFind(normalized.toLower());
    if (it == m_lowerPathToIndex.constEnd()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::EntryNotFound,
            QCoreApplication::translate("BspPakZipReader", "Entry not found in BSP pack"),
            entryPath);
    }

    return readEntry(*it);
}

Core::Result<std::vector<std::byte>> BspPakZipReader::readEntry(std::size_t index) const {
    if (index >= m_entries.size()) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("BspPakZipReader", "BSP pack entry index out of range"));
    }

    const BspZipEntry& entry = m_entries[index];
    const std::span<const std::byte> pakSpan(m_pakBytes);

    if (entry.localHeaderOffset > pakSpan.size() || pakSpan.size() - entry.localHeaderOffset < 30) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "ZIP local file header out of bounds"),
            entry.rawPath);
    }

    const std::size_t localOffset = static_cast<std::size_t>(entry.localHeaderOffset);
    if (readU32LE(pakSpan, localOffset) != kZipLocalHeaderSignature) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "Invalid ZIP local file header signature"),
            entry.rawPath);
    }

    const std::uint16_t nameLen = readU16LE(pakSpan, localOffset + 26);
    const std::uint16_t extraLen = readU16LE(pakSpan, localOffset + 28);
    const std::uint64_t dataOffset = entry.localHeaderOffset + 30ULL + nameLen + extraLen;

    if (dataOffset > pakSpan.size() || entry.compressedSize > pakSpan.size() - dataOffset) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "ZIP entry payload exceeds BSP PAKFILE bounds"),
            entry.rawPath);
    }

    if (entry.uncompressedSize > kMaxUncompressedEntryBytes) {
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("BspPakZipReader", "ZIP entry uncompressed size exceeds safety limit"),
            entry.rawPath);
    }

    const std::span<const std::byte> compressed = pakSpan.subspan(
        static_cast<std::size_t>(dataOffset),
        static_cast<std::size_t>(entry.compressedSize));

    switch (entry.compressionMethod) {
    case kZipMethodStore: {
        if (entry.compressedSize != entry.uncompressedSize) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("BspPakZipReader", "Stored ZIP entry size mismatch"),
                entry.rawPath);
        }
        return Core::Result<std::vector<std::byte>>::success(
            std::vector<std::byte>(compressed.begin(), compressed.end()));
    }
    case kZipMethodDeflate:
        return inflateDeflate(compressed, entry.uncompressedSize, entry.rawPath);
    case kZipMethodLzma: {
        if (compressed.size() < 9) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("BspPakZipReader", "Truncated ZIP LZMA entry header"),
                entry.rawPath);
        }
        const std::uint16_t propSize = readU16LE(compressed, 2);
        if (propSize != 5) {
            return Core::Result<std::vector<std::byte>>::failure(
                Core::Error::ErrorCode::NotSupported,
                QCoreApplication::translate("BspPakZipReader", "Unsupported ZIP LZMA property size"),
                entry.rawPath);
        }
        const std::span<const std::byte> props5 = compressed.subspan(4, 5);
        const std::span<const std::byte> stream = compressed.subspan(9);
        return Lzma1Decoder::decompressRaw(
            props5,
            stream,
            static_cast<std::size_t>(entry.uncompressedSize));
    }
    default:
        return Core::Result<std::vector<std::byte>>::failure(
            Core::Error::ErrorCode::NotSupported,
            QCoreApplication::translate("BspPakZipReader", "Unsupported ZIP compression method: %1")
                .arg(entry.compressionMethod),
            entry.rawPath);
    }
}

} // namespace Domain::Package::Internal
