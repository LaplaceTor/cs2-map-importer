#include "Core/Hash/Sha256.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QFile>

namespace Core::Hash {

Core::Result<QString> Sha256::computeFileHash(const Path::FilesystemPath& filePath) {
    if (filePath.isEmpty() || !filePath.isValid()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("Sha256", "File path is empty or invalid"),
            filePath.toString());
    }

    QFile file(filePath.toString());
    if (!file.exists()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("Sha256", "File not found"),
            filePath.toString());
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QCoreApplication::translate("Sha256", "Failed to open file for reading: %1").arg(file.errorString()),
            filePath.toString());
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    constexpr qint64 ChunkSize = 64 * 1024; // 64 KB chunks
    QByteArray buffer;
    buffer.resize(ChunkSize);

    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(buffer.data(), ChunkSize);
        if (bytesRead < 0) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::ReadFailed,
                QCoreApplication::translate("Sha256", "Failed to read file: %1").arg(file.errorString()),
                filePath.toString());
        }
        if (bytesRead > 0) {
            hash.addData(buffer.constData(), bytesRead);
        }
    }

    return Core::Result<QString>::success(QString::fromLatin1(hash.result().toHex()));
}

QString Sha256::computeDataHash(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

} // namespace Core::Hash
