#include "Core/KeyValues/KeyValuesParser.h"
#include "Core/Error/ErrorCode.h"
#include <utility>

namespace Core::KeyValues {

bool KeyValuesParser::isConditional(const QString& token) const noexcept {
    return token.startsWith(QLatin1Char('[')) && token.endsWith(QLatin1Char(']')) && token.contains(QLatin1Char('$'));
}

Core::Result<void> KeyValuesParser::parse(const QString& source, KeyValuesNode& rootNode) {
    rootNode.clear();
    rootNode.setName(QString());

    KeyValuesLexer lexer(source);
    while (lexer.hasNext()) {
        Token token = lexer.peekToken();
        if (token.isEof()) {
            break;
        }

        if (token.isCloseBrace()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Unexpected '}' at top level"),
                QStringLiteral("Line %1, column %2").arg(token.line).arg(token.column));
        }

        if (token.isOpenBrace()) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Unexpected '{' without a preceding key"),
                QStringLiteral("Line %1, column %2").arg(token.line).arg(token.column));
        }

        auto blockRes = parseBlock(lexer, rootNode);
        if (!blockRes.isSuccess()) {
            return blockRes;
        }
    }

    return Core::Result<void>::success();
}

KeyValuesNode KeyValuesParser::parseOrThrow(const QString& source) {
    KeyValuesNode root;
    auto res = parse(source, root);
    if (!res.isSuccess()) {
        throw Error::Exception(
            res.error().code(),
            QStringLiteral("KeyValues parsing failed: %1").arg(res.message()),
            res.details());
    }
    return root;
}

Core::Result<void> KeyValuesParser::parseBlock(KeyValuesLexer& lexer, KeyValuesNode& parentNode) {
    Token keyToken = lexer.nextToken();
    if (!keyToken.isString()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QStringLiteral("Expected string token for key"),
            QStringLiteral("Line %1, column %2").arg(keyToken.line).arg(keyToken.column));
    }

    // Skip conditional if it appeared directly after the key (rare but possible)
    if (lexer.hasNext() && lexer.peekToken().isString() && isConditional(lexer.peekToken().text)) {
        lexer.nextToken(); // consume conditional
    }

    if (!lexer.hasNext()) {
        // End of file with single property key with empty value
        parentNode.addProperty(keyToken.text, QString());
        return Core::Result<void>::success();
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
                return Core::Result<void>::failure(
                    Core::Error::ErrorCode::InvalidFile,
                    QStringLiteral("Unexpected '{' inside section"),
                    QStringLiteral("Section '%1' at line %2, column %3")
                        .arg(keyToken.text)
                        .arg(childToken.line)
                        .arg(childToken.column));
            }

            auto childBlockRes = parseBlock(lexer, sectionNode);
            if (!childBlockRes.isSuccess()) {
                return childBlockRes;
            }
        }

        if (!closed) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidFile,
                QStringLiteral("Unclosed '{' block (reached EOF)"),
                QStringLiteral("Section '%1'").arg(keyToken.text));
        }

        parentNode.addChild(std::move(sectionNode));
        return Core::Result<void>::success();
    }

    if (nextToken.isString()) {
        // Property: "key" "value"
        QString val = nextToken.text;

        // Skip trailing conditional if present (e.g. "key" "value" [$WIN32])
        if (lexer.hasNext() && lexer.peekToken().isString() && isConditional(lexer.peekToken().text)) {
            lexer.nextToken(); // consume conditional
        }

        parentNode.addProperty(keyToken.text, std::move(val));
        return Core::Result<void>::success();
    }

    if (nextToken.isCloseBrace()) {
        // Standalone key before close brace
        parentNode.addProperty(keyToken.text, QString());
        return Core::Result<void>::success();
    }

    return Core::Result<void>::failure(
        Core::Error::ErrorCode::InvalidFile,
        QStringLiteral("Unexpected token"),
        QStringLiteral("Line %1, column %2").arg(nextToken.line).arg(nextToken.column));
}

} // namespace Core::KeyValues
