#include "Domain/Package/VpkIndex.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Domain::Package {

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

bool VpkIndex::matchesDiskMetadata() const {
    if (m_vpks.empty()) {
        return false;
    }

    for (const auto& meta : m_vpks) {
        QFileInfo fi(meta.path.toString());
        if (!fi.exists() || !fi.isFile()) {
            return false;
        }
        if (fi.size() != meta.size) {
            return false;
        }
        if (fi.lastModified().toMSecsSinceEpoch() != meta.lastModified) {
            return false;
        }
    }

    return true;
}

Core::Result<void> VpkIndex::saveToFile(const Core::Path::FilesystemPath& filePath) const {
    if (filePath.isEmpty() || !filePath.isValid()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkIndex", "Index file path is empty or invalid"));
    }

    // Ensure parent directory exists
    const Core::Path::FilesystemPath parentDir = filePath.parentPath();
    if (parentDir.isValid()) {
        QDir().mkpath(parentDir.toString());
    }

    QFile file(filePath.toString());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::WriteFailed,
            QCoreApplication::translate("VpkIndex", "Failed to open index file for writing: %1").arg(file.errorString()),
            filePath.toString());
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_6_8);

    // 1. Header
    out << MagicHeader;
    out << CurrentVersion;
    const quint32 flags = m_isCs2Index ? 1 : 0;
    out << flags;
    out << m_generatedTimestamp;

    // 2. VPK table
    const quint16 vpkCount = static_cast<quint16>(m_vpks.size());
    out << vpkCount;
    for (const auto& meta : m_vpks) {
        out << meta.id;
        out << meta.path.toString();
        out << meta.size;
        out << meta.lastModified;
        out << meta.sha256;
    }

    // 3. Entries
    const quint32 entryCount = static_cast<quint32>(m_entryToVpk.size());
    out << entryCount;
    for (auto it = m_entryToVpk.constBegin(); it != m_entryToVpk.constEnd(); ++it) {
        out << it.key();
        out << it.value();
    }

    // 4. CS2 Native Stems
    const quint32 stemCount = static_cast<quint32>(m_cs2NativeStems.size());
    out << stemCount;
    for (const auto& stem : m_cs2NativeStems) {
        out << stem;
    }

    if (out.status() != QDataStream::Ok) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::WriteFailed,
            QCoreApplication::translate("VpkIndex", "Stream write failed while writing index data"),
            filePath.toString());
    }

    return Core::Result<void>::success();
}

Core::Result<VpkIndex> VpkIndex::loadFromFile(const Core::Path::FilesystemPath& filePath) {
    if (filePath.isEmpty() || !filePath.isValid()) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VpkIndex", "Index file path is empty or invalid"));
    }

    QFile file(filePath.toString());
    if (!file.exists()) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("VpkIndex", "Index file does not exist"),
            filePath.toString());
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("VpkIndex", "Failed to open index file for reading: %1").arg(file.errorString()),
            filePath.toString());
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_8);

    // 1. Header
    quint64 magic = 0;
    in >> magic;
    if (magic != MagicHeader) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VpkIndex", "Index file has invalid magic header"),
            filePath.toString());
    }

    quint32 version = 0;
    in >> version;
    if (version != CurrentVersion) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VpkIndex", "Index file version mismatch (expected %1, got %2)").arg(CurrentVersion).arg(version),
            filePath.toString());
    }

    quint32 flags = 0;
    in >> flags;
    qint64 timestamp = 0;
    in >> timestamp;

    VpkIndex index;
    index.m_isCs2Index = (flags & 1) != 0;
    index.m_generatedTimestamp = timestamp;

    // 2. VPK table
    quint16 vpkCount = 0;
    in >> vpkCount;
    index.m_vpks.reserve(vpkCount);
    for (quint16 i = 0; i < vpkCount; ++i) {
        VpkArchiveMeta meta;
        QString pathStr;
        in >> meta.id;
        in >> pathStr;
        meta.path = Core::Path::FilesystemPath(pathStr);
        in >> meta.size;
        in >> meta.lastModified;
        in >> meta.sha256;
        index.m_vpks.push_back(std::move(meta));
    }

    // 3. Entries
    quint32 entryCount = 0;
    in >> entryCount;
    index.m_entryToVpk.reserve(static_cast<qsizetype>(entryCount));
    for (quint32 i = 0; i < entryCount; ++i) {
        QString key;
        uint16_t vpkId = 0;
        in >> key;
        in >> vpkId;
        index.m_entryToVpk.insert(key, vpkId);
    }

    // 4. CS2 Native Stems
    quint32 stemCount = 0;
    in >> stemCount;
    index.m_cs2NativeStems.reserve(static_cast<qsizetype>(stemCount));
    for (quint32 i = 0; i < stemCount; ++i) {
        QString stem;
        in >> stem;
        index.m_cs2NativeStems.insert(stem);
    }

    if (in.status() != QDataStream::Ok) {
        return Core::Result<VpkIndex>::failure(
            Core::Error::ErrorCode::CorruptedData,
            QCoreApplication::translate("VpkIndex", "Failed to deserialize index: data stream is corrupted or truncated"),
            filePath.toString());
    }

    return Core::Result<VpkIndex>::success(std::move(index));
}

} // namespace Domain::Package
