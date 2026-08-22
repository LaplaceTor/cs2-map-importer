#include "Core/KeyValues/KeyValuesWriter.h"

namespace Core::KeyValues {

QString KeyValuesWriter::formatToken(const QString& token, QuoteStyle style) {
    if (style == QuoteStyle::PreserveOrAuto) {
        bool needsQuote = token.isEmpty();
        for (const QChar& c : token) {
            if (c.isSpace() || c == QChar('{') || c == QChar('}') || c == QChar('"') || c == QChar('/')) {
                needsQuote = true;
                break;
            }
        }
        if (!needsQuote) {
            return token;
        }
    }

    QString escaped = token;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

void KeyValuesWriter::writeNode(QTextStream& stream, const KeyValuesNode& node, int indentLevel, const Options& options) {
    QString currentIndent;
    for (int i = 0; i < indentLevel; ++i) {
        currentIndent += options.indent;
    }

    if (node.isProperty()) {
        stream << currentIndent
               << formatToken(node.name(), options.quoteStyle)
               << options.indent
               << formatToken(node.value(), options.quoteStyle)
               << options.newline;
    } else {
        // Section
        if (!node.name().isEmpty()) {
            stream << currentIndent << formatToken(node.name(), options.quoteStyle) << options.newline;
            stream << currentIndent << QStringLiteral("{") << options.newline;
            for (const auto& child : node.children()) {
                writeNode(stream, child, indentLevel + 1, options);
            }
            stream << currentIndent << QStringLiteral("}") << options.newline;
        } else {
            // Root anonymous container
            for (const auto& child : node.children()) {
                writeNode(stream, child, indentLevel, options);
            }
        }
    }
}

QString KeyValuesWriter::toString(const KeyValuesNode& rootNode, const Options& options) {
    QString output;
    QTextStream stream(&output);
    write(stream, rootNode, options);
    return output;
}

void KeyValuesWriter::write(QTextStream& stream, const KeyValuesNode& rootNode, const Options& options) {
    if (rootNode.name().isEmpty() && rootNode.isSection()) {
        for (const auto& child : rootNode.children()) {
            writeNode(stream, child, 0, options);
        }
    } else {
        writeNode(stream, rootNode, 0, options);
    }
}

} // namespace Core::KeyValues

