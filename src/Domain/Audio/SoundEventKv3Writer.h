#pragma once

#include "Domain/Audio/SoundEvent.h"
#include "Core/Result/Result.h"
#include "Core/Path/FilesystemPath.h"
#include <QString>
#include <vector>

namespace Domain::Audio {

class SoundEventKv3Writer {
public:
    /**
     * @brief Writes a list of SoundEvents into standard Source 2 KV3 text format.
     */
    static QString writeToString(const std::vector<SoundEvent>& events);

    /**
     * @brief Writes a list of SoundEvents into a .vsndevts file on disk.
     */
    static Core::Result<void> writeToFile(const Core::Path::FilesystemPath& filePath, const std::vector<SoundEvent>& events);

private:
    static void writeSoundEvent(QString& out, const SoundEvent& ev);
    static void writeCurve(QString& out, const QString& curveName, const std::vector<CurveControlPoint>& points);
};

} // namespace Domain::Audio

