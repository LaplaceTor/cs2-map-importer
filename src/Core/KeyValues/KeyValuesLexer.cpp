#include "Core/KeyValues/KeyValuesLexer.h"

#include <cstring>
#include <utility>

namespace Core::KeyValues {

namespace {

constexpr qsizetype kChunkSize = 64 * 1024;
constexpr qsizetype kMinLookahead = 4;

constexpr bool isAsciiWhitespace(unsigned char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

constexpr bool isUtf8LeadingOrAscii(unsigned char c) noexcept {
    return (c & 0xC0) != 0x80;
}

} // namespace

KeyValuesLexer::KeyValuesLexer(QIODevice* device, KeyValuesLexerOptions options)
    : m_device(device), m_options(options) {
    if (m_device == nullptr || !m_device->isOpen() || !m_device->isReadable()) {
        m_readError = true;
        m_readErrorString = (m_device == nullptr)
            ? QStringLiteral("Input QIODevice is null")
            : (m_device->errorString().isEmpty()
                   ? QStringLiteral("Input QIODevice is not open for reading")
                   : m_device->errorString());
        m_eof = true;
        return;
    }

    m_buffer.reserve(kChunkSize + kMinLookahead);
    initStream();
}

KeyValuesLexer::KeyValuesLexer(QByteArray utf8Source, KeyValuesLexerOptions options)
    : m_options(options), m_buffer(std::move(utf8Source)), m_eof(true) {
    skipUtf8Bom();
}

KeyValuesLexer::KeyValuesLexer(const QString& source, KeyValuesLexerOptions options)
    : m_options(options), m_buffer(source.toUtf8()), m_eof(true) {
    skipUtf8Bom();
}

void KeyValuesLexer::initStream() {
    refill();
    skipUtf8Bom();
    if (!m_eof && !m_readError && (m_buffer.size() - m_pos) < kMinLookahead) {
        refill();
    }
}

void KeyValuesLexer::skipUtf8Bom() {
    if ((m_buffer.size() - m_pos) >= 3 &&
        peekByte(0) == 0xEF &&
        peekByte(1) == 0xBB &&
        peekByte(2) == 0xBF) {
        m_pos += 3;
    }
}

void KeyValuesLexer::refill() {
    if (m_device == nullptr || m_eof || m_readError) {
        return;
    }

    const qsizetype remaining = m_buffer.size() - m_pos;
    if (remaining >= kMinLookahead) {
        return;
    }

    if (m_pos > 0) {
        if (remaining > 0) {
            std::memmove(m_buffer.data(), m_buffer.constData() + m_pos, static_cast<size_t>(remaining));
        }
        m_buffer.resize(remaining);
        m_pos = 0;
    }

    while (m_buffer.size() < kMinLookahead && !m_eof && !m_readError) {
        const qsizetype oldSize = m_buffer.size();
        m_buffer.resize(oldSize + kChunkSize);

        const qint64 bytesRead = m_device->read(m_buffer.data() + oldSize, kChunkSize);
        if (bytesRead < 0) {
            m_buffer.resize(oldSize);
            m_readError = true;
            m_readErrorString = m_device->errorString().isEmpty()
                ? QStringLiteral("Failed to read from QIODevice")
                : m_device->errorString();
            m_eof = true;
            break;
        }
        if (bytesRead == 0) {
            m_buffer.resize(oldSize);
            m_eof = true;
            break;
        }

        m_buffer.resize(oldSize + static_cast<qsizetype>(bytesRead));
    }
}

unsigned char KeyValuesLexer::peekByte(qsizetype offset) const noexcept {
    const qsizetype targetPos = m_pos + offset;
    if (targetPos >= 0 && targetPos < m_buffer.size()) {
        return static_cast<unsigned char>(m_buffer.at(targetPos));
    }
    return 0;
}

unsigned char KeyValuesLexer::advanceByte() {
    if (m_pos >= m_buffer.size()) {
        refill();
        if (m_pos >= m_buffer.size()) {
            return 0;
        }
    }

    const unsigned char c = static_cast<unsigned char>(m_buffer.at(m_pos++));
    if (c == '\n') {
        ++m_line;
        m_column = 1;
    } else if (isUtf8LeadingOrAscii(c)) {
        ++m_column;
    }

    if ((m_buffer.size() - m_pos) < kMinLookahead && m_device != nullptr && !m_eof && !m_readError) {
        refill();
    }

    return c;
}

void KeyValuesLexer::skipWhitespaceAndComments() {
    while (m_pos < m_buffer.size()) {
        const unsigned char c = peekByte(0);

        if (isAsciiWhitespace(c)) {
            advanceByte();
            continue;
        }

        if (c == '/' && (m_pos + 1 < m_buffer.size()) && peekByte(1) == '/') {
            advanceByte();
            advanceByte();
            while (m_pos < m_buffer.size() && peekByte(0) != '\n') {
                advanceByte();
            }
            continue;
        }

        break;
    }
}

Token KeyValuesLexer::peekToken() {
    if (!m_peeked.has_value()) {
        m_peeked = nextToken();
    }
    return *m_peeked;
}

bool KeyValuesLexer::hasNext() const noexcept {
    if (m_peeked.has_value()) {
        return !m_peeked->isEof();
    }
    if (m_readError) {
        return false;
    }
    return (m_pos < m_buffer.size()) || (m_device != nullptr && !m_eof);
}

Token KeyValuesLexer::nextToken() {
    if (m_peeked.has_value()) {
        Token token = std::move(*m_peeked);
        m_peeked.reset();
        return token;
    }

    if (m_readError) {
        return Token{TokenType::EndOfFile, QString(), m_line, m_column};
    }

    skipWhitespaceAndComments();

    if (m_pos >= m_buffer.size()) {
        return Token{TokenType::EndOfFile, QString(), m_line, m_column};
    }

    const int tokenLine = m_line;
    const int tokenCol = m_column;
    const unsigned char c = peekByte(0);

    if (c == '{' || c == '}') {
        const bool nextIsEof = (m_pos + 1) >= m_buffer.size();
        const unsigned char next = peekByte(1);
        const bool nextIsComment = (next == '/') && ((m_pos + 2) < m_buffer.size()) && (peekByte(2) == '/');
        if (nextIsEof || isAsciiWhitespace(next) || nextIsComment || next == '"' || next == '{' || next == '}') {
            advanceByte();
            if (c == '{') {
                return Token{TokenType::OpenBrace, QStringLiteral("{"), tokenLine, tokenCol};
            }
            return Token{TokenType::CloseBrace, QStringLiteral("}"), tokenLine, tokenCol};
        }
    }

    if (c == '"') {
        return readQuotedString();
    }

    return readUnquotedString();
}

Token KeyValuesLexer::readQuotedString() {
    const int tokenLine = m_line;
    const int tokenCol = m_column;

    advanceByte(); // Consume opening '"'

    QByteArray bytes;
    bytes.reserve(64);

    while (m_pos < m_buffer.size()) {
        const unsigned char c = peekByte(0);
        if (c == '"') {
            advanceByte(); // Consume closing '"'
            break;
        }

        if (m_options.unescapeStrings && c == '\\' && (m_pos + 1 < m_buffer.size())) {
            const unsigned char next = peekByte(1);
            if (next == '"') {
                advanceByte(); // Consume '\\'
                advanceByte(); // Consume '"'
                bytes.append('"');
                continue;
            }
            if (next == '\\') {
                advanceByte(); // Consume '\\'
                advanceByte(); // Consume second '\\'
                bytes.append('\\');
                continue;
            }
            // For Windows file paths in Valve VDF (e.g. \tools, \nature, \tree),
            // preserve backslash literally rather than treating \t or \n as escape sequences.
            bytes.append(static_cast<char>(advanceByte()));
            continue;
        }

        bytes.append(static_cast<char>(advanceByte()));
    }

    return Token{TokenType::String, QString::fromUtf8(bytes), tokenLine, tokenCol};
}

Token KeyValuesLexer::readUnquotedString() {
    const int tokenLine = m_line;
    const int tokenCol = m_column;

    QByteArray bytes;
    bytes.reserve(64);

    while (m_pos < m_buffer.size()) {
        const unsigned char c = peekByte(0);

        if (isAsciiWhitespace(c) || c == '"') {
            break;
        }

        if (c == '/' && (m_pos + 1 < m_buffer.size()) && peekByte(1) == '/') {
            break;
        }

        bytes.append(static_cast<char>(advanceByte()));
    }

    return Token{TokenType::String, QString::fromUtf8(bytes), tokenLine, tokenCol};
}

} // namespace Core::KeyValues
