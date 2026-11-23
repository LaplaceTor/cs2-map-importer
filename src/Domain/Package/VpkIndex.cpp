#include "Domain/Package/VpkIndex.h"

#include <limits>
#include <utility>

#include <QByteArray>
#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>

#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"

namespace Domain::Package {

VpkArchiveIndex::VpkArchiveIndex(QString sha256, std::vector<QString> entries)
    : m_sha256(sha256.toLower())
    , m_entries(std::move(entries)) {
}

bool VpkArchiveIndex::isValidSha256Hex(QStringView hex) noexcept {
    if (hex.size() != 64) {
        return false;
    }

    for (const QChar ch : hex) {
        const char16_t code = ch.unicode();
        const bool isHex = (code >= u'0' && code <= u'9')
            || (code >= u'a' && code <= u'f')
            || (code >= u'A' && code <= u'F');
        if (!isHex) {
            return false;
        }
    }

    return true;
}

Core::Result<VpkArchiveIndex> VpkArchiveIndex::loadFromFile(
    const Core::Path::FilesystemPath& filePath,
    const QString& expectedSha256) {
    if (filePath.isEmpty() || !filePath.isValid()) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file path is empty or invalid"));
    }

    QFile file(filePath.toString());
    if (!file.exists()) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file does not exist"),
            filePath.toString());
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VpkArchiveIndex", "Failed to open archive index file for reading: %1")
                .arg(file.errorString()),
            filePath.toString());
    }

    const qint64 fileSize = file.size();
    if (fileSize < MinHeaderSize) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file is smaller than minimum header size"),
            filePath.toString());
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_8);

    quint64 magic = 0;
    in >> magic;
    if (in.status() != QDataStream::Ok) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index magic header"),
            filePath.toString());
    }
    if (magic != MagicHeader) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file has invalid magic header"),
            filePath.toString());
    }

    quint32 version = 0;
    in >> version;
    if (in.status() != QDataStream::Ok) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index version"),
            filePath.toString());
    }
    if (version != CurrentVersion) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file version mismatch (expected %1, got %2)")
                .arg(CurrentVersion)
                .arg(version),
            filePath.toString());
    }

    char sha256Raw[64]{};
    if (in.readRawData(sha256Raw, 64) != 64) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index SHA-256 header"),
            filePath.toString());
    }

    const QString headerSha256 = QString::fromLatin1(sha256Raw, 64).toLower();
    if (!isValidSha256Hex(headerSha256)) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index header contains an invalid SHA-256 digest"),
            filePath.toString());
    }

    if (!expectedSha256.isEmpty() && headerSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index SHA-256 header does not match expected digest"),
            filePath.toString());
    }

    quint32 entryCount = 0;
    in >> entryCount;
    if (in.status() != QDataStream::Ok) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index entry count"),
            filePath.toString());
    }

    qint64 remainingBytes = fileSize - MinHeaderSize;
    if (static_cast<qint64>(entryCount) > remainingBytes / 4) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index entry count exceeds maximum possible records for file size"),
            filePath.toString());
    }

    std::vector<QString> entries;
    entries.reserve(entryCount);

    for (quint32 i = 0; i < entryCount; ++i) {
        if (remainingBytes < 4) {
            return Core::Result<VpkArchiveIndex>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VpkArchiveIndex", "Archive index data stream is truncated"),
                filePath.toString());
        }

        quint32 utf8ByteLen = 0;
        in >> utf8ByteLen;
        if (in.status() != QDataStream::Ok) {
            return Core::Result<VpkArchiveIndex>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index entry length"),
                filePath.toString());
        }
        remainingBytes -= 4;

        if (utf8ByteLen > MaxEntryByteLength || static_cast<qint64>(utf8ByteLen) > remainingBytes) {
            return Core::Result<VpkArchiveIndex>::failure(
                Core::Error::ErrorCode::CorruptedData,
                QCoreApplication::translate("VpkArchiveIndex", "Archive index entry length is invalid or exceeds remaining file size"),
                filePath.toString());
        }

        QString entry;
        if (utf8ByteLen > 0) {
            QByteArray utf8(static_cast<qsizetype>(utf8ByteLen), Qt::Uninitialized);
            if (in.readRawData(utf8.data(), static_cast<int>(utf8ByteLen)) != static_cast<int>(utf8ByteLen)) {
                return Core::Result<VpkArchiveIndex>::failure(
                    Core::Error::ErrorCode::CorruptedData,
                    QCoreApplication::translate("VpkArchiveIndex", "Failed to read archive index entry payload"),
                    filePath.toString());
            }
            remainingBytes -= static_cast<qint64>(utf8ByteLen);
            entry = QString::fromUtf8(utf8);
        }
        entries.push_back(std::move(entry));
    }

    if (in.status() != QDataStream::Ok || !file.atEnd()) {
        return Core::Result<VpkArchiveIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index data stream is corrupted or contains trailing data"),
            filePath.toString());
    }

    return Core::Result<VpkArchiveIndex>::success(
        VpkArchiveIndex(headerSha256, std::move(entries)));
}

