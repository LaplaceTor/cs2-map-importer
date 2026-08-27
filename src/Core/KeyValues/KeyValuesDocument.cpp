#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Core/Error/Exception.h"
#include "Core/Error/ErrorCode.h"
#include <utility>

namespace Core::KeyValues {

KeyValuesDocument::KeyValuesDocument()
    : m_root(KeyValuesNode::makeSection(QString())) {
}

KeyValuesDocument::KeyValuesDocument(KeyValuesNode rootNode)
    : m_root(std::move(rootNode)) {
}

KeyValuesDocument KeyValuesDocument::fromFile(const Path::FilesystemPath& path) {
    KeyValuesDocument doc;
    auto res = doc.loadFromFile(path);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QStringLiteral("Failed to load KeyValues document from %1: %2")
                .arg(path.toString(), res.message()),
            res.details());
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromString(const QString& content) {
    KeyValuesDocument doc;
    auto res = doc.loadFromString(content);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QStringLiteral("Failed to parse KeyValues string: %1").arg(res.message()),
            res.details());
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromData(const QByteArray& data) {
    return fromString(QString::fromUtf8(data));
}

Core::Result<void> KeyValuesDocument::loadFromFile(const Path::FilesystemPath& path) {
    if (!path.isValid() || path.isEmpty()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QStringLiteral("Path is empty or invalid"),
            path.toString());
    }
    if (!path.exists()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QStringLiteral("File does not exist"),
            path.toString());
    }

    try {
        const QByteArray data = FileSystem::FileSystem::readAll(path.toString());
        return loadFromData(data);
    } catch (const Error::Exception& e) {
        return Core::Result<void>::failure(e.error());
    } catch (const std::exception& e) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QString::fromUtf8(e.what()));
    }
}

Core::Result<void> KeyValuesDocument::loadFromString(const QString& content) {
    KeyValuesParser parser;
    return parser.parse(content, m_root);
}

Core::Result<void> KeyValuesDocument::loadFromData(const QByteArray& data) {
    return loadFromString(QString::fromUtf8(data));
}

Core::Result<void> KeyValuesDocument::saveToFile(const Path::FilesystemPath& path, const KeyValuesWriter::Options& options) const {
    const QString text = saveToString(options);
    const QByteArray data = text.toUtf8();
    try {
        FileSystem::AtomicFile::writeAtomic(path.toString(), data);
        return Core::Result<void>::success();
    } catch (const Error::Exception& e) {
        return Core::Result<void>::failure(e.error());
    } catch (const std::exception& e) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::WriteFailed,
            QString::fromUtf8(e.what()));
    }
}

QString KeyValuesDocument::saveToString(const KeyValuesWriter::Options& options) const {
    return KeyValuesWriter::toString(m_root, options);
}

QByteArray KeyValuesDocument::saveToData(const KeyValuesWriter::Options& options) const {
    return saveToString(options).toUtf8();
}

} // namespace Core::KeyValues
