#include "Core/KeyValues/KeyValuesNode.h"

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

bool KeyValuesNode::removeChild(int index) {
    if (index >= 0 && index < static_cast<int>(m_children.size())) {
        m_children.erase(m_children.begin() + index);
        return true;
    }
    return false;
}

int KeyValuesNode::removeChildren(const QString& name, Qt::CaseSensitivity cs) {
    int removed = 0;
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

int KeyValuesNode::removeProperties(const QString& key, Qt::CaseSensitivity cs) {
    int removed = 0;
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

