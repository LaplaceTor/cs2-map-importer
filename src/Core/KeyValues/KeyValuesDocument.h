#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesWriter.h"
#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include <QString>
#include <QByteArray>
#include <utility>

namespace Core::KeyValues {

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
    Core::Result<void> saveToFile(const Path::FilesystemPath& path, const KeyValuesWriter::Options& options = {}) const;
    QString saveToString(const KeyValuesWriter::Options& options = {}) const;
    QByteArray saveToData(const KeyValuesWriter::Options& options = {}) const;

    // Root node access
    KeyValuesNode& root() noexcept { return m_root; }
    const KeyValuesNode& root() const noexcept { return m_root; }

    // Direct querying / mutation helpers on root
    const KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const {
        return m_root.findChild(name, cs);
    }
    KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.findChild(name, cs);
    }

    std::vector<const KeyValuesNode*> findChildren(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const {
        return m_root.findChildren(name, cs);
    }
    std::vector<KeyValuesNode*> findChildren(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.findChildren(name, cs);
    }

    bool hasChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const {
        return m_root.hasChild(name, cs);
    }

    KeyValuesNode& addChild(KeyValuesNode child) {
        return m_root.addChild(std::move(child));
    }

    KeyValuesNode& addSection(const QString& name) {
        return m_root.addSection(name);
    }

    KeyValuesNode& addProperty(const QString& key, const QString& value) {
        return m_root.addProperty(key, value);
    }

    int indexOfChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const {
        return m_root.indexOfChild(name, cs);
    }

    int indexOfProperty(const QString& key, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const {
        return m_root.indexOfProperty(key, cs);
    }

    KeyValuesNode& insertChild(int index, KeyValuesNode child) {
        return m_root.insertChild(index, std::move(child));
    }

    KeyValuesNode& insertProperty(int index, const QString& key, const QString& value) {
        return m_root.insertProperty(index, key, value);
    }

    KeyValuesNode& insertSection(int index, const QString& name) {
        return m_root.insertSection(index, name);
    }

    bool insertPropertyAfter(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.insertPropertyAfter(targetKey, key, value, cs);
    }

    bool insertPropertyBefore(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.insertPropertyBefore(targetKey, key, value, cs);
    }

    bool insertChildAfter(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.insertChildAfter(targetName, std::move(child), cs);
    }

    bool insertChildBefore(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs = Qt::CaseInsensitive) {
        return m_root.insertChildBefore(targetName, std::move(child), cs);
    }

    void clear() {
        m_root.clear();
    }

private:
    KeyValuesNode m_root;
};

} // namespace Core::KeyValues
