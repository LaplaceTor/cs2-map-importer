#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <optional>

#include "Domain/Material/MaterialTextureSynthesizer.h"
#include "Domain/Material/VmtDocument.h"
#include "Domain/Material/VtexBuilder.h"
#include "Domain/Material/VtfCodec.h"

namespace Domain::Material {

/**
 * @brief Modern CS2 target shader (or non-VMAT disposition) for a Source 1 VMT.
 */
enum class TargetShader : int {
    CsgoEnvironment,
    CsgoEnvironmentBlend,
    CsgoComplex,
    CsgoStaticOverlay,
    CsgoGlass,
    CsgoWaterFancy,
    CsgoLightmapped4WayBlend,
    CsgoRefract,
    CsgoEffects,
    CsgoFoliage,
    CsgoCharacter,
    CsgoEyeball,
    CsgoBlackUnlit,
    Cables,
    Sky,
    ExplicitVtexOnly,
    Skip,
};

/**
 * @brief Policy for handling `$detail` on LightmappedGeneric world surfaces.
 */
enum class DetailHandlingMode : int {
    PreserveViaComplex,
    BakeToEnvironment,
};

/**
 * @brief Usage context supplied by the caller when planning material translation.
 */
enum class MaterialUsage : int {
    WorldSurface,
    WorldOverlay,
    Model,
    ExplicitSprite,
    SkyboxFace,
};

/**
 * @brief Translation policy flags for MaterialTranslator.
 */
struct MaterialTranslatePolicy {
    DetailHandlingMode detailMode = DetailHandlingMode::PreserveViaComplex;
    bool emitLegacyImportMetadata = false;
};

/**
 * @brief Pre-probed VTF header and alpha metadata passed to MaterialTranslator::plan.
 */
struct VtfProbeInfo {
    std::optional<VtfHeaderInfo> header;
    bool hasNonOpaqueAlpha = false;
};

/**
 * @brief Pixel synthesis / export mode for a planned texture asset.
 */
enum class TextureExportFlavor : int {
    OpaqueColor,
    StraightRgba,
    AlphaMask,
    NormalDirectX,
    SsbumpToNormal,
    RoughnessFromSpecMask,
    RoughnessFromPhongExp,
    MetalnessFromSpecMask,
    ChannelMask,
    LuminanceMask,
    LinearTintedColor,
    TwoTextureCombine,
    LinearPassThrough,
    AnimatedColorAtlas,
    AnimatedAlphaAtlas,
};

/**
 * @brief Single texture export instruction emitted by MaterialTranslator::plan.
 */
struct TextureExportItem {
    QString role;
    QString sourceVtfPath;
    QString secondaryVtfPath;
    TextureExportFlavor flavor = TextureExportFlavor::OpaqueColor;
    TextureChannel sourceChannel = TextureChannel::Alpha;
    std::optional<std::array<float, 3>> envmapTint;
    bool isMetallic = false;
    std::optional<std::array<float, 3>> linearTint;
    std::optional<VmtTransform> secondaryTransform;
    QString targetRelativePngPath;
};

/**
 * @brief Pure translation plan describing target shader, required texture exports, and vtex disposition.
 */
struct MaterialPlan {
    TargetShader targetShader = TargetShader::Skip;
    QString targetShaderFileName;
    QVector<TextureExportItem> textureExports;
    bool needsExplicitVtex = false;
    bool needsFrameAtlas = false;
    VtexPreset vtexPreset = VtexPreset::Sprite2DAlpha;
    QStringList warnings;
};

/**
 * @brief Output of MaterialTranslator::buildVmat.
 */
struct VmatBuildOutput {
    QString vmatContent;
    TargetShader targetShader = TargetShader::Skip;
    QStringList warnings;
};

/**
 * @brief Returns the CS2 `.vfx` shader filename for a TargetShader (empty for ExplicitVtexOnly/Skip).
 */
[[nodiscard]] inline QString targetShaderFileName(TargetShader shader)
{
    switch (shader) {
    case TargetShader::CsgoEnvironment:          return QStringLiteral("csgo_environment.vfx");
    case TargetShader::CsgoEnvironmentBlend:     return QStringLiteral("csgo_environment_blend.vfx");
    case TargetShader::CsgoComplex:              return QStringLiteral("csgo_complex.vfx");
    case TargetShader::CsgoStaticOverlay:        return QStringLiteral("csgo_static_overlay.vfx");
    case TargetShader::CsgoGlass:                return QStringLiteral("csgo_glass.vfx");
    case TargetShader::CsgoWaterFancy:           return QStringLiteral("csgo_water_fancy.vfx");
    case TargetShader::CsgoLightmapped4WayBlend: return QStringLiteral("csgo_lightmapped_4wayblend.vfx");
    case TargetShader::CsgoRefract:              return QStringLiteral("csgo_refract.vfx");
    case TargetShader::CsgoEffects:              return QStringLiteral("csgo_effects.vfx");
    case TargetShader::CsgoFoliage:              return QStringLiteral("csgo_foliage.vfx");
    case TargetShader::CsgoCharacter:            return QStringLiteral("csgo_character.vfx");
    case TargetShader::CsgoEyeball:              return QStringLiteral("csgo_eyeball.vfx");
    case TargetShader::CsgoBlackUnlit:           return QStringLiteral("csgo_black_unlit.vfx");
    case TargetShader::Cables:                   return QStringLiteral("cables.vfx");
    case TargetShader::Sky:                      return QStringLiteral("sky.vfx");
    case TargetShader::ExplicitVtexOnly:         return {};
    case TargetShader::Skip:                     return {};
    }
    return {};
}

} // namespace Domain::Material
