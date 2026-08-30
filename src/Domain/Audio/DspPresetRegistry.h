#pragma once

#include <QString>
#include <optional>

namespace Domain::Audio {

struct DspPresetInfo {
    int s1DspIndex = 0;
    QString s1Description;
    QString s2PresetName;
    bool overrideDsp = false;
    double defaultReverbWet = 1.0;
};

class DspPresetRegistry {
public:
    /**
     * @brief Resolves Source 1 DSP index (0-28) to Source 2 reverb preset metadata.
     */
    static std::optional<DspPresetInfo> lookupByIndex(int dspIndex);

    /**
     * @brief Resolves from string containing integer DSP index.
     */
    static std::optional<DspPresetInfo> lookupByString(const QString& dspString);

    /**
     * @brief Returns Source 2 preset name for given DSP index (or empty string if disabled/unknown).
     */
    static QString getPresetName(int dspIndex);
};

} // namespace Domain::Audio

