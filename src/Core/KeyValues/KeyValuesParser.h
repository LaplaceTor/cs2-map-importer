#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesLexer.h"
#include "Core/Result/Result.h"
#include "Core/Error/Exception.h"
#include <QString>

namespace Core::KeyValues {

class KeyValuesParser {
public:
    KeyValuesParser() = default;

    // Parses the source string into the provided root node (which acts as the top-level container).
    Core::Result<void> parse(const QString& source, KeyValuesNode& rootNode);

    // Parses the source string and returns the root container node.
    // Throws Core::Error::Exception if a fatal parsing error occurs.
    KeyValuesNode parseOrThrow(const QString& source);

    private:
    Core::Result<void> parseBlock(KeyValuesLexer& lexer, KeyValuesNode& parentNode);
    bool isConditional(const QString& token) const noexcept;
};

} // namespace Core::KeyValues
