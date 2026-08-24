#pragma once

#include <QString>
#include <QStringView>
#include <optional>

namespace Core::KeyValues {

enum class TokenType {
    EndOfFile,
    OpenBrace,
    CloseBrace,
    String
};

struct Token {
    TokenType type = TokenType::EndOfFile;
    QString text;
    int line = 1;
    int column = 1;

    bool is(TokenType t) const noexcept { return type == t; }
    bool isString() const noexcept { return type == TokenType::String; }
    bool isOpenBrace() const noexcept { return type == TokenType::OpenBrace; }
    bool isCloseBrace() const noexcept { return type == TokenType::CloseBrace; }
    bool isEof() const noexcept { return type == TokenType::EndOfFile; }
};

class KeyValuesLexer {
public:
    explicit KeyValuesLexer(QString source);

    Token nextToken();
    Token peekToken();
    bool hasNext() const noexcept;

    int currentLine() const noexcept { return m_line; }
    int currentColumn() const noexcept { return m_column; }

private:
    void skipWhitespaceAndComments();
    Token readQuotedString();
    Token readUnquotedString();
    QChar peekChar(int offset = 0) const;
    QChar advanceChar();

    QString m_source;
    int m_pos = 0;
    int m_line = 1;
    int m_column = 1;
    std::optional<Token> m_peeked = std::nullopt;
};

} // namespace Core::KeyValues

