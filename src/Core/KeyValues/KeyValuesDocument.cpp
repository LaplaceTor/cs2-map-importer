#include <QCoreApplication>
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/KeyValues/KeyValuesWriter.h"
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
            QCoreApplication::translate("KeyValuesDocument", "Failed to load KeyValues document from %1: %2")
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
            QCoreApplication::translate("KeyValuesDocument", "Failed to parse KeyValues string: %1").arg(res.message()),
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

Core::Result<void> KeyValuesDocument::saveToFile(
    const Path::FilesystemPath& path,
    const Path::FilesystemPath& expectedBaseDir) const {
    const QString text = saveToString();
    const QByteArray data = text.toUtf8();
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

QString KeyValuesDocument::saveToString() const {
    return KeyValuesWriter::toString(m_root);
}

QByteArray KeyValuesDocument::saveToData() const {
    return saveToString().toUtf8();
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
