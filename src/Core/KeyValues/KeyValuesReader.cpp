#include <QCoreApplication>
#include "Core/KeyValues/KeyValuesReader.h"
#include "Core/Error/ErrorCode.h"
#include <cmath>
#include <utility>

namespace Core::KeyValues {

namespace {

KeyValuesLexerOptions toLexerOptions(KeyValuesReadOptions options) noexcept {
    return KeyValuesLexerOptions{
        .unescapeStrings = options.unescapeStrings,
        .stopUnquotedAtBraces = options.stopUnquotedAtBraces,
    };
}

bool isNumericToken(const QString& text) noexcept {
    if (text.isEmpty()) {
        return false;
    }
    const QChar first = text.at(0);
    if (!first.isDigit() && first != QLatin1Char('.') && first != QLatin1Char('+') && first != QLatin1Char('-')) {
        return false;
    }
    bool ok = false;
    const double val = text.toDouble(&ok);
    return ok && std::isfinite(val);
}

} // namespace

KeyValuesReader::KeyValuesReader(QIODevice* device, KeyValuesReadOptions options)
    : m_lexer(device, toLexerOptions(options)), m_options(options) {
}

KeyValuesReader::KeyValuesReader(QByteArray utf8Source, KeyValuesReadOptions options)
    : m_lexer(std::move(utf8Source), toLexerOptions(options)), m_options(options) {
}

KeyValuesReader::KeyValuesReader(const QString& source, KeyValuesReadOptions options)
    : m_lexer(source, toLexerOptions(options)), m_options(options) {
}

bool KeyValuesReader::isConditional(const QString& token) noexcept {
    return token.startsWith(QLatin1Char('[')) &&
           token.endsWith(QLatin1Char(']')) &&
           token.contains(QLatin1Char('$'));
}

Core::Result<KeyValuesEventType> KeyValuesReader::readErrorFailure() const {
    return Core::Result<KeyValuesEventType>::failure(
        Core::Error::ErrorCode::ReadFailed,
        QCoreApplication::translate("KeyValuesReader", "Failed to read KeyValues input stream"),
        m_lexer.readErrorString());
}

Core::Result<KeyValuesEventType> KeyValuesReader::readNext() {
    if (m_lexer.hasReadError()) {
        return readErrorFailure();
    }

    const Token token = m_lexer.peekToken();
    if (m_lexer.hasReadError()) {
        return readErrorFailure();
    }

    if (token.isEof()) {
        m_line = token.line;
        m_column = token.column;
        if (!m_sectionStack.empty()) {
            return Core::Result<KeyValuesEventType>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("KeyValuesReader", "Unclosed '{' block (reached EOF)"),
                QStringLiteral("Section '%1' at line %2, column %3")
                    .arg(m_sectionStack.back())
                    .arg(token.line)
                    .arg(token.column));
        }
        m_currentEvent = KeyValuesEventType::EndOfDocument;
        m_name.clear();
        m_value.clear();
        return Core::Result<KeyValuesEventType>::success(m_currentEvent);
    }

    if (token.isCloseBrace()) {
        m_lexer.nextToken(); // Consume '}'
        m_line = token.line;
        m_column = token.column;

        if (m_sectionStack.empty()) {
            return Core::Result<KeyValuesEventType>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("KeyValuesReader", "Unexpected '}' at top level"),
                QStringLiteral("Line %1, column %2").arg(token.line).arg(token.column));
        }

