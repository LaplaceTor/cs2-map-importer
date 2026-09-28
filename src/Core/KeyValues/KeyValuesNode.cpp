#include "Core/KeyValues/KeyValuesNode.h"
#include <utility>

namespace Core::KeyValues {

KeyValuesNode::KeyValuesNode()
    : m_isSection(false) {
}

KeyValuesNode::KeyValuesNode(QString name)
    : m_name(std::move(name)), m_isSection(true) {
}

KeyValuesNode::KeyValuesNode(QString name, QString value)
    : m_name(std::move(name)), m_value(std::move(value)), m_isSection(false) {
}

KeyValuesNode KeyValuesNode::makeSection(QString name) {
    return KeyValuesNode(std::move(name));
}

KeyValuesNode KeyValuesNode::makeProperty(QString name, QString value) {
    return KeyValuesNode(std::move(name), std::move(value));
}

bool KeyValuesNode::isEmpty() const noexcept {
    if (m_isSection) {
        return m_children.empty();
    }
    return m_name.isEmpty() && m_value.isEmpty();
}

int KeyValuesNode::toInt(int defaultValue, bool* ok) const {
    bool convertOk = false;
    const int result = m_value.toInt(&convertOk);
    if (ok) {
        *ok = convertOk;
    }
    return convertOk ? result : defaultValue;
}

qint64 KeyValuesNode::toInt64(qint64 defaultValue, bool* ok) const {
    bool convertOk = false;
    const qint64 result = m_value.toLongLong(&convertOk);
    if (ok) {
        *ok = convertOk;
    }
    return convertOk ? result : defaultValue;
}

double KeyValuesNode::toDouble(double defaultValue, bool* ok) const {
    bool convertOk = false;
    const double result = m_value.toDouble(&convertOk);
    if (ok) {
        *ok = convertOk;
    }
    return convertOk ? result : defaultValue;
}

bool KeyValuesNode::toBool(bool defaultValue) const {
    const QString trimmed = m_value.trimmed();
    if (trimmed == QStringLiteral("1") || trimmed.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (trimmed == QStringLiteral("0") || trimmed.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QStringLiteral("no"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return defaultValue;
}

QStringList KeyValuesNode::toStringList(const QChar& separator) const {
    return m_value.split(separator, Qt::SkipEmptyParts);
}

const KeyValuesNode* KeyValuesNode::findChild(const QString& name, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (child.name().compare(name, cs) == 0) {
            return &child;
        }
    }
    return nullptr;
}

KeyValuesNode* KeyValuesNode::findChild(const QString& name, Qt::CaseSensitivity cs) {
    for (auto& child : m_children) {
        if (child.name().compare(name, cs) == 0) {
            return &child;
        }
    }
    return nullptr;
}

std::vector<const KeyValuesNode*> KeyValuesNode::findChildren(const QString& name, Qt::CaseSensitivity cs) const {
    std::vector<const KeyValuesNode*> result;
    for (const auto& child : m_children) {
        if (child.name().compare(name, cs) == 0) {
            result.push_back(&child);
        }
    }
    return result;
}

std::vector<KeyValuesNode*> KeyValuesNode::findChildren(const QString& name, Qt::CaseSensitivity cs) {
    std::vector<KeyValuesNode*> result;
    for (auto& child : m_children) {
        if (child.name().compare(name, cs) == 0) {
            result.push_back(&child);
        }
    }
    return result;
}

bool KeyValuesNode::hasChild(const QString& name, Qt::CaseSensitivity cs) const {
    return findChild(name, cs) != nullptr;
}

bool KeyValuesNode::hasProperty(const QString& key, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            return true;
        }
    }
    return false;
}

QString KeyValuesNode::property(const QString& key, const QString& defaultValue, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            return child.value();
        }
    }
    return defaultValue;
}

int KeyValuesNode::propertyInt(const QString& key, int defaultValue, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            return child.toInt(defaultValue);
        }
    }
    return defaultValue;
}

double KeyValuesNode::propertyDouble(const QString& key, double defaultValue, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            return child.toDouble(defaultValue);
        }
    }
    return defaultValue;
}

bool KeyValuesNode::propertyBool(const QString& key, bool defaultValue, Qt::CaseSensitivity cs) const {
    for (const auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            return child.toBool(defaultValue);
        }
    }
    return defaultValue;
}

KeyValuesNode& KeyValuesNode::addChild(KeyValuesNode child) {
    m_isSection = true;
    m_children.push_back(std::move(child));
    return m_children.back();
}

