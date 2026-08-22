#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Core/Error/ImportException.h"
#include "Core/Error/ImportError.h"

namespace Core::KeyValues {

KeyValuesDocument::KeyValuesDocument()
    : m_root(KeyValuesNode::makeSection(QString())) {
}

KeyValuesDocument::KeyValuesDocument(KeyValuesNode rootNode)
    : m_root(std::move(rootNode)) {
}

KeyValuesDocument KeyValuesDocument::fromFile(const Path::FilesystemPath& path) {
    KeyValuesDocument doc;
    QString errorMessage;
    if (!doc.loadFromFile(path, &errorMessage)) {
        throw Error::ImportException(
            Error::ImportErrorCode::InvalidFile,
            QStringLiteral("Failed to load KeyValues document from %1: %2")
                .arg(path.toString())
                .arg(errorMessage));
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromString(const QString& content) {
    KeyValuesDocument doc;
    QString errorMessage;
    if (!doc.loadFromString(content, &errorMessage)) {
        throw Error::ImportException(
            Error::ImportErrorCode::InvalidFile,
            QStringLiteral("Failed to parse KeyValues string: %1").arg(errorMessage));
    }
    return doc;
}

KeyValuesDocument KeyValuesDocument::fromData(const QByteArray& data) {
    return fromString(QString::fromUtf8(data));
}

bool KeyValuesDocument::loadFromFile(const Path::FilesystemPath& path, QString* errorMessage) {
    if (!path.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("File does not exist: %1").arg(path.toString());
        }
        return false;
    }

    try {
        const QByteArray data = FileSystem::FileSystem::readAll(path.toString());
        return loadFromData(data, errorMessage);
    } catch (const Error::ImportException& e) {
        if (errorMessage) {
            *errorMessage = e.message();
        }
        return false;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(e.what());
        }
        return false;
    }
}

bool KeyValuesDocument::loadFromString(const QString& content, QString* errorMessage) {
    KeyValuesParser parser;
    return parser.parse(content, m_root, errorMessage);
}

bool KeyValuesDocument::loadFromData(const QByteArray& data, QString* errorMessage) {
    return loadFromString(QString::fromUtf8(data), errorMessage);
}

bool KeyValuesDocument::saveToFile(const Path::FilesystemPath& path, const KeyValuesWriter::Options& options, QString* errorMessage) const {
    const QString text = saveToString(options);
    const QByteArray data = text.toUtf8();
    try {
        FileSystem::AtomicFile::writeAtomic(path.toString(), data);
        return true;
    } catch (const Error::ImportException& e) {
        if (errorMessage) {
            *errorMessage = e.message();
        }
        return false;
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(e.what());
        }
        return false;
    }
}

QString KeyValuesDocument::saveToString(const KeyValuesWriter::Options& options) const {
    return KeyValuesWriter::toString(m_root, options);
}

QByteArray KeyValuesDocument::saveToData(const KeyValuesWriter::Options& options) const {
    return saveToString(options).toUtf8();
}

} // namespace Core::KeyValues

