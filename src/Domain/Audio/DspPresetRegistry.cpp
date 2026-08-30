#include "Domain/Audio/DspPresetRegistry.h"
#include <array>

namespace Domain::Audio {

namespace {

const std::array<DspPresetInfo, 29> kDspPresets = {{
    {0,  QStringLiteral("Normal (off)"),      QString(),                                     false, 0.0},
    {1,  QStringLiteral("Generic"),           QStringLiteral("reverb_1_generic"),           true,  0.3},
    {2,  QStringLiteral("Metal Small"),       QStringLiteral("reverb_2_smallMetal"),        true,  0.3},
    {3,  QStringLiteral("Metal Medium"),      QStringLiteral("reverb_3_smallTunnels"),      true,  0.3},
    {4,  QStringLiteral("Metal Large"),       QStringLiteral("reverb_4_largeMetal"),        true,  0.3},
    {5,  QStringLiteral("Tunnel Small"),      QStringLiteral("reverb_5_smallRoom"),         true,  0.3},
    {6,  QStringLiteral("Tunnel Medium"),     QStringLiteral("reverb_6_largeRoom"),         true,  0.3},
    {7,  QStringLiteral("Tunnel Large"),      QStringLiteral("reverb_7_mediumHall"),        true,  0.3},
    {8,  QStringLiteral("Chamber Small"),     QStringLiteral("reverb_8_smallChamber"),      true,  0.3},
    {9,  QStringLiteral("Chamber Medium"),    QStringLiteral("reverb_9_mediumChamber"),     true,  0.3},
    {10, QStringLiteral("Chamber Large"),     QStringLiteral("reverb_10_largeChamber"),     true,  0.3},
    {11, QStringLiteral("Bright Small"),      QStringLiteral("reverb_11_smallBright"),      true,  0.3},
    {12, QStringLiteral("Bright Medium"),     QStringLiteral("reverb_12_mediumBright"),     true,  0.3},
    {13, QStringLiteral("Bright Large"),      QStringLiteral("reverb_13_largeBright"),      true,  0.3},
    {14, QStringLiteral("Water 1"),           QStringLiteral("reverb_14_water1"),           true,  0.3},
    {15, QStringLiteral("Water 2"),           QStringLiteral("reverb_15_water2"),           true,  0.3},
    {16, QStringLiteral("Water 3"),           QStringLiteral("reverb_16_water3"),           true,  0.3},
    {17, QStringLiteral("Concrete Small"),    QStringLiteral("reverb_17_smallConcrete"),    true,  0.3},
    {18, QStringLiteral("Concrete Medium"),   QStringLiteral("reverb_18_mediumConcrete"),   true,  0.3},
    {19, QStringLiteral("Concrete Large"),    QStringLiteral("reverb_19_largeConcrete"),    true,  0.3},
    {20, QStringLiteral("Outside Alley"),     QStringLiteral("reverb_20_outsideAlley"),     true,  1.0},
    {21, QStringLiteral("Outside Street"),    QStringLiteral("reverb_21_outsideStreet"),    true,  1.0},
    {22, QStringLiteral("Outside Open"),      QStringLiteral("reverb_22_outsideOpen"),      true,  1.0},
    {23, QStringLiteral("Cavern Small"),      QStringLiteral("reverb_23_smallCavern"),      true,  0.3},
    {24, QStringLiteral("Cavern Medium"),     QStringLiteral("reverb_24_mediumCavern"),     true,  0.3},
    {25, QStringLiteral("Cavern Large"),      QStringLiteral("reverb_25_largeCavern"),      true,  0.3},
    {26, QStringLiteral("Weirdo 1"),          QStringLiteral("reverb_26_weirdo1"),          true,  0.3},
    {27, QStringLiteral("Weirdo 2"),          QStringLiteral("reverb_27_weirdo2"),          true,  0.3},
    {28, QStringLiteral("Weirdo 3"),          QStringLiteral("reverb_28_weirdo3"),          true,  0.3}
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

