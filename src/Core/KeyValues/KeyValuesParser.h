#pragma once

#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesLexer.h"
#include "Core/Error/Exception.h"
#include <QString>

namespace Core::KeyValues {

class KeyValuesParser {
public:
    KeyValuesParser() = default;

    // Parses the source string into the provided root node (which acts as the top-level container).
    // Returns true on success, false on error.
    bool parse(const QString& source, KeyValuesNode& rootNode, QString* errorMessage = nullptr);

    // Parses the source string and returns the root container node.
    // Throws Core::Error::Exception if a fatal parsing error occurs.
    KeyValuesNode parseOrThrow(const QString& source);

private:
    bool parseBlock(KeyValuesLexer& lexer, KeyValuesNode& parentNode, QString* errorMessage);
    bool isConditional(const QString& token) const noexcept;
};

} // namespace Core::KeyValues

