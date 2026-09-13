#pragma once

// Internal shared color math for the texture generation pipelines. Ported
// from Materialize (GPLv3, https://github.com/maikramer/Materialize):
// Assets/Shaders/Resources/Photoshop.cginc (RGBToHSL) and the helper
// functions of Blit_Shader.shader / Blit_Height_From_Normal.shader.
// Not part of the public Domain API — include from the generator .cpp files.

#include <algorithm>
#include <cmath>

namespace Domain::Material::TextureProcess::ColorMath {

/// HLSL saturate().
inline float saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

/// Luminance weights shared by every shader of the reference pipeline.
inline float luminance(float r, float g, float b)
{
    return r * 0.3f + g * 0.5f + b * 0.2f;
}

/// Deterministic per-sample hash (fragAO / fragHeight rand()).
inline float sampleRandom(float u, float v, float seed)
{
    const float value = std::sin(u * 12.9898f + v * 78.233f + seed * 137.9462f) * 43758.5453f;
    return value - std::floor(value);
}

/// RGBToHSL from Photoshop.cginc (operates on whatever space the shader
/// samples — here linear RGB).
inline void rgbToHsl(float r, float g, float b, float& h, float& s, float& l)
{
    const float cmin = std::min(std::min(r, g), b);
    const float cmax = std::max(std::max(r, g), b);
    const float delta = cmax - cmin;
    l = (cmax + cmin) / 2.0f;
    if (delta == 0.0f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }
    s = l < 0.5f ? delta / (cmax + cmin) : delta / (2.0f - cmax - cmin);
    const float deltaR = ((cmax - r) / 6.0f + delta / 2.0f) / delta;
    const float deltaG = ((cmax - g) / 6.0f + delta / 2.0f) / delta;
    const float deltaB = ((cmax - b) / 6.0f + delta / 2.0f) / delta;
    if (r == cmax) {
        h = deltaB - deltaG;
    } else if (g == cmax) {
        h = 1.0f / 3.0f + deltaR - deltaB;
    } else {
        h = 2.0f / 3.0f + deltaG - deltaR;
    }
    if (h < 0.0f) {
        h += 1.0f;
    } else if (h > 1.0f) {
        h -= 1.0f;
    }
}

/// HLSL smoothstep() with a degenerate-range guard.
inline float smoothStep(float low, float high, float value)
{
    if (std::abs(high - low) < 1e-8f) {
        return value < low ? 0.0f : 1.0f;
    }
    const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// HueToRGB helper of HSLToRGB (Photoshop.cginc).
inline float hueToRgb(float f1, float f2, float hue)
{
    if (hue < 0.0f) {
        hue += 1.0f;
    } else if (hue > 1.0f) {
        hue -= 1.0f;
    }
    if (6.0f * hue < 1.0f) {
        return f1 + (f2 - f1) * 6.0f * hue;
    }
    if (2.0f * hue < 1.0f) {
        return f2;
    }
    if (3.0f * hue < 2.0f) {
        return f1 + (f2 - f1) * ((2.0f / 3.0f) - hue) * 6.0f;
    }
    return f1;
}

/// HSLToRGB from Photoshop.cginc.
inline void hslToRgb(float h, float s, float l, float& r, float& g, float& b)
{
    if (s == 0.0f) {
        r = l;
        g = l;
        b = l;
        return;
    }
    const float f2 = l < 0.5f ? l * (1.0f + s) : (l + s) - (s * l);
    const float f1 = 2.0f * l - f2;
    r = hueToRgb(f1, f2, h + (1.0f / 3.0f));
    g = hueToRgb(f1, f2, h);
    b = hueToRgb(f1, f2, h - (1.0f / 3.0f));
}

} // namespace Domain::Material::TextureProcess::ColorMath
