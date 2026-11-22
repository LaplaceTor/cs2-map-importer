#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <optional>

namespace Core::KeyValues {

/**
 * @brief Token kinds produced by KeyValuesLexer.
 */
enum class TokenType : int {
    EndOfFile,
    OpenBrace,
    CloseBrace,
    String,
};

/**
 * @brief Lexer dialect options controlling string escape handling.
 */
struct KeyValuesLexerOptions {
    bool unescapeStrings = true;

    static KeyValuesLexerOptions vdfDefault() noexcept { return {}; }
    static KeyValuesLexerOptions vmf() noexcept { return {.unescapeStrings = false}; }
};

/**
 * @brief Single lexical token with 1-based source position.
 */
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

/**
 * @brief Streaming UTF-8 lexer for Valve KeyValues (KV1 / VDF / VMF / VMT) inputs.
 *
 * Supports QIODevice* via a 64 KB sliding chunk buffer as well as in-memory
 * QByteArray and QString inputs, automatically skipping any leading UTF-8 BOM.
 */
class KeyValuesLexer {
public:
    explicit KeyValuesLexer(QIODevice* device, KeyValuesLexerOptions options = {});
    explicit KeyValuesLexer(QByteArray utf8Source, KeyValuesLexerOptions options = {});
    explicit KeyValuesLexer(const QString& source, KeyValuesLexerOptions options = {});

    Token nextToken();
    Token peekToken();
    bool hasNext() const noexcept;

    bool hasReadError() const noexcept { return m_readError; }
    QString readErrorString() const { return m_readErrorString; }

    int currentLine() const noexcept { return m_line; }
    int currentColumn() const noexcept { return m_column; }

private:
    void initStream();
    void skipUtf8Bom();
    void refill();
    unsigned char peekByte(qsizetype offset = 0) const noexcept;
    unsigned char advanceByte();
    void skipWhitespaceAndComments();
    Token readQuotedString();
    Token readUnquotedString();

    QIODevice* m_device = nullptr;
    KeyValuesLexerOptions m_options;
    QByteArray m_buffer;
    qsizetype m_pos = 0;
    bool m_eof = false;
    bool m_readError = false;
    QString m_readErrorString;
    int m_line = 1;
    int m_column = 1;
    std::optional<Token> m_peeked = std::nullopt;
};

} // namespace Core::KeyValues
