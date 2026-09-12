#include "Domain/Audio/DspPresetRegistry.h"
#include <array>

namespace Domain::Audio {

namespace {

const std::array<DspPresetInfo, 29> kDspPresets = {{
    {0,  QT_TRANSLATE_NOOP("DspPreset", "Normal (off)"),      QString(),                                     false, 0.0},
    {1,  QT_TRANSLATE_NOOP("DspPreset", "Generic"),           QStringLiteral("reverb_1_generic"),           true,  0.3},
    {2,  QT_TRANSLATE_NOOP("DspPreset", "Metal Small"),       QStringLiteral("reverb_2_smallMetal"),        true,  0.3},
    {3,  QT_TRANSLATE_NOOP("DspPreset", "Metal Medium"),      QStringLiteral("reverb_3_smallTunnels"),      true,  0.3},
    {4,  QT_TRANSLATE_NOOP("DspPreset", "Metal Large"),       QStringLiteral("reverb_4_largeMetal"),        true,  0.3},
    {5,  QT_TRANSLATE_NOOP("DspPreset", "Tunnel Small"),      QStringLiteral("reverb_5_smallRoom"),         true,  0.3},
    {6,  QT_TRANSLATE_NOOP("DspPreset", "Tunnel Medium"),     QStringLiteral("reverb_6_largeRoom"),         true,  0.3},
    {7,  QT_TRANSLATE_NOOP("DspPreset", "Tunnel Large"),      QStringLiteral("reverb_7_mediumHall"),        true,  0.3},
    {8,  QT_TRANSLATE_NOOP("DspPreset", "Chamber Small"),     QStringLiteral("reverb_8_smallChamber"),      true,  0.3},
    {9,  QT_TRANSLATE_NOOP("DspPreset", "Chamber Medium"),    QStringLiteral("reverb_9_mediumChamber"),     true,  0.3},
    {10, QT_TRANSLATE_NOOP("DspPreset", "Chamber Large"),     QStringLiteral("reverb_10_largeChamber"),     true,  0.3},
    {11, QT_TRANSLATE_NOOP("DspPreset", "Bright Small"),      QStringLiteral("reverb_11_smallBright"),      true,  0.3},
    {12, QT_TRANSLATE_NOOP("DspPreset", "Bright Medium"),     QStringLiteral("reverb_12_mediumBright"),     true,  0.3},
    {13, QT_TRANSLATE_NOOP("DspPreset", "Bright Large"),      QStringLiteral("reverb_13_largeBright"),      true,  0.3},
    {14, QT_TRANSLATE_NOOP("DspPreset", "Water 1"),           QStringLiteral("reverb_14_water1"),           true,  0.3},
    {15, QT_TRANSLATE_NOOP("DspPreset", "Water 2"),           QStringLiteral("reverb_15_water2"),           true,  0.3},
    {16, QT_TRANSLATE_NOOP("DspPreset", "Water 3"),           QStringLiteral("reverb_16_water3"),           true,  0.3},
    {17, QT_TRANSLATE_NOOP("DspPreset", "Concrete Small"),    QStringLiteral("reverb_17_smallConcrete"),    true,  0.3},
    {18, QT_TRANSLATE_NOOP("DspPreset", "Concrete Medium"),   QStringLiteral("reverb_18_mediumConcrete"),   true,  0.3},
    {19, QT_TRANSLATE_NOOP("DspPreset", "Concrete Large"),    QStringLiteral("reverb_19_largeConcrete"),    true,  0.3},
    {20, QT_TRANSLATE_NOOP("DspPreset", "Outside Alley"),     QStringLiteral("reverb_20_outsideAlley"),     true,  1.0},
    {21, QT_TRANSLATE_NOOP("DspPreset", "Outside Street"),    QStringLiteral("reverb_21_outsideStreet"),    true,  1.0},
    {22, QT_TRANSLATE_NOOP("DspPreset", "Outside Open"),      QStringLiteral("reverb_22_outsideOpen"),      true,  1.0},
    {23, QT_TRANSLATE_NOOP("DspPreset", "Cavern Small"),      QStringLiteral("reverb_23_smallCavern"),      true,  0.3},
    {24, QT_TRANSLATE_NOOP("DspPreset", "Cavern Medium"),     QStringLiteral("reverb_24_mediumCavern"),     true,  0.3},
    {25, QT_TRANSLATE_NOOP("DspPreset", "Cavern Large"),      QStringLiteral("reverb_25_largeCavern"),      true,  0.3},
    {26, QT_TRANSLATE_NOOP("DspPreset", "Weirdo 1"),          QStringLiteral("reverb_26_weirdo1"),          true,  0.3},
    {27, QT_TRANSLATE_NOOP("DspPreset", "Weirdo 2"),          QStringLiteral("reverb_27_weirdo2"),          true,  0.3},
    {28, QT_TRANSLATE_NOOP("DspPreset", "Weirdo 3"),          QStringLiteral("reverb_28_weirdo3"),          true,  0.3}
}};

} // namespace

std::optional<DspPresetInfo> DspPresetRegistry::lookupByIndex(int dspIndex) {
    if (dspIndex >= 0 && dspIndex < static_cast<int>(kDspPresets.size())) {
        return kDspPresets[static_cast<size_t>(dspIndex)];
    }
    return std::nullopt;
}

std::optional<DspPresetInfo> DspPresetRegistry::lookupByString(const QString& dspString) {
    bool ok = false;
    int index = dspString.trimmed().toInt(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return lookupByIndex(index);
}

QString DspPresetRegistry::getPresetName(int dspIndex) {
    auto info = lookupByIndex(dspIndex);
    return info.has_value() ? info->s2PresetName : QString();
}

} // namespace Domain::Audio