        m_currentEvent = KeyValuesEventType::EndSection;
        m_name = std::move(m_sectionStack.back());
        m_sectionStack.pop_back();
        m_value.clear();
        return Core::Result<KeyValuesEventType>::success(m_currentEvent);
    }

    if (token.isOpenBrace()) {
        m_line = token.line;
        m_column = token.column;
        if (m_sectionStack.empty()) {
            return Core::Result<KeyValuesEventType>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QCoreApplication::translate("KeyValuesReader", "Unexpected '{' without a preceding key"),
                QStringLiteral("Line %1, column %2").arg(token.line).arg(token.column));
        }
        return Core::Result<KeyValuesEventType>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("KeyValuesReader", "Unexpected '{' inside section"),
            QStringLiteral("Section '%1' at line %2, column %3")
                .arg(m_sectionStack.back())
                .arg(token.line)
                .arg(token.column));
    }

    if (!token.isString()) {
        m_line = token.line;
        m_column = token.column;
        return Core::Result<KeyValuesEventType>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("KeyValuesReader", "Expected string token for key"),
            QStringLiteral("Line %1, column %2").arg(token.line).arg(token.column));
    }

    Token keyToken = m_lexer.nextToken();
    m_line = keyToken.line;
    m_column = keyToken.column;

    if (m_lexer.hasReadError()) {
        return readErrorFailure();
    }

    // Skip conditional if it appeared directly after the key
    if (m_options.supportConditionals) {
        const Token maybeCond = m_lexer.peekToken();
        if (m_lexer.hasReadError()) {
            return readErrorFailure();
        }
        if (maybeCond.isString() && isConditional(maybeCond.text)) {
            m_lexer.nextToken();
        }
    }

    // Peek before consuming so a standalone key before '}' or EOF does not swallow '}'
    const Token nextTok = m_lexer.peekToken();
    if (m_lexer.hasReadError()) {
        return readErrorFailure();
    }

    if (nextTok.isEof() || nextTok.isCloseBrace()) {
        m_currentEvent = KeyValuesEventType::Property;
        m_name = std::move(keyToken.text);
        m_value.clear();
        return Core::Result<KeyValuesEventType>::success(m_currentEvent);
    }

    if (nextTok.isOpenBrace()) {
        m_lexer.nextToken(); // Consume '{'

        if (m_options.stopUnquotedAtBraces && !m_sectionStack.empty()) {
            const Token afterBrace = m_lexer.peekToken();
            if (m_lexer.hasReadError()) {
                return readErrorFailure();
            }
            if (afterBrace.isString() && !afterBrace.quoted && isNumericToken(afterBrace.text)) {
                QString collected = QStringLiteral("{");
                bool first = true;
                while (true) {
                    Token part = m_lexer.nextToken();
                    if (m_lexer.hasReadError()) {
                        return readErrorFailure();
                    }
                    if (part.isCloseBrace()) {
                        collected += QLatin1Char('}');
                        break;
                    }
                    if (!part.isString()) {
                        return Core::Result<KeyValuesEventType>::failure(
                            Core::Error::ErrorCode::InvalidFile,
                            QCoreApplication::translate("KeyValuesReader", "Unclosed '{' in property value"),
                            QStringLiteral("Key '%1' at line %2, column %3")
                                .arg(keyToken.text)
                                .arg(nextTok.line)
                                .arg(nextTok.column));
                    }
                    if (!first) {
                        collected += QLatin1Char(' ');
                    }
                    collected += part.text;
                    first = false;
                }

                if (m_options.supportConditionals) {
                    const Token maybeCond = m_lexer.peekToken();
                    if (m_lexer.hasReadError()) {
                        return readErrorFailure();
                    }
                    if (maybeCond.isString() && isConditional(maybeCond.text)) {
                        m_lexer.nextToken();
                    }
                }

                m_currentEvent = KeyValuesEventType::Property;
                m_name = std::move(keyToken.text);
                m_value = std::move(collected);
                return Core::Result<KeyValuesEventType>::success(m_currentEvent);
            }
        }

        m_sectionStack.push_back(std::move(keyToken.text));
        m_currentEvent = KeyValuesEventType::BeginSection;
        m_name = m_sectionStack.back();
        m_value.clear();
        return Core::Result<KeyValuesEventType>::success(m_currentEvent);
    }

    if (nextTok.isString()) {
        Token valToken = m_lexer.nextToken();
        if (m_lexer.hasReadError()) {
            return readErrorFailure();
        }

        if (m_options.stopUnquotedAtBraces &&
            !valToken.quoted &&
            valToken.text.startsWith(QLatin1Char('[')) &&
            !valToken.text.endsWith(QLatin1Char(']'))) {
            QString collected = valToken.text;
            while (true) {
                Token part = m_lexer.nextToken();
                if (m_lexer.hasReadError()) {
                    return readErrorFailure();
                }
                if (!part.isString()) {
                    return Core::Result<KeyValuesEventType>::failure(
                        Core::Error::ErrorCode::InvalidFile,
                        QCoreApplication::translate("KeyValuesReader", "Unclosed '[' in property value"),
                        QStringLiteral("Key '%1' at line %2, column %3")
                            .arg(keyToken.text)
                            .arg(valToken.line)
                            .arg(valToken.column));
                }
                if (collected != QLatin1String("[") && part.text != QLatin1String("]")) {
                    collected += QLatin1Char(' ');
                }
                collected += part.text;
                if (part.text.endsWith(QLatin1Char(']'))) {
                    break;
                }
            }
            valToken.text = std::move(collected);
        }

        if (m_options.supportConditionals) {
            const Token maybeCond = m_lexer.peekToken();
            if (m_lexer.hasReadError()) {
                return readErrorFailure();
            }
            if (maybeCond.isString() && isConditional(maybeCond.text)) {
                m_lexer.nextToken();
            }
        }

        m_currentEvent = KeyValuesEventType::Property;
        m_name = std::move(keyToken.text);
        m_value = std::move(valToken.text);
        return Core::Result<KeyValuesEventType>::success(m_currentEvent);
    }

    return Core::Result<KeyValuesEventType>::failure(
        Core::Error::ErrorCode::InvalidFile,
        QCoreApplication::translate("KeyValuesReader", "Unexpected token"),
        QStringLiteral("Line %1, column %2").arg(nextTok.line).arg(nextTok.column));
}

