#pragma once

#include "Domain/Audio/SoundscapeDefinition.h"
#include "Core/KeyValues/KeyValuesDocument.h"
#include "Core/Result/Result.h"
#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <vector>

namespace Domain::Audio {

class SoundscapeParser {
public:
    /**
     * @brief Parses a Source 1 soundscape VDF script string into a list of SoundscapeDefinition objects.
     */
    static Core::Result<std::vector<SoundscapeDefinition>> parseString(const QString& content);

    /**
     * @brief Parses a Source 1 soundscape file from disk.
     */
    static Core::Result<std::vector<SoundscapeDefinition>> parseFile(const Core::Path::FilesystemPath& filePath);

    /**
     * @brief Parses SoundscapeDefinitions from an already parsed KeyValuesDocument.
     */
    static Core::Result<std::vector<SoundscapeDefinition>> parseDocument(const Core::KeyValues::KeyValuesDocument& doc);

    /**
     * @brief Parses a single soundscape definition from a root KeyValuesNode.
     */
    static std::optional<SoundscapeDefinition> parseSoundscapeNode(const Core::KeyValues::KeyValuesNode& node);

private:
    static void extractElementsFromNode(const Core::KeyValues::KeyValuesNode& node, std::vector<SoundscapeElement>& elements);
    static PlayLoopingElement parsePlayLooping(const Core::KeyValues::KeyValuesNode& node);
    static PlayRandomElement parsePlayRandom(const Core::KeyValues::KeyValuesNode& node);
    static PlaySoundscapeElement parsePlaySoundscape(const Core::KeyValues::KeyValuesNode& node);
};

} // namespace Domain::Audio

