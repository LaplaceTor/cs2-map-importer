#include <QCoreApplication>
#include "Core/KeyValues/KeyValuesDocument.h"
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

KeyValuesDocument KeyValuesDocument::fromFile(const Path::FilesystemPath& path, KeyValuesReadOptions options) {
    KeyValuesDocument doc;
    auto res = doc.loadFromFile(path, options);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QCoreApplication::translate("KeyValuesDocument", "Failed to load KeyValues document from %1: %2")
                .arg(path.toString(), res.message()),
            res.details());
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromString(const QString& content, KeyValuesReadOptions options) {
    KeyValuesDocument doc;
    auto res = doc.loadFromString(content, options);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QCoreApplication::translate("KeyValuesDocument", "Failed to parse KeyValues string: %1").arg(res.message()),
            res.details());
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromData(const QByteArray& data, KeyValuesReadOptions options) {
    KeyValuesDocument doc;
    auto res = doc.loadFromData(data, options);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QCoreApplication::translate("KeyValuesDocument", "Failed to parse KeyValues data: %1").arg(res.message()),
            res.details());
    }
    return doc;
}

Core::Result<void> KeyValuesDocument::loadFromFile(const Path::FilesystemPath& path, KeyValuesReadOptions options) {
    if (!path.isValid() || path.isEmpty()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("KeyValuesDocument", "Path is empty or invalid"),
            path.toString());
    }
    if (!path.exists()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::FileNotFound,
            QCoreApplication::translate("KeyValuesDocument", "File does not exist"),
            path.toString());
    }

    try {
        const QByteArray data = FileSystem::FileSystem::readAll(path.toString());
        return loadFromData(data, options);
    } catch (const Error::Exception& e) {
        return Core::Result<void>::failure(e.error());
    } catch (const std::exception& e) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::ReadFailed,
            QString::fromUtf8(e.what()));
    }
}

Core::Result<void> KeyValuesDocument::loadFromString(const QString& content, KeyValuesReadOptions options) {
    KeyValuesReader reader(content, options);
    return reader.readDocument(m_root);
}

Core::Result<void> KeyValuesDocument::loadFromData(const QByteArray& data, KeyValuesReadOptions options) {
    KeyValuesReader reader(data, options);
    return reader.readDocument(m_root);
}

Core::Result<void> KeyValuesDocument::saveToFile(
    const Path::FilesystemPath& path,
    const Path::FilesystemPath& expectedBaseDir,
    const KeyValuesWriter::Options& options) const {
    const QByteArray data = saveToData(options);
    try {
        FileSystem::AtomicFile::writeAtomic(path.toString(), data, expectedBaseDir);
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
    return KeyValuesWriter::toUtf8(m_root, options);
}

const KeyValuesNode* KeyValuesDocument::findChild(const QString& name, Qt::CaseSensitivity cs) const {
    return m_root.findChild(name, cs);
}

KeyValuesNode* KeyValuesDocument::findChild(const QString& name, Qt::CaseSensitivity cs) {
    return m_root.findChild(name, cs);
}

bool KeyValuesDocument::hasChild(const QString& name, Qt::CaseSensitivity cs) const {
    return m_root.hasChild(name, cs);
}

void KeyValuesDocument::clear() {
    m_root.clear();
}

} // namespace Core::KeyValues
