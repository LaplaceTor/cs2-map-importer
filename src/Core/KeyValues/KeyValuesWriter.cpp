#include "Core/KeyValues/KeyValuesWriter.h"

#include <QCoreApplication>

#include <utility>

namespace Core::KeyValues {

namespace {

constexpr qsizetype kFlushThresholdBytes = 65536;

bool needsQuotingForPreserveOrAuto(QStringView token) noexcept {
    if (token.isEmpty()) {
        return true;
    }
    for (const QChar c : token) {
        if (c.isSpace() || c == u'{' || c == u'}' || c == u'"' || c == u'/') {
            return true;
        }
    }
    return false;
}

bool needsQuotingForPreserveOrAutoUtf8(QByteArrayView token) {
    if (token.isEmpty()) {
        return true;
    }
    for (const char ch : token) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (uc >= 0x80u) {
            return needsQuotingForPreserveOrAuto(QString::fromUtf8(token.data(), token.size()));
        }
        if (uc == ' ' || uc == '\t' || uc == '\n' || uc == '\r' || uc == '\v' || uc == '\f'
            || uc == '{' || uc == '}' || uc == '"' || uc == '/') {
            return true;
        }
    }
    return false;
}

bool equalsUtf8(QStringView utf16, QByteArrayView utf8) {
    if (utf16.size() == utf8.size()) {
        for (qsizetype i = 0; i < utf8.size(); ++i) {
            const unsigned char b = static_cast<unsigned char>(utf8[i]);
            if (b >= 0x80u) {
                return utf16 == QString::fromUtf8(utf8.data(), utf8.size());
            }
            if (utf16[i].unicode() != b) {
                return false;
            }
        }
        return true;
    }
    for (const char ch : utf8) {
        if (static_cast<unsigned char>(ch) >= 0x80u) {
            return utf16 == QString::fromUtf8(utf8.data(), utf8.size());
        }
    }
    return false;
}

void appendCodePointUtf8(QByteArray& out, char32_t cp) {
    if (cp <= 0x7Fu) {
        out.append(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.append(static_cast<char>(0xC0u | (cp >> 6)));
        out.append(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.append(static_cast<char>(0xE0u | (cp >> 12)));
        out.append(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.append(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.append(static_cast<char>(0xF0u | (cp >> 18)));
        out.append(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.append(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.append(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

void appendReplacementCharUtf8(QByteArray& out) {
    out.append('\xEF');
    out.append('\xBF');
    out.append('\xBD');
}

} // namespace

KeyValuesWriter::KeyValuesWriter(Options options)
    : m_options(std::move(options)) {
}

KeyValuesWriter::KeyValuesWriter(QIODevice* device, Options options)
    : m_options(std::move(options))
    , m_device(device)
    , m_hasDevice(true) {
    validateDevice();
}

void KeyValuesWriter::validateDevice() {
    if (!m_hasDevice || m_options.nullOutput || m_writeError.isFailure()) {
        return;
    }
    if (m_device == nullptr) {
        m_writeError = Core::Error::Error::writeFailed(
            QCoreApplication::translate("KeyValuesWriter", "Output device is null"));
        return;
    }
    if (!m_device->isOpen() || !m_device->isWritable()) {
        m_writeError = Core::Error::Error::writeFailed(
            QCoreApplication::translate("KeyValuesWriter", "Output device is not open for writing"),
            m_device->errorString());
    }
}

bool KeyValuesWriter::canEmitBytes() const noexcept {
    if (m_options.nullOutput) {
        return false;
    }
    if (m_hasDevice && m_writeError.isFailure()) {
        return false;
    }
    return true;
}

void KeyValuesWriter::appendIndent() {
    if (m_options.indent.isEmpty() || m_sectionStack.empty()) {
        return;
    }
    for (std::size_t i = 0; i < m_sectionStack.size(); ++i) {
        m_buffer.append(m_options.indent);
    }
}

void KeyValuesWriter::appendUtf8Raw(QStringView text) {
    if (text.isEmpty()) {
        return;
    }
    m_buffer.reserve(m_buffer.size() + text.size());
    const qsizetype len = text.size();
    for (qsizetype i = 0; i < len; ++i) {
        const char16_t u = text[i].unicode();
        if (u < 0x80u) {
            m_buffer.append(static_cast<char>(u));
        } else if (u >= 0xD800u && u <= 0xDBFFu) {
            if (i + 1 < len) {
                const char16_t lo = text[i + 1].unicode();
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    const char32_t cp = 0x10000u
                        + ((static_cast<char32_t>(u - 0xD800u)) << 10)
                        + static_cast<char32_t>(lo - 0xDC00u);
                    appendCodePointUtf8(m_buffer, cp);
                    ++i;
                    continue;
                }
            }
            appendReplacementCharUtf8(m_buffer);
        } else if (u >= 0xDC00u && u <= 0xDFFFu) {
            appendReplacementCharUtf8(m_buffer);
        } else {
            appendCodePointUtf8(m_buffer, static_cast<char32_t>(u));
        }
    }
}

void KeyValuesWriter::appendUtf8Escaped(QStringView text) {
    if (text.isEmpty()) {
        return;
    }
    if (!m_options.escapeBackslashes && !m_options.escapeQuotes) {
        appendUtf8Raw(text);
        return;
    }

    m_buffer.reserve(m_buffer.size() + text.size());
    const qsizetype len = text.size();
    for (qsizetype i = 0; i < len; ++i) {
        const char16_t u = text[i].unicode();
        if (u == u'\\' && m_options.escapeBackslashes) {
            m_buffer.append("\\\\", 2);
        } else if (u == u'"' && m_options.escapeQuotes) {
            m_buffer.append("\\\"", 2);
        } else if (u < 0x80u) {
            m_buffer.append(static_cast<char>(u));
        } else if (u >= 0xD800u && u <= 0xDBFFu) {
            if (i + 1 < len) {
                const char16_t lo = text[i + 1].unicode();
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    const char32_t cp = 0x10000u
                        + ((static_cast<char32_t>(u - 0xD800u)) << 10)
                        + static_cast<char32_t>(lo - 0xDC00u);
                    appendCodePointUtf8(m_buffer, cp);
                    ++i;
                    continue;
                }
            }
            appendReplacementCharUtf8(m_buffer);
        } else if (u >= 0xDC00u && u <= 0xDFFFu) {
            appendReplacementCharUtf8(m_buffer);
        } else {
            appendCodePointUtf8(m_buffer, static_cast<char32_t>(u));
        }
    }
}

void KeyValuesWriter::appendUtf8BytesEscaped(QByteArrayView bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    if (!m_options.escapeBackslashes && !m_options.escapeQuotes) {
        m_buffer.append(bytes.data(), bytes.size());
        return;
    }

    m_buffer.reserve(m_buffer.size() + bytes.size());
    const char* const data = bytes.data();
    const qsizetype len = bytes.size();
    qsizetype spanStart = 0;
    for (qsizetype i = 0; i < len; ++i) {
        const char ch = data[i];
        if (ch == '\\' && m_options.escapeBackslashes) {
            if (i > spanStart) {
                m_buffer.append(data + spanStart, i - spanStart);
            }
            m_buffer.append("\\\\", 2);
            spanStart = i + 1;
        } else if (ch == '"' && m_options.escapeQuotes) {
            if (i > spanStart) {
                m_buffer.append(data + spanStart, i - spanStart);
            }
            m_buffer.append("\\\"", 2);
            spanStart = i + 1;
        }
    }
    if (len > spanStart) {
        m_buffer.append(data + spanStart, len - spanStart);
    }
}

void KeyValuesWriter::appendToken(QStringView token, QuoteStyle style) {
    switch (style) {
    case QuoteStyle::NeverQuote:
        appendUtf8Raw(token);
        return;
    case QuoteStyle::PreserveOrAuto:
        if (!needsQuotingForPreserveOrAuto(token)) {
            appendUtf8Raw(token);
            return;
        }
        break;
    case QuoteStyle::AlwaysQuote:
        break;
    }

    m_buffer.append('"');
    appendUtf8Escaped(token);
    m_buffer.append('"');
}

void KeyValuesWriter::appendTokenUtf8(QByteArrayView token, QuoteStyle style) {
    switch (style) {
    case QuoteStyle::NeverQuote:
        if (!token.isEmpty()) {
            m_buffer.append(token.data(), token.size());
        }
        return;
    case QuoteStyle::PreserveOrAuto:
        if (!needsQuotingForPreserveOrAutoUtf8(token)) {
            if (!token.isEmpty()) {
                m_buffer.append(token.data(), token.size());
            }
            return;
        }
        break;
    case QuoteStyle::AlwaysQuote:
        break;
    }

    m_buffer.append('"');
    appendUtf8BytesEscaped(token);
    m_buffer.append('"');
}

Core::Result<void> KeyValuesWriter::flushToDevice() {
    if (!m_hasDevice || m_options.nullOutput) {
        return Core::Result<void>::success();
    }
    if (m_writeError.isFailure()) {
        return Core::Result<void>::failure(m_writeError);
    }

    validateDevice();
    if (m_writeError.isFailure()) {
        return Core::Result<void>::failure(m_writeError);
    }

    qint64 offset = 0;
    const qint64 total = m_buffer.size();
    while (offset < total) {
        const qint64 written = m_device->write(m_buffer.constData() + offset, total - offset);
        if (written <= 0) {
            m_writeError = Core::Error::Error::writeFailed(
                QCoreApplication::translate("KeyValuesWriter", "Failed to write KeyValues output to device"),
                m_device->errorString());
            return Core::Result<void>::failure(m_writeError);
        }
        offset += written;
    }

    m_buffer.truncate(0);
    return Core::Result<void>::success();
}

void KeyValuesWriter::maybeAutoFlush() {
    if (m_hasDevice && !m_options.nullOutput && m_writeError.isSuccess() && m_buffer.size() >= kFlushThresholdBytes) {
        (void)flushToDevice();
    }
}

void KeyValuesWriter::startSection(QStringView name) {
    if (canEmitBytes()) {
        appendIndent();
        appendToken(name, m_options.sectionQuoteStyle);
        m_buffer.append(m_options.newline);
        appendIndent();
        m_buffer.append('{');
        m_buffer.append(m_options.newline);
        maybeAutoFlush();
    }
    m_sectionStack.push_back(name.toString());
}

bool KeyValuesWriter::endSection(QStringView expectedName) {
    if (m_sectionStack.empty()) {
        return false;
    }
    if (!expectedName.isEmpty() && m_sectionStack.back() != expectedName) {
        return false;
    }

    m_sectionStack.pop_back();
    if (canEmitBytes()) {
        appendIndent();
        m_buffer.append('}');
        m_buffer.append(m_options.newline);
        maybeAutoFlush();
    }
    return true;
}

void KeyValuesWriter::writeProperty(QStringView key, QStringView value) {
    if (!canEmitBytes()) {
        return;
    }
    appendIndent();
    appendToken(key, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.propertySeparator);
    appendToken(value, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.newline);
    maybeAutoFlush();
}

void KeyValuesWriter::startSectionUtf8(QByteArrayView name) {
    if (canEmitBytes()) {
        appendIndent();
        appendTokenUtf8(name, m_options.sectionQuoteStyle);
        m_buffer.append(m_options.newline);
        appendIndent();
        m_buffer.append('{');
        m_buffer.append(m_options.newline);
        maybeAutoFlush();
    }
    m_sectionStack.push_back(name.isEmpty() ? QString() : QString::fromUtf8(name.data(), name.size()));
}

bool KeyValuesWriter::endSectionUtf8(QByteArrayView expectedName) {
    if (m_sectionStack.empty()) {
        return false;
    }
    if (!expectedName.isEmpty() && !equalsUtf8(m_sectionStack.back(), expectedName)) {
        return false;
    }
    return endSection();
}

void KeyValuesWriter::writePropertyUtf8(QByteArrayView key, QByteArrayView value) {
    if (!canEmitBytes()) {
        return;
    }
    appendIndent();
    appendTokenUtf8(key, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.propertySeparator);
    appendTokenUtf8(value, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.newline);
    maybeAutoFlush();
}

void KeyValuesWriter::writePropertyUtf8(QByteArrayView key, QStringView value) {
    if (!canEmitBytes()) {
        return;
    }
    appendIndent();
    appendTokenUtf8(key, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.propertySeparator);
    appendToken(value, m_options.propertyQuoteStyle);
    m_buffer.append(m_options.newline);
    maybeAutoFlush();
}

void KeyValuesWriter::writeNode(const KeyValuesNode& node) {
    if (node.isProperty()) {
        writeProperty(node.name(), node.value());
        return;
    }

    if (node.name().isEmpty()) {
        for (const auto& child : node.children()) {
            writeNode(child);
        }
        return;
    }

    startSection(node.name());
    for (const auto& child : node.children()) {
        writeNode(child);
    }
    (void)endSection(node.name());
}

int KeyValuesWriter::depth() const noexcept {
    return static_cast<int>(m_sectionStack.size());
}

const std::vector<QString>& KeyValuesWriter::openSections() const noexcept {
    return m_sectionStack;
}

Core::Result<void> KeyValuesWriter::flush() {
    return flushToDevice();
}

QByteArray KeyValuesWriter::finish() {
    if (m_hasDevice) {
        (void)flushToDevice();
    }
    QByteArray out = std::move(m_buffer);
    m_buffer.clear();
    return out;
}

void KeyValuesWriter::clear() {
    m_buffer.clear();
    m_sectionStack.clear();
    m_writeError = Core::Error::Error::success();
    if (m_hasDevice) {
        validateDevice();
    }
}

const QByteArray& KeyValuesWriter::buffer() const noexcept {
    return m_buffer;
}

QByteArray KeyValuesWriter::toUtf8(const KeyValuesNode& rootNode, const Options& options) {
    KeyValuesWriter writer(options);
    writer.writeNode(rootNode);
    return writer.finish();
}

QString KeyValuesWriter::toString(const KeyValuesNode& rootNode, const Options& options) {
    return QString::fromUtf8(toUtf8(rootNode, options));
}

Core::Result<void> KeyValuesWriter::write(QIODevice& device, const KeyValuesNode& rootNode, const Options& options) {
    KeyValuesWriter writer(&device, options);
    writer.writeNode(rootNode);
    return writer.flush();
}

} // namespace Core::KeyValues