KeyValuesNode& KeyValuesNode::addProperty(const QString& key, const QString& value) {
    m_isSection = true;
    m_children.emplace_back(key, value);
    return m_children.back();
}

KeyValuesNode& KeyValuesNode::addSection(const QString& name) {
    m_isSection = true;
    m_children.emplace_back(name);
    return m_children.back();
}

void KeyValuesNode::setProperty(const QString& key, const QString& value, Qt::CaseSensitivity cs) {
    m_isSection = true;
    for (auto& child : m_children) {
        if (!child.isSection() && child.name().compare(key, cs) == 0) {
            child.setValue(value);
            return;
        }
    }
    addProperty(key, value);
}

std::optional<qsizetype> KeyValuesNode::indexOfChild(const QString& name, Qt::CaseSensitivity cs) const {
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i].name().compare(name, cs) == 0) {
            return static_cast<qsizetype>(i);
        }
    }
    return std::nullopt;
}

std::optional<qsizetype> KeyValuesNode::indexOfProperty(const QString& key, Qt::CaseSensitivity cs) const {
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (!m_children[i].isSection() && m_children[i].name().compare(key, cs) == 0) {
            return static_cast<qsizetype>(i);
        }
    }
    return std::nullopt;
}

bool KeyValuesNode::insertChild(qsizetype index, KeyValuesNode child) {
    if (index < 0 || index > static_cast<qsizetype>(m_children.size())) {
        return false;
    }
    m_isSection = true;
    m_children.insert(m_children.begin() + index, std::move(child));
    return true;
}

bool KeyValuesNode::insertProperty(qsizetype index, const QString& key, const QString& value) {
    return insertChild(index, KeyValuesNode(key, value));
}

bool KeyValuesNode::insertSection(qsizetype index, const QString& name) {
    return insertChild(index, KeyValuesNode(name));
}

bool KeyValuesNode::insertPropertyAfter(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs) {
    auto idx = indexOfProperty(targetKey, cs);
    if (!idx) {
        return false;
    }
    return insertProperty(*idx + 1, key, value);
}

bool KeyValuesNode::insertPropertyBefore(const QString& targetKey, const QString& key, const QString& value, Qt::CaseSensitivity cs) {
    auto idx = indexOfProperty(targetKey, cs);
    if (!idx) {
        return false;
    }
    return insertProperty(*idx, key, value);
}

bool KeyValuesNode::insertChildAfter(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs) {
    auto idx = indexOfChild(targetName, cs);
    if (!idx) {
        return false;
    }
    return insertChild(*idx + 1, std::move(child));
}

bool KeyValuesNode::insertChildBefore(const QString& targetName, KeyValuesNode child, Qt::CaseSensitivity cs) {
    auto idx = indexOfChild(targetName, cs);
    if (!idx) {
        return false;
    }
    return insertChild(*idx, std::move(child));
}

bool KeyValuesNode::setPropertyAt(qsizetype index, const QString& key, const QString& value) {
    if (index < 0 || index >= static_cast<qsizetype>(m_children.size())) {
        return false;
    }
    m_children[static_cast<size_t>(index)] = KeyValuesNode(key, value);
    return true;
}

bool KeyValuesNode::setChildAt(qsizetype index, KeyValuesNode child) {
    if (index < 0 || index >= static_cast<qsizetype>(m_children.size())) {
        return false;
    }
    m_children[static_cast<size_t>(index)] = std::move(child);
    return true;
}

bool KeyValuesNode::removeChild(qsizetype index) {
    if (index >= 0 && index < static_cast<qsizetype>(m_children.size())) {
        m_children.erase(m_children.begin() + index);
        return true;
    }
    return false;
}

qsizetype KeyValuesNode::removeChildren(const QString& name, Qt::CaseSensitivity cs) {
    qsizetype removed = 0;
    for (auto it = m_children.begin(); it != m_children.end();) {
        if (it->name().compare(name, cs) == 0) {
            it = m_children.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

qsizetype KeyValuesNode::removeProperties(const QString& key, Qt::CaseSensitivity cs) {
    qsizetype removed = 0;
    for (auto it = m_children.begin(); it != m_children.end();) {
        if (!it->isSection() && it->name().compare(key, cs) == 0) {
            it = m_children.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

void KeyValuesNode::clear() {
    m_children.clear();
    m_value.clear();
}

} // namespace Core::KeyValues

