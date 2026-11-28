#pragma once

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "Domain/Material/MaterialConvertTypes.h"
#include "Domain/Material/VmtDocument.h"

namespace Domain::Material {

/**
 * @brief CS2 `F_TEXTURE_ANIMATION_MODE` enumeration for animated atlas materials.
 */
enum class TextureAnimationMode : int {
    Sequential = 0,
    Random = 1,
    Scripted = 2,
};

/**
 * @brief Uniform power-of-2 grid atlas dimensions produced for an animated multi-frame VTF.
 */
struct TextureAtlasFrameInfo {
    int cols = 1;
    int rows = 1;
    int cellCount = 1;
};

/**
 * @brief Pre-routing proxy capability hints queried by MaterialTranslator::plan().
 */
struct ProxyRoutingHints {
    bool hasFrameAnimation = false;
    bool hasDynamicAlpha = false;
    bool hasDualAsyncScroll = false;
    bool hasBreakableSurface = false;
};

/**
 * @brief Compiled CS2 animation overrides and DynamicParams generated from Source 1 VMT Proxies.
 */
struct MaterialAnimationPlan {
    bool hasAnimation = false;
    bool usesTextureAnimation = false;
    TextureAnimationMode animationMode = TextureAnimationMode::Sequential;
    float animationTimePerFrame = 0.1f;
    float animationTimeOffset = 0.0f;
    QVector<QPair<QString, QString>> featureOverrides;
    QVector<QPair<QString, QString>> staticParamOverrides;
    QVector<QPair<QString, QString>> dynamicParams;
    QStringList warnings;
};

/**
 * @brief Compiles ordered Source 1 VMT `Proxies` into CS2 static overrides, GPU native
 *        scroll/atlas parameters, or CSE-optimized `DynamicParams` expressions.
 */
class MaterialProxyCompiler {
public:
    /**
     * @brief Lightweight pre-routing scan used before MaterialTranslator::plan() finalizes TargetShader.
     */
    [[nodiscard]] static ProxyRoutingHints analyzeRoutingHints(const VmtMaterial& vmt);

    /**
     * @brief Compiles `vmt.proxies` for `targetShader` via SSA symbolic evaluation, constant
     *        folding, CSE, taint isolation, and shader capability filtering.
     */
    [[nodiscard]] static MaterialAnimationPlan compile(
        const VmtMaterial& vmt,
        TargetShader targetShader,
        const std::optional<TextureAtlasFrameInfo>& atlasInfo = std::nullopt);
};

} // namespace Domain::Material
