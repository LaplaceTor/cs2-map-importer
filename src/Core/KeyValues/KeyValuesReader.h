#pragma once

#include "Core/KeyValues/KeyValuesLexer.h"
#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/Result/Result.h"
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <vector>

namespace Core::KeyValues {

/**
 * @brief Streaming pull-parser event types emitted by KeyValuesReader.
 */
enum class KeyValuesEventType : int {
    None,
    BeginSection,
    EndSection,
    Property,
    EndOfDocument,
};

/**
 * @brief Dialect options controlling KeyValuesReader parsing behavior.
 */
struct KeyValuesReadOptions {
    bool unescapeStrings = true;
    bool supportConditionals = true;

    static KeyValuesReadOptions vdfDefault() noexcept { return {}; }
    static KeyValuesReadOptions vmf() noexcept { return {.unescapeStrings = false, .supportConditionals = false}; }
};

/**
 * @brief Streaming pull-parser for Valve KeyValues (KV1 / VDF / VMF / VMT) documents.
 *
 * Supports event-by-event traversal, zero-allocation section skipping,
 * scoped KeyValuesNode subtree materialization, and full-document loading.
 */
class KeyValuesReader {
public:
    explicit KeyValuesReader(QIODevice* device, KeyValuesReadOptions options = {});
    explicit KeyValuesReader(QByteArray utf8Source, KeyValuesReadOptions options = {});
    explicit KeyValuesReader(const QString& source, KeyValuesReadOptions options = {});

    Core::Result<KeyValuesEventType> readNext();
    Core::Result<KeyValuesNode> readSubtree();
    Core::Result<void> skipCurrentSection();
    Core::Result<void> readDocument(KeyValuesNode& rootNode);

    KeyValuesEventType currentEvent() const noexcept { return m_currentEvent; }
    const QString& name() const noexcept { return m_name; }
    const QString& value() const noexcept { return m_value; }
    int depth() const noexcept { return static_cast<int>(m_sectionStack.size()); }
    int line() const noexcept { return m_line; }
    int column() const noexcept { return m_column; }

private:
    static bool isConditional(const QString& token) noexcept;
    Core::Result<KeyValuesEventType> readErrorFailure() const;

    KeyValuesLexer m_lexer;
    KeyValuesReadOptions m_options;
    KeyValuesEventType m_currentEvent = KeyValuesEventType::None;
    QString m_name;
    QString m_value;
    std::vector<QString> m_sectionStack;
    int m_line = 1;
    int m_column = 1;
};

} // namespace Core::KeyValues