Core::Result<void> VpkArchiveIndex::saveToFile(
    const Core::Path::FilesystemPath& filePath,
    const Core::Path::FilesystemPath& expectedBaseDir) const {
    if (filePath.isEmpty() || !filePath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index file path is empty or invalid"));
    }

    if (!isValidSha256Hex(m_sha256)) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidState,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index does not contain a valid SHA-256 digest"),
            filePath.toString());
    }

    if (m_entries.size() > std::numeric_limits<quint32>::max()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidState,
            QCoreApplication::translate("VpkArchiveIndex", "Archive index entry count exceeds maximum supported format limit"),
            filePath.toString());
    }

    const Core::Error::ExecutionContext execCtx{
        QStringLiteral("Saving VPK archive index"),
        m_sha256,
        filePath.toString()
    };

    return Core::Error::ExecutionGuard::guard<void>(
        [&]() -> Core::Result<void> {
            Core::FileSystem::AtomicFile atomic(filePath.toString(), expectedBaseDir);
            atomic.open();

            QIODevice* device = atomic.device();
            if (!device) {
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("VpkArchiveIndex", "Failed to access atomic output device for archive index"),
                    filePath.toString());
            }

            QDataStream out(device);
            out.setByteOrder(QDataStream::LittleEndian);
            out.setVersion(QDataStream::Qt_6_8);

            out << MagicHeader;
            out << CurrentVersion;

            const QByteArray sha256Bytes = m_sha256.toLower().toLatin1();
            if (sha256Bytes.size() != 64 || out.writeRawData(sha256Bytes.constData(), 64) != 64) {
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("VpkArchiveIndex", "Failed to write SHA-256 header to archive index"),
                    filePath.toString());
            }

            const quint32 entryCount = static_cast<quint32>(m_entries.size());
            out << entryCount;

            for (const QString& entry : m_entries) {
                const QByteArray utf8 = entry.toUtf8();
                if (static_cast<quint64>(utf8.size()) > std::numeric_limits<quint32>::max()) {
                    return Core::Result<void>::failure(
                        Core::Error::ErrorCode::InvalidState,
                        QCoreApplication::translate("VpkArchiveIndex", "Archive index entry exceeds maximum supported UTF-8 byte length"),
                        filePath.toString());
                }

                const quint32 byteLen = static_cast<quint32>(utf8.size());
                out << byteLen;
                if (byteLen > 0
                    && out.writeRawData(utf8.constData(), static_cast<int>(byteLen)) != static_cast<int>(byteLen)) {
                    return Core::Result<void>::failure(
                        Core::Error::ErrorCode::WriteFailed,
                        QCoreApplication::translate("VpkArchiveIndex", "Failed to write entry data to archive index"),
                        filePath.toString());
                }
            }

            if (out.status() != QDataStream::Ok) {
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("VpkArchiveIndex", "Stream write failed while writing archive index"),
                    filePath.toString());
            }

            atomic.commit();
            return Core::Result<void>::success();
        },
        execCtx);
}

QString VpkIndex::normalizeArchivePathKey(const Core::Path::FilesystemPath& path) {
    if (path.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QDir::fromNativeSeparators(path.absolutePath().toString())).toLower();
}

