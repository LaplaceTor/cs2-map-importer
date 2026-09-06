#pragma once

#include <cstring>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QString>

#include <bsppp/BSP.h>
#include <vpkpp/format/VPK.h>
#include <vpkpp/format/ZIP.h>

/**
 * @brief Programmatic pack-file fixtures for tests, built through the
 *        write-capable vpkpp/bsppp APIs.
 */
namespace TestPackFixtures {

using PackEntryList = std::vector<std::pair<QString, QByteArray>>;

inline std::vector<std::byte> toByteVector(const QByteArray& data) {
    std::vector<std::byte> buffer(static_cast<std::size_t>(data.size()));
    if (!data.isEmpty()) {
        std::memcpy(buffer.data(), data.constData(), static_cast<std::size_t>(data.size()));
    }
    return buffer;
}

inline bool createTestVpk(const QString& vpkPath, const PackEntryList& entries) {
    auto vpk = vpkpp::VPK::create(vpkPath.toStdString(), 2);
    if (!vpk) {
        return false;
    }
    for (const auto& [entryPath, data] : entries) {
        if (!vpk->addEntry(entryPath.toStdString(), toByteVector(data))) {
            return false;
        }
    }
    return vpk->bake("");
}

inline bool createTestBsp(const QString& bspPath, const PackEntryList& embeddedEntries) {
    QByteArray zipData;
    if (!embeddedEntries.empty()) {
        const QString zipPath = bspPath + QStringLiteral(".fixture.zip");
        auto zip = vpkpp::ZIP::create(zipPath.toStdString());
        if (!zip) {
            return false;
        }
        for (const auto& [entryPath, data] : embeddedEntries) {
            if (!zip->addEntry(entryPath.toStdString(), toByteVector(data))) {
                return false;
            }
        }
        if (!zip->bake("")) {
            return false;
        }

        QFile zipFile(zipPath);
        if (!zipFile.open(QIODevice::ReadOnly)) {
            return false;
        }
        zipData = zipFile.readAll();
        zipFile.close();
        QFile::remove(zipPath);
    }

    QFile bspFile(bspPath);
    if (!bspFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QDataStream out(&bspFile);
    out.setByteOrder(QDataStream::LittleEndian);

    // 1. Signature 'VBSP' (0x50534256)
    out << static_cast<quint32>(0x50534256);
    // 2. Version 21
    out << static_cast<quint32>(21);

    // 3. 64 lumps: each lump has {offset, length, version, uncompressedLength}
    const quint32 headerSize = 4 + 4 + (64 * 16) + 4; // 1036 bytes
    const auto pakLumpIndex = static_cast<int>(bsppp::BSPLump::PAKFILE); // 40

    for (int i = 0; i < 64; ++i) {
        if (i == pakLumpIndex && !zipData.isEmpty()) {
            out << static_cast<quint32>(headerSize);     // offset
            out << static_cast<quint32>(zipData.size()); // length
            out << static_cast<quint32>(0);              // version
            out << static_cast<quint32>(0);              // uncompressedLength
        } else {
            out << static_cast<quint32>(0);
            out << static_cast<quint32>(0);
            out << static_cast<quint32>(0);
            out << static_cast<quint32>(0);
        }
    }

    // 4. Map revision 0
    out << static_cast<quint32>(0);

    // 5. Embedded PAKFILE zip lump
    if (!zipData.isEmpty()) {
        if (bspFile.write(zipData) != zipData.size()) {
            return false;
        }
    }

    bspFile.close();
    return true;
}

} // namespace TestPackFixtures
