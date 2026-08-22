#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include <QString>
#include <QTextStream>

namespace Core::KeyValues {

class KeyValuesWriter {
public:
    enum class QuoteStyle {
        AlwaysQuote,
        PreserveOrAuto
    };

    struct Options {
        QuoteStyle quoteStyle = QuoteStyle::AlwaysQuote;
        QString indent = QStringLiteral("\t");
        QString newline = QStringLiteral("\n");
    };

    static QString toString(const KeyValuesNode& rootNode, const Options& options = {});
    static void write(QTextStream& stream, const KeyValuesNode& rootNode, const Options& options = {});

private:
    static void writeNode(QTextStream& stream, const KeyValuesNode& node, int indentLevel, const Options& options);
    static QString formatToken(const QString& token, QuoteStyle style);
};

} // namespace Core::KeyValues