VpkIndex VpkIndex::compose(std::vector<VpkArchiveSource> sources, bool isCs2) {
    VpkIndex index;
    index.m_isCs2Index = isCs2;
    index.m_generatedTimestamp = QDateTime::currentMSecsSinceEpoch();
    index.m_vpks.reserve(sources.size());

    std::size_t estimatedEntries = 0;
    for (const auto& source : sources) {
        if (source.archiveIndex) {
            estimatedEntries += source.archiveIndex->entryCount();
        }
    }
    index.m_entryToVpk.reserve(static_cast<qsizetype>(estimatedEntries));
    if (isCs2) {
        index.m_cs2NativeStems.reserve(static_cast<qsizetype>(estimatedEntries));
    }

    for (auto& source : sources) {
        if (!source.archiveIndex) {
            continue;
        }
        if (index.m_vpks.size() > std::numeric_limits<uint16_t>::max()) {
            break;
        }

        const uint16_t assignedId = static_cast<uint16_t>(index.m_vpks.size());
        source.meta.id = assignedId;
        if (source.meta.sha256.isEmpty()) {
            source.meta.sha256 = source.archiveIndex->sha256();
        } else {
            source.meta.sha256 = source.meta.sha256.toLower();
        }

        const QString archiveKey = normalizeArchivePathKey(source.meta.path);
        if (!archiveKey.isEmpty()) {
            index.m_indexedVpkPaths.insert(archiveKey);
        }

        for (const QString& rawEntry : source.archiveIndex->entries()) {
            const QString normalized = normalizePath(rawEntry);
            if (normalized.isEmpty()) {
                continue;
            }

            // First occurrence wins (sources is ordered from highest to lowest priority)
            if (!index.m_entryToVpk.contains(normalized)) {
                index.m_entryToVpk.insert(normalized, assignedId);
            }

            if (isCs2) {
                const QString stem = extractAssetStem(normalized);
                if (!stem.isEmpty()) {
                    index.m_cs2NativeStems.insert(stem);
                }
            }
        }

        index.m_vpks.push_back(std::move(source.meta));
    }

    return index;
}

QString VpkIndex::normalizePath(const QString& rawPath) {
    QString normalized = rawPath;
    normalized.replace(u'\\', u'/');
    while (normalized.startsWith(u'/')) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(u'/')) {
        normalized.chop(1);
    }
    return normalized.toLower();
}

QString VpkIndex::extractAssetStem(const QString& rawPath) {
    QString stem = normalizePath(rawPath);

    // Unify sound/ and sounds/ prefix for cross-engine audio mapping
    if (stem.startsWith(QStringLiteral("sounds/"))) {
        stem = QStringLiteral("sound/") + stem.mid(7);
    }

    // Strip CS2 compiled asset suffix "_c"
    if (stem.endsWith(QStringLiteral("_c"), Qt::CaseInsensitive)) {
        stem.chop(2);
    }

    // Strip file extension
    const int lastSlash = stem.lastIndexOf(u'/');
    const int lastDot = stem.lastIndexOf(u'.');
    if (lastDot > lastSlash && lastDot != -1) {
        stem.truncate(lastDot);
    }

    return stem;
}

std::optional<Core::Path::FilesystemPath> VpkIndex::findVpkForEntry(const QString& entryPath) const {
    const QString normalized = normalizePath(entryPath);
    const auto it = m_entryToVpk.constFind(normalized);
    if (it == m_entryToVpk.constEnd()) {
        return std::nullopt;
    }

    const uint16_t targetId = it.value();
    if (targetId < m_vpks.size() && m_vpks[targetId].id == targetId) {
        return m_vpks[targetId].path;
    }

    for (const auto& meta : m_vpks) {
        if (meta.id == targetId) {
            return meta.path;
        }
    }

    return std::nullopt;
}

bool VpkIndex::hasEntry(const QString& entryPath) const {
    return m_entryToVpk.contains(normalizePath(entryPath));
}

bool VpkIndex::coversVpk(const Core::Path::FilesystemPath& vpkPath) const {
    const QString key = normalizeArchivePathKey(vpkPath);
    if (key.isEmpty()) {
        return false;
    }
    return m_indexedVpkPaths.contains(key);
}

bool VpkIndex::hasCs2NativeAsset(const QString& relativeAssetPath) const {
    if (!m_isCs2Index) {
        return false;
    }

    const QString stem = extractAssetStem(relativeAssetPath);
    if (!stem.isEmpty() && m_cs2NativeStems.contains(stem)) {
        return true;
    }

    return hasEntry(relativeAssetPath);
}

void VpkIndex::setVpks(std::vector<VpkArchiveMeta> vpks) {
    m_vpks = std::move(vpks);
    m_indexedVpkPaths.clear();
    m_indexedVpkPaths.reserve(static_cast<qsizetype>(m_vpks.size()));
    for (const auto& meta : m_vpks) {
        const QString key = normalizeArchivePathKey(meta.path);
        if (!key.isEmpty()) {
            m_indexedVpkPaths.insert(key);
        }
    }
}

} // namespace Domain::Package
