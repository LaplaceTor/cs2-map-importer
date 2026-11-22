#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/Result/Result.h"

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QStringView>

#include <vector>

namespace Core::KeyValues {

/**
 * @brief Streaming and DOM-based KeyValues text writer supporting VDF and VMF dialects.
 *
 * Supports two output modes:
 * - In-memory mode (`KeyValuesWriter(options)`): appends UTF-8 bytes directly to an internal buffer.
 * - Device mode (`KeyValuesWriter(device, options)`): buffers UTF-8 bytes and auto-flushes to `device`
 *   whenever the internal buffer reaches 64 KB.
 */
class KeyValuesWriter {
public:
    enum class QuoteStyle : int {
        AlwaysQuote,
        PreserveOrAuto,
        NeverQuote,
    };

    struct Options {
        QuoteStyle sectionQuoteStyle = QuoteStyle::AlwaysQuote;
        QuoteStyle propertyQuoteStyle = QuoteStyle::AlwaysQuote;
        bool escapeBackslashes = true;
        bool escapeQuotes = true;
        QByteArray indent = "\t";
        QByteArray propertySeparator = "\t";
        QByteArray newline = "\n";
        bool nullOutput = false;

        static Options vdfDefault() { return {}; }
        static Options vmf(bool nullOutput = false) {
            return Options{
                .sectionQuoteStyle = QuoteStyle::NeverQuote,
                .propertyQuoteStyle = QuoteStyle::AlwaysQuote,
                .escapeBackslashes = false,
                .escapeQuotes = false,
                .indent = "\t",
                .propertySeparator = " ",
                .newline = "\r\n",
                .nullOutput = nullOutput,
            };
        }
    };

    explicit KeyValuesWriter(Options options = {});
    explicit KeyValuesWriter(QIODevice* device, Options options = {});

    void startSection(QStringView name);
    bool endSection(QStringView expectedName = {});
    void writeProperty(QStringView key, QStringView value);
    void writeNode(const KeyValuesNode& node);

    int depth() const noexcept;
    const std::vector<QString>& openSections() const noexcept;

    Core::Result<void> flush();
    QByteArray finish();
    void clear();
    const QByteArray& buffer() const noexcept;

    static QByteArray toUtf8(const KeyValuesNode& rootNode, const Options& options = {});
    static QString toString(const KeyValuesNode& rootNode, const Options& options = {});
    static Core::Result<void> write(QIODevice& device, const KeyValuesNode& rootNode, const Options& options = {});

private:
    void validateDevice();
    bool canEmitBytes() const noexcept;
    void appendIndent();
    void appendToken(QStringView token, QuoteStyle style);
    void appendUtf8Raw(QStringView text);
    void appendUtf8Escaped(QStringView text);
    Core::Result<void> flushToDevice();
    void maybeAutoFlush();

    Options m_options;
    QIODevice* m_device = nullptr;
    bool m_hasDevice = false;
    QByteArray m_buffer;
    std::vector<QString> m_sectionStack;
    Core::Error::Error m_writeError;
};

} // namespace Core::KeyValues
