#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesReader.h"
#include "Core/KeyValues/KeyValuesWriter.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <QString>
#include <QByteArray>
#include <utility>

namespace Core::KeyValues {

/**
 * @brief High-level document container for Valve KeyValues (KV1) documents.
 *
 * Represents an entire KeyValues document owning a root KeyValuesNode.
 * Provides loading/saving operations and access to the root AST node.
 */
class KeyValuesDocument {
public:
    KeyValuesDocument();
    explicit KeyValuesDocument(KeyValuesNode rootNode);

    // Factory methods
    static KeyValuesDocument fromFile(const Path::FilesystemPath& path, KeyValuesReadOptions options = {});
    static KeyValuesDocument fromString(const QString& content, KeyValuesReadOptions options = {});
    static KeyValuesDocument fromData(const QByteArray& data, KeyValuesReadOptions options = {});

    // Load methods returning Result<void>
    Core::Result<void> loadFromFile(const Path::FilesystemPath& path, KeyValuesReadOptions options = {});
    Core::Result<void> loadFromString(const QString& content, KeyValuesReadOptions options = {});
    Core::Result<void> loadFromData(const QByteArray& data, KeyValuesReadOptions options = {});

    // Save methods returning Result<void>
    Core::Result<void> saveToFile(
        const Path::FilesystemPath& path,
        const Path::FilesystemPath& expectedBaseDir = {},
        const KeyValuesWriter::Options& options = {}) const;
    QString saveToString(const KeyValuesWriter::Options& options = {}) const;
    QByteArray saveToData(const KeyValuesWriter::Options& options = {}) const;

    // Root node access
    KeyValuesNode& root() noexcept { return m_root; }
    const KeyValuesNode& root() const noexcept { return m_root; }
    void setRoot(KeyValuesNode rootNode) noexcept { m_root = std::move(rootNode); }

    // Direct querying helpers on root
    const KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    bool hasChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;

    void clear();

private:
    KeyValuesNode m_root;
};

} // namespace Core::KeyValues
