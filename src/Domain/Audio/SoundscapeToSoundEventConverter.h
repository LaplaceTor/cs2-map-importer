#pragma once

#include "Domain/Audio/SoundscapeDefinition.h"
#include "Domain/Audio/SoundEvent.h"
#include <QString>
#include <QSet>
#include <vector>

namespace Domain::Audio {

struct ConversionOptions {
    QString mixgroup;                     ///< Custom mixgroup (e.g. map name). Defaults to "Amb_Common".
};

struct ConversionResult {
    std::vector<SoundEvent> soundEvents;  ///< Generated master and child soundevents
    QSet<QString> uniqueRawSoundAssets;   ///< Source 1 sound paths (e.g. "sound/ambient/wind.wav")
};

class SoundscapeToSoundEventConverter {
public:
    /**
     * @brief Converts a Source 1 soundscape definition to a collection of Source 2 soundevents.
     */
    static ConversionResult convert(const SoundscapeDefinition& def, const ConversionOptions& options = {});

    /**
     * @brief Converts a batch of Source 1 soundscape definitions.
     */
    static ConversionResult convertBatch(const std::vector<SoundscapeDefinition>& defs, const ConversionOptions& options = {});

    /**
     * @brief Formats a Source 1 wave path into a Source 2 .vsnd path (e.g. "ambient/wind.wav" -> "sounds/ambient/wind.vsnd").
     */
    static QString formatVsndPath(const QString& rawWavePath);

    /**
     * @brief Formats a Source 1 wave path into a normalized raw asset path (e.g. "ambient/wind.wav" -> "sound/ambient/wind.wav").
     */
    static QString formatRawAssetPath(const QString& rawWavePath);
};

} // namespace Domain::Audio

