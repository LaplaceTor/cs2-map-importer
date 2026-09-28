#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include <optional>

namespace Core::KeyValues {

class KeyValuesNode {
public:
    KeyValuesNode();
    explicit KeyValuesNode(QString name);
    KeyValuesNode(QString name, QString value);

    static KeyValuesNode makeSection(QString name);
    static KeyValuesNode makeProperty(QString name, QString value);

    // State inspection
    bool isSection() const noexcept { return m_isSection; }
    bool isProperty() const noexcept { return !m_isSection; }
    bool isEmpty() const noexcept;

    // Name & Value
    const QString& name() const noexcept { return m_name; }
    void setName(const QString& name) { m_name = name; }

    const QString& value() const noexcept { return m_value; }
    void setValue(const QString& value) {
        m_value = value;
        m_isSection = false;
    }

    // Type conversion helpers
    int toInt(int defaultValue = 0, bool* ok = nullptr) const;
    qint64 toInt64(qint64 defaultValue = 0, bool* ok = nullptr) const;
    double toDouble(double defaultValue = 0.0, bool* ok = nullptr) const;
    bool toBool(bool defaultValue = false) const;
    QStringList toStringList(const QChar& separator = ' ') const;

    // Children & hierarchy
    const std::vector<KeyValuesNode>& children() const noexcept { return m_children; }
    std::vector<KeyValuesNode>& children() noexcept { return m_children; }
    qsizetype childCount() const noexcept { return static_cast<qsizetype>(m_children.size()); }
    bool hasChildren() const noexcept { return !m_children.empty(); }

    // Queries
    const KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    KeyValuesNode* findChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive);

    std::vector<const KeyValuesNode*> findChildren(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    std::vector<KeyValuesNode*> findChildren(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive);

    bool hasChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    bool hasProperty(const QString& key, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;

    QString property(const QString& key, const QString& defaultValue = QString(), Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    int propertyInt(const QString& key, int defaultValue = 0, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    double propertyDouble(const QString& key, double defaultValue = 0.0, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    bool propertyBool(const QString& key, bool defaultValue = false, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;

    // Positional queries
    std::optional<qsizetype> indexOfChild(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;
    std::optional<qsizetype> indexOfProperty(const QString& key, Qt::CaseSensitivity cs = Qt::CaseInsensitive) const;

    // Mutation
    KeyValuesNode& addChild(KeyValuesNode child);
    KeyValuesNode& addProperty(const QString& key, const QString& value);
    KeyValuesNode& addSection(const QString& name);
    void setProperty(const QString& key, const QString& value, Qt::CaseSensitivity cs = Qt::CaseInsensitive);

    // Positional insertion (fails and returns false if index < 0 or index > childCount())
    bool insertChild(qsizetype index, KeyValuesNode child);
    bool insertProperty(qsizetype index, const QString& key, const QString& value);
    bool insertSection(qsizetype index, const QString& name);
    bool insertPropertyAfter(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    bool insertPropertyBefore(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    bool insertChildAfter(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    bool insertChildBefore(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs = Qt::CaseInsensitive);

    // Positional mutation
    bool setPropertyAt(qsizetype index, const QString& key, const QString& value);
    bool setChildAt(qsizetype index, KeyValuesNode child);

    bool removeChild(qsizetype index);
    qsizetype removeChildren(const QString& name, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    qsizetype removeProperties(const QString& key, Qt::CaseSensitivity cs = Qt::CaseInsensitive);
    void clear();

private:
    QString m_name;
    QString m_value;
    std::vector<KeyValuesNode> m_children;
    bool m_isSection = false;
};

} // namespace Core::KeyValues