Core::Result<KeyValuesNode> KeyValuesReader::readSubtree() {
    if (m_currentEvent != KeyValuesEventType::BeginSection) {
        return Core::Result<KeyValuesNode>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("KeyValuesReader", "readSubtree() requires currentEvent() to be BeginSection"),
            QStringLiteral("Current event: %1 at line %2, column %3")
                .arg(static_cast<int>(m_currentEvent))
                .arg(m_line)
                .arg(m_column));
    }

    std::vector<KeyValuesNode> nodes;
    nodes.push_back(KeyValuesNode::makeSection(m_name));

    while (true) {
        auto nextRes = readNext();
        if (!nextRes.isSuccess()) {
            return Core::Result<KeyValuesNode>::failure(nextRes.error());
        }

        const KeyValuesEventType ev = nextRes.value();
        if (ev == KeyValuesEventType::Property) {
            nodes.back().addProperty(m_name, m_value);
        } else if (ev == KeyValuesEventType::BeginSection) {
            nodes.push_back(KeyValuesNode::makeSection(m_name));
        } else if (ev == KeyValuesEventType::EndSection) {
            if (nodes.size() == 1) {
                return Core::Result<KeyValuesNode>::success(std::move(nodes.back()));
            }
            KeyValuesNode completed = std::move(nodes.back());
            nodes.pop_back();
            nodes.back().addChild(std::move(completed));
        } else {
            return Core::Result<KeyValuesNode>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("KeyValuesReader", "Unexpected end of document while reading subtree"));
        }
    }
}

Core::Result<void> KeyValuesReader::skipCurrentSection() {
    if (m_currentEvent != KeyValuesEventType::BeginSection) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("KeyValuesReader", "skipCurrentSection() requires currentEvent() to be BeginSection"),
            QStringLiteral("Current event: %1 at line %2, column %3")
                .arg(static_cast<int>(m_currentEvent))
                .arg(m_line)
                .arg(m_column));
    }

    const int targetDepth = depth() - 1;
    while (true) {
        auto nextRes = readNext();
        if (!nextRes.isSuccess()) {
            return Core::Result<void>::failure(nextRes.error());
        }
        if (nextRes.value() == KeyValuesEventType::EndSection && depth() == targetDepth) {
            return Core::Result<void>::success();
        }
    }
}

Core::Result<void> KeyValuesReader::readDocument(KeyValuesNode& rootNode) {
    rootNode = KeyValuesNode::makeSection(QString());

    while (true) {
        auto nextRes = readNext();
        if (!nextRes.isSuccess()) {
            return Core::Result<void>::failure(nextRes.error());
        }

        const KeyValuesEventType ev = nextRes.value();
        if (ev == KeyValuesEventType::EndOfDocument) {
            return Core::Result<void>::success();
        }
        if (ev == KeyValuesEventType::Property) {
            rootNode.addProperty(m_name, m_value);
        } else if (ev == KeyValuesEventType::BeginSection) {
            auto subRes = readSubtree();
            if (!subRes.isSuccess()) {
                return Core::Result<void>::failure(subRes.error());
            }
            rootNode.addChild(std::move(subRes.value()));
        } else {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidState,
                QCoreApplication::translate("KeyValuesReader", "Unexpected event at document root"));
        }
    }
}

} // namespace Core::KeyValues
