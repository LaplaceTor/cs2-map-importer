#include "Core/KeyValues/KeyValuesLexer.h"

namespace Core::KeyValues {

KeyValuesLexer::KeyValuesLexer(QString source)
    : m_source(std::move(source)), m_pos(0), m_line(1), m_column(1) {
}

QChar KeyValuesLexer::peekChar(int offset) const {
    const int targetPos = m_pos + offset;
    if (targetPos >= 0 && targetPos < m_source.size()) {
        return m_source.at(targetPos);
    }
    return QChar('\0');
}

QChar KeyValuesLexer::advanceChar() {
    if (m_pos >= m_source.size()) {
        return QChar('\0');
    }
    const QChar c = m_source.at(m_pos++);
    if (c == QChar('\n')) {
        ++m_line;
        m_column = 1;
    } else {
        ++m_column;
    }
    return c;
}

void KeyValuesLexer::skipWhitespaceAndComments() {
    while (m_pos < m_source.size()) {
        const QChar c = peekChar();

        // Whitespace
        if (c.isSpace()) {
            advanceChar();
            continue;
        }

        // Single line comments //
        if (c == QChar('/') && peekChar(1) == QChar('/')) {
            advanceChar();
            advanceChar();
            while (m_pos < m_source.size() && peekChar() != QChar('\n')) {
                advanceChar();
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
    if (m_peeked.has_value() && !m_peeked->isEof()) {
        return true;
    }
    return m_pos < m_source.size();
}

Token KeyValuesLexer::nextToken() {
    if (m_peeked.has_value()) {
        Token token = std::move(*m_peeked);
        m_peeked.reset();
        return token;
    }

    skipWhitespaceAndComments();

    if (m_pos >= m_source.size()) {
        return Token{TokenType::EndOfFile, QString(), m_line, m_column};
    }

    const int tokenLine = m_line;
    const int tokenCol = m_column;
    const QChar c = peekChar();

    if (c == QChar('{')) {
        const QChar next = peekChar(1);
        if (next.isSpace() || next == QChar('\0') || (next == QChar('/') && peekChar(2) == QChar('/'))) {
            advanceChar();
            return Token{TokenType::OpenBrace, QStringLiteral("{"), tokenLine, tokenCol};
        }
    }

    if (c == QChar('}')) {
        const QChar next = peekChar(1);
        if (next.isSpace() || next == QChar('\0') || (next == QChar('/') && peekChar(2) == QChar('/'))) {
            advanceChar();
            return Token{TokenType::CloseBrace, QStringLiteral("}"), tokenLine, tokenCol};
        }
    }

    if (c == QChar('"')) {
        return readQuotedString();
    }

    return readUnquotedString();
}

Token KeyValuesLexer::readQuotedString() {
    const int tokenLine = m_line;
    const int tokenCol = m_column;

    advanceChar(); // Consume opening '"'

    QString result;
    result.reserve(64);

    while (m_pos < m_source.size()) {
        const QChar c = peekChar();
        if (c == QChar('"')) {
            advanceChar(); // Consume closing '"'
            break;
        }

        if (c == QChar('\\')) {
            advanceChar(); // Consume '\\'
            if (m_pos < m_source.size()) {
                const QChar esc = advanceChar();
                switch (esc.toLatin1()) {
                    case '"': result.append(QChar('"')); break;
                    case '\\': result.append(QChar('\\')); break;
                    case 'n': result.append(QChar('\n')); break;
                    case 't': result.append(QChar('\t')); break;
                    case 'r': result.append(QChar('\r')); break;
                    default:
                        result.append(QChar('\\'));
                        result.append(esc);
                        break;
                }
            }
            continue;
        }

        if (c == QChar('\n')) {
            // Unclosed quote spanning multiple lines or raw newline
            result.append(advanceChar());
            continue;
        }

        result.append(advanceChar());
    }

    return Token{TokenType::String, std::move(result), tokenLine, tokenCol};
}

Token KeyValuesLexer::readUnquotedString() {
    const int tokenLine = m_line;
    const int tokenCol = m_column;

    QString result;
    result.reserve(64);

    while (m_pos < m_source.size()) {
        const QChar c = peekChar();

        if (c.isSpace() || c == QChar('"')) {
            break;
        }

        // Comment start
        if (c == QChar('/') && peekChar(1) == QChar('/')) {
            break;
        }

        result.append(advanceChar());
    }

    return Token{TokenType::String, std::move(result), tokenLine, tokenCol};
}

} // namespace Core::KeyValues

