#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
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
    static KeyValuesDocument fromFile(const Path::FilesystemPath& path);
    static KeyValuesDocument fromString(const QString& content);
    static KeyValuesDocument fromData(const QByteArray& data);

    // Load methods returning Result<void>
    Core::Result<void> loadFromFile(const Path::FilesystemPath& path);
    Core::Result<void> loadFromString(const QString& content);
    Core::Result<void> loadFromData(const QByteArray& data);

    // Save methods returning Result<void>
    Core::Result<void> saveToFile(const Path::FilesystemPath& path) const;
    QString saveToString() const;
    QByteArray saveToData() const;

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
