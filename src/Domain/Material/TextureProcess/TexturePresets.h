#pragma once

#include <array>

namespace Domain::Material::TextureProcess {

/**
 * @brief Frequency-equalizer presets ported verbatim from Materialize
 *        (GPLv3, https://github.com/maikramer/Materialize) — the button
 *        values of HeightFromDiffuseGui.cs and NormalFromHeightGui.cs.
 *
 * Index 0..6 maps to band 0..6 (Blur0..Blur6). The Default presets are
 * identical to the parameter defaults in HeightParams / NormalParams and are
 * included for completeness so UI layers can enumerate all buttons.
 */
struct HeightWeightPreset {
    const char* id;
    std::array<float, 7> weights;
};

struct HeightContrastPreset {
    const char* id;
    std::array<float, 7> contrasts;
};

struct NormalWeightPreset {
    const char* id;
    std::array<float, 7> weights;
};

inline constexpr std::array<HeightWeightPreset, 3> kHeightWeightPresets = {{
    {"Default", {0.15f, 0.19f, 0.30f, 0.50f, 0.70f, 0.90f, 1.00f}},
    {"Detail", {0.70f, 0.40f, 0.30f, 0.50f, 0.80f, 0.90f, 0.70f}},
    {"Displace", {0.02f, 0.03f, 0.10f, 0.35f, 0.70f, 0.90f, 1.00f}},
}};

inline constexpr std::array<HeightContrastPreset, 3> kHeightContrastPresets = {{
    {"Default", {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}},
    {"Cracked Mud", {1.0f, 1.0f, 1.0f, 1.0f, -0.2f, -2.0f, -4.0f}},
    {"Funky", {-3.0f, -1.2f, 0.3f, 1.3f, 2.0f, 2.5f, 2.0f}},
}};

inline constexpr std::array<NormalWeightPreset, 4> kNormalWeightPresets = {{
    {"Default", {0.30f, 0.35f, 0.50f, 0.80f, 1.00f, 0.95f, 0.80f}},
    {"Smooth", {0.10f, 0.15f, 0.25f, 0.60f, 0.90f, 1.00f, 1.00f}},
    {"Crisp", {1.00f, 0.90f, 0.60f, 0.40f, 0.25f, 0.15f, 0.10f}},
    {"Mids", {0.15f, 0.50f, 0.85f, 1.00f, 0.85f, 0.50f, 0.15f}},
}};

} // namespace Domain::Material::TextureProcess
