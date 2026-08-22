#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/Error/ImportError.h"

namespace Core::KeyValues {

bool KeyValuesParser::isConditional(const QString& token) const noexcept {
    return token.startsWith(QLatin1Char('[')) && token.endsWith(QLatin1Char(']')) && token.contains(QLatin1Char('$'));
}

bool KeyValuesParser::parse(const QString& source, KeyValuesNode& rootNode, QString* errorMessage) {
    rootNode.clear();
    rootNode.setName(QString());

    KeyValuesLexer lexer(source);
    while (lexer.hasNext()) {
        Token token = lexer.peekToken();
        if (token.isEof()) {
            break;
        }

        if (token.isCloseBrace()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unexpected '}' at line %1, column %2")
                                    .arg(token.line)
                                    .arg(token.column);
            }
            return false;
        }

        if (token.isOpenBrace()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unexpected '{' without a preceding key at line %1, column %2")
                                    .arg(token.line)
                                    .arg(token.column);
            }
            return false;
        }

        if (!parseBlock(lexer, rootNode, errorMessage)) {
            return false;
        }
    }

    return true;
}

KeyValuesNode KeyValuesParser::parseOrThrow(const QString& source) {
    KeyValuesNode root;
    QString errorMessage;
    if (!parse(source, root, &errorMessage)) {
        throw Error::ImportException(
            Error::ImportErrorCode::InvalidFile,
            QStringLiteral("KeyValues parsing failed: %1").arg(errorMessage));
    }
    return root;
}

bool KeyValuesParser::parseBlock(KeyValuesLexer& lexer, KeyValuesNode& parentNode, QString* errorMessage) {
    Token keyToken = lexer.nextToken();
    if (!keyToken.isString()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Expected string token for key at line %1, column %2")
                                .arg(keyToken.line)
                                .arg(keyToken.column);
        }
        return false;
    }

    // Skip conditional if it appeared directly after the key (rare but possible)
    if (lexer.hasNext() && lexer.peekToken().isString() && isConditional(lexer.peekToken().text)) {
        lexer.nextToken(); // consume conditional
    }

    if (!lexer.hasNext()) {
        // End of file with single property key with empty value
        parentNode.addProperty(keyToken.text, QString());
        return true;
    }

    Token nextToken = lexer.nextToken();

    if (nextToken.isOpenBrace()) {
        // Section: "key" { ... }
        KeyValuesNode sectionNode = KeyValuesNode::makeSection(keyToken.text);
        bool closed = false;

        while (lexer.hasNext()) {
            Token childToken = lexer.peekToken();
            if (childToken.isEof()) {
                break;
            }

            if (childToken.isCloseBrace()) {
                lexer.nextToken(); // consume '}'
                closed = true;
                break;
            }

            if (childToken.isOpenBrace()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Unexpected '{' inside section '%1' at line %2, column %3")
                                        .arg(keyToken.text)
                                        .arg(childToken.line)
                                        .arg(childToken.column);
                }
                return false;
            }

            if (!parseBlock(lexer, sectionNode, errorMessage)) {
                return false;
            }
        }

        if (!closed) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Unclosed '{' block starting for section '%1' (reached EOF)")
                                    .arg(keyToken.text);
            }
            return false;
        }

        parentNode.addChild(std::move(sectionNode));
        return true;
    }

    if (nextToken.isString()) {
        // Property: "key" "value"
        QString val = nextToken.text;

        // Skip trailing conditional if present (e.g. "key" "value" [$WIN32])
        if (lexer.hasNext() && lexer.peekToken().isString() && isConditional(lexer.peekToken().text)) {
            lexer.nextToken(); // consume conditional
        }

        parentNode.addProperty(keyToken.text, std::move(val));
        return true;
    }

    if (nextToken.isCloseBrace()) {
        // Standalone key before close brace
        parentNode.addProperty(keyToken.text, QString());
        // Do not consume close brace, let the caller see it next
        // But we already consumed nextToken via lexer.nextToken()!
        // To handle cleanly, the caller's loop will see the matching structure
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Unexpected token at line %1, column %2")
                            .arg(nextToken.line)
                            .arg(nextToken.column);
    }
    return false;
}

} // namespace Core::KeyValues
