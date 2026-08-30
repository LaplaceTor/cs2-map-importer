#pragma once

#include "Domain/Audio/Range.h"
#include <QString>
#include <QStringList>
#include <vector>
#include <variant>
#include <optional>

namespace Domain::Audio {

struct PlayLoopingElement {
    QString wave;
    DoubleRange volume{1.0};
    DoubleRange pitch{100.0};
    QString soundLevel;
    std::optional<Vector3> origin;
    QString position;
    std::optional<int> positionOverride;
};

struct PlayRandomElement {
    DoubleRange timeInterval{10.0, 30.0};
    DoubleRange volume{1.0};
    DoubleRange pitch{100.0};
    QString soundLevel;
    std::optional<Vector3> origin;
    QString position;
    std::optional<int> positionOverride;
    QStringList waves;
};

struct PlaySoundscapeElement {
    QString targetSoundscape;
    std::optional<double> volume;
    std::optional<Vector3> origin;
    QString position;
    std::optional<int> positionOverride;
};

using SoundscapeElement = std::variant<PlayLoopingElement, PlayRandomElement, PlaySoundscapeElement>;

struct SoundscapeDefinition {
    QString name;
    std::optional<int> dspIndex;
    std::optional<double> dspVolume;
    std::optional<double> fadeTime;
    std::optional<Vector3> origin;
    QString position;
    std::optional<int> positionOverride;
    std::vector<SoundscapeElement> elements;

    bool isEmpty() const noexcept {
        return name.isEmpty() && elements.empty();
    }
};

} // namespace Domain::Audio

