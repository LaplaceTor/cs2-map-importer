#include "Domain/Material/MaterialTranslator.h"

#include <QCoreApplication>
#include <QPair>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "Core/KeyValues/KeyValuesWriter.h"
#include "Domain/Material/MaterialProxyCompiler.h"
#include "Domain/Material/MaterialTextureSynthesizer.h"

namespace Domain::Material {

namespace {

constexpr const char* kDefaultAoTexture = "materials/default/default_ao.tga";
constexpr const char* kDefaultDetailMask = "[1.000000 1.000000 1.000000 0.000000]";
constexpr const char* kMatteRoughnessVec = "[1.000000 1.000000 1.000000 0.000000]";
constexpr const char* kZeroVec4 = "[0.000000 0.000000 0.000000 0.000000]";
constexpr const char* kWhiteVec4ZeroW = "[1.000000 1.000000 1.000000 0.000000]";

/**
 * @brief Ordered key-value map for Layer0 properties, guaranteeing zero duplicate keys
 *        when merging proxy feature/static overrides over base material parameters.
 */
class OrderedProperties {
public:
    void set(const QString& key, const QString& value)
    {
        for (auto& entry : m_entries) {
            if (entry.first == key) {
                entry.second = value;
                return;
            }
        }
        m_entries.push_back({key, value});
    }

    void remove(const QString& key)
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [&key](const auto& entry) { return entry.first == key; }),
            m_entries.end());
    }

    bool contains(const QString& key) const
    {
        for (const auto& entry : m_entries) {
            if (entry.first == key) {
                return true;
            }
        }
        return false;
    }

    QString value(const QString& key, const QString& defaultValue = {}) const
    {
        for (const auto& entry : m_entries) {
            if (entry.first == key) {
                return entry.second;
            }
        }
        return defaultValue;
    }

    void writeTo(Core::KeyValues::KeyValuesWriter& writer) const
    {
        for (const auto& [k, v] : m_entries) {
            writer.writeProperty(k, v);
        }
    }

private:
    QVector<QPair<QString, QString>> m_entries;
};

QString formatFloat(float value)
{
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    return QString::number(static_cast<double>(value), 'f', 6);
}

QString formatVec2(float x, float y)
{
    return QStringLiteral("[%1 %2]").arg(formatFloat(x), formatFloat(y));
}

QString formatVec4(float x, float y, float z, float w)
{
    return QStringLiteral("[%1 %2 %3 %4]")
        .arg(formatFloat(x), formatFloat(y), formatFloat(z), formatFloat(w));
}

QString formatGrayVec4(float value)
{
    return formatVec4(value, value, value, 0.0f);
}

float srgbToLinear(float c) noexcept
{
    const float clamped = std::clamp(c, 0.0f, 1.0f);
    if (clamped <= 0.04045f) {
        return clamped / 12.92f;
    }
    return std::pow((clamped + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float c) noexcept
{
    const float clamped = std::clamp(c, 0.0f, 1.0f);
    if (clamped <= 0.0031308f) {
        return clamped * 12.92f;
    }
    return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

std::array<float, 3> colorToLinear(const VmtColor& color) noexcept
{
    return {
        srgbToLinear(color.r),
        srgbToLinear(color.g),
        srgbToLinear(color.b),
    };
}

bool colorsApproximatelyEqual(const VmtColor& a, const VmtColor& b) noexcept
{
    constexpr float kEps = 1e-4f;
    return std::abs(a.r - b.r) <= kEps
        && std::abs(a.g - b.g) <= kEps
        && std::abs(a.b - b.b) <= kEps;
}

std::optional<std::array<float, 3>> combinedTintSrgb(
    const std::optional<VmtColor>& c1,
    const std::optional<VmtColor>& c2) noexcept
{
    if (!c1.has_value() && !c2.has_value()) {
        return std::nullopt;
    }
    if (c1.has_value() && !c2.has_value()) {
        return std::array<float, 3>{
            std::clamp(c1->r, 0.0f, 1.0f),
            std::clamp(c1->g, 0.0f, 1.0f),
            std::clamp(c1->b, 0.0f, 1.0f),
        };
    }
    if (!c1.has_value() && c2.has_value()) {
        return std::array<float, 3>{
            std::clamp(c2->r, 0.0f, 1.0f),
            std::clamp(c2->g, 0.0f, 1.0f),
            std::clamp(c2->b, 0.0f, 1.0f),
        };
    }

    const auto lin1 = colorToLinear(*c1);
    const auto lin2 = colorToLinear(*c2);
    return std::array<float, 3>{
        linearToSrgb(lin1[0] * lin2[0]),
        linearToSrgb(lin1[1] * lin2[1]),
        linearToSrgb(lin1[2] * lin2[2]),
    };
}

std::optional<std::array<float, 3>> extractEnvmapTint(const VmtMaterial& vmt)
{
    const auto color = vmt.colorParam(QStringLiteral("$envmaptint"));
    if (!color.has_value()) {
        return std::nullopt;
    }
    return std::array<float, 3>{color->r, color->g, color->b};
}

QString makeTargetPngPath(const QString& normalizedVtfPath, const QString& suffix = {})
{
    if (normalizedVtfPath.isEmpty()) {
        return {};
    }
    return QStringLiteral("materials/%1%2.png").arg(normalizedVtfPath, suffix);
}

QString resolveExportedPath(
    const QHash<QString, QString>& exportedPaths,
    const QString& role,
    const QString& fallback = {})
{
    const auto it = exportedPaths.constFind(role);
    if (it != exportedPaths.constEnd() && !it.value().isEmpty()) {
        return it.value();
    }
    return fallback;
}

bool isToolsMaterialPath(const QString& relativePath)
{
    QString norm = relativePath.trimmed().toLower();
    norm.replace(u'\\', u'/');
    while (norm.startsWith(u'/')) {
        norm.remove(0, 1);
    }
    return norm.startsWith(QStringLiteral("materials/tools/"))
        || norm.startsWith(QStringLiteral("tools/"));
}

bool isSkipShaderName(const QString& shader)
{
    if (shader.startsWith(QStringLiteral("blurfilter"))
        || shader.startsWith(QStringLiteral("sample4x4"))
        || shader.startsWith(QStringLiteral("debug"))) {
        return true;
    }

    return shader == QStringLiteral("weapondecal")
        || shader == QStringLiteral("customweapon")
        || shader == QStringLiteral("customcharacter")
        || shader == QStringLiteral("screenspace_general")
        || shader == QStringLiteral("bloom")
        || shader == QStringLiteral("depthoffield")
        || shader == QStringLiteral("colorcorrection")
        || shader == QStringLiteral("wireframe")
        || shader == QStringLiteral("depthwrite")
        || shader == QStringLiteral("writez")
        || shader == QStringLiteral("writestencil")
        || shader == QStringLiteral("shadow")
        || shader == QStringLiteral("flashlight_shadow_decal")
        || shader == QStringLiteral("occlusion")
        || shader == QStringLiteral("nodraw")
        || shader == QStringLiteral("empty")
        || shader == QStringLiteral("null");
}

bool isUnlitComplexSourceShader(const QString& shader)
{
    return shader == QStringLiteral("unlitgeneric")
        || shader == QStringLiteral("unlittwotexture")
        || shader == QStringLiteral("modulate")
        || shader == QStringLiteral("solidenergy")
        || shader == QStringLiteral("monitorscreen")
        || shader == QStringLiteral("decalbasetimeslightmapalphablendselfillum");
}

bool isGlassSurfaceProp(const VmtMaterial& vmt)
{
    return vmt.stringParam(QStringLiteral("$surfaceprop")).toLower().contains(QStringLiteral("glass"));
}

bool hasTranslucentLikeFlag(const VmtMaterial& vmt)
{
    return vmt.boolParam(QStringLiteral("$translucent"))
        || vmt.boolParam(QStringLiteral("$additive"))
        || vmt.boolParam(QStringLiteral("$vertexalpha"))
        || (vmt.hasParam(QStringLiteral("$alpha")) && vmt.floatParam(QStringLiteral("$alpha"), 1.0f) < 0.999f);
}

QString layerSurfaceProp(const VmtMaterial& vmt, int layerIndex)
{
    if (layerIndex <= 1) {
        return vmt.stringParam(QStringLiteral("$surfaceprop")).trimmed();
    }
    const QString key = QStringLiteral("$surfaceprop%1").arg(layerIndex);
    QString sp = vmt.stringParam(key).trimmed();
    if (sp.isEmpty()) {
        sp = vmt.stringParam(QStringLiteral("$surfaceprop")).trimmed();
    }
    return sp;
}

bool probeHasAlpha(const QHash<QString, VtfProbeInfo>& vtfProbes, const QString& normalizedVtfPath)
{
    if (normalizedVtfPath.isEmpty()) {
        return false;
    }
    const auto it = vtfProbes.constFind(normalizedVtfPath);
    if (it == vtfProbes.constEnd()) {
        // Unknown probe: conservatively allow alpha extraction when VMT requests translucency/alphatest.
        return true;
    }
    return it.value().hasNonOpaqueAlpha;
}

std::uint16_t probeFrameCount(const QHash<QString, VtfProbeInfo>& vtfProbes, const QString& normalizedVtfPath)
{
    if (normalizedVtfPath.isEmpty()) {
        return 1;
    }
    const auto it = vtfProbes.constFind(normalizedVtfPath);
    if (it == vtfProbes.constEnd() || !it.value().header.has_value()) {
        return 1;
    }
    return it.value().header->frameCount;
}

struct ComplexFeatures {
    bool isUnlitMode = false;
    bool alphaTest = false;
    bool translucent = false;
    bool additiveBlend = false;
    bool selfIllum = false;
    bool detailTexture = false;
};

ComplexFeatures resolveComplexFeatures(const VmtMaterial& vmt)
{
    ComplexFeatures f;
    f.isUnlitMode = isUnlitComplexSourceShader(vmt.normalizedShaderName);

    // Allow1(F_ALPHA_TEST, F_TRANSLUCENT): $alphatest takes strict priority.
    f.alphaTest = vmt.boolParam(QStringLiteral("$alphatest"));
    if (!f.alphaTest) {
        f.additiveBlend = vmt.boolParam(QStringLiteral("$additive"));
        f.translucent = f.additiveBlend
            || vmt.boolParam(QStringLiteral("$translucent"))
            || vmt.boolParam(QStringLiteral("$vertexalpha"))
            || (vmt.hasParam(QStringLiteral("$alpha")) && vmt.floatParam(QStringLiteral("$alpha"), 1.0f) < 0.999f);
    }

    f.selfIllum = f.isUnlitMode
        || vmt.boolParam(QStringLiteral("$selfillum"))
        || !vmt.normalizedTexturePath(QStringLiteral("$selfillummask")).isEmpty();

    // Allow1(F_DETAIL_TEXTURE, F_SELF_ILLUM) and Allow1(F_DETAIL_TEXTURE, F_ADDITIVE_BLEND).
    const bool hasDetail = !vmt.normalizedTexturePath(QStringLiteral("$detail")).isEmpty();
    f.detailTexture = hasDetail && !f.selfIllum && !f.additiveBlend;

    return f;
}

struct StaticOverlayMode {
    int blendMode = 0;
    bool lit = true;
};

StaticOverlayMode resolveStaticOverlayMode(const VmtMaterial& vmt)
{
    const QString& shader = vmt.normalizedShaderName;
    if (shader == QStringLiteral("decalmodulate")) {
        return {3, false};
    }
    if (shader == QStringLiteral("modulate")) {
        if (vmt.boolParam(QStringLiteral("$mod2x"))) {
            return {3, false};
        }
        return {5, false};
    }

    // LightmappedGeneric overlay / decal
    if (vmt.boolParam(QStringLiteral("$additive"))) {
        return {4, false};
    }
    if (vmt.boolParam(QStringLiteral("$alphatest"))) {
        return {2, true};
    }
    if (vmt.boolParam(QStringLiteral("$translucent"))) {
        return {1, true};
    }
    return {0, true};
}

void addExport(
    MaterialPlan& plan,
    const QString& role,
    const QString& sourceVtfPath,
    TextureExportFlavor flavor,
    const QString& suffix = {},
    TextureChannel channel = TextureChannel::Alpha,
    const std::optional<std::array<float, 3>>& envmapTint = std::nullopt,
    bool isMetallic = false)
{
    if (sourceVtfPath.isEmpty()) {
        return;
    }
    TextureExportItem item;
    item.role = role;
    item.sourceVtfPath = sourceVtfPath;
    item.flavor = flavor;
    item.sourceChannel = channel;
    item.envmapTint = envmapTint;
    item.isMetallic = isMetallic;
    item.targetRelativePngPath = makeTargetPngPath(sourceVtfPath, suffix);
    plan.textureExports.push_back(std::move(item));
}

bool planLayerRoughnessAndMetalness(
    const VmtMaterial& vmt,
    int layerIndex,
    const QString& roughnessRole,
    const QString& metalnessRole,
    const QString& baseVtf,
    const QString& bumpVtf,
    const QString& surfaceProp,
    bool allowMetalnessTexture,
    MaterialPlan& plan)
{
    const auto envTint = extractEnvmapTint(vmt);
    const bool metallic = MaterialTextureSynthesizer::isMetallicSurfaceProp(surfaceProp);

    const QString phongExpKey = (layerIndex == 1)
        ? QStringLiteral("$phongexponenttexture")
        : QStringLiteral("$phongexponenttexture%1").arg(layerIndex);
    const QString phongExpTex = vmt.normalizedTexturePath(phongExpKey);
    if (!phongExpTex.isEmpty()) {
        addExport(
            plan,
            roughnessRole,
            phongExpTex,
            TextureExportFlavor::RoughnessFromPhongExp,
            QStringLiteral("_rough"),
            TextureChannel::Green,
            envTint,
            metallic);
        return true;
    }

    QString specSource;
    TextureChannel specChannel = TextureChannel::Luminance;

    const QString envMaskKey = (layerIndex == 1)
        ? QStringLiteral("$envmapmask")
        : QStringLiteral("$envmapmask%1").arg(layerIndex);
    const QString envMaskTex = vmt.normalizedTexturePath(envMaskKey);
    if (!envMaskTex.isEmpty()) {
        specSource = envMaskTex;
        specChannel = TextureChannel::Luminance;
    } else {
        const QString normMaskKey = (layerIndex == 1)
            ? QStringLiteral("$normalmapalphaenvmapmask")
            : QStringLiteral("$normalmapalphaenvmapmask%1").arg(layerIndex);
        if (vmt.boolParam(normMaskKey) && !bumpVtf.isEmpty()) {
            specSource = bumpVtf;
            specChannel = TextureChannel::Alpha;
        } else {
            const QString baseEnvKey = (layerIndex == 1)
                ? QStringLiteral("$basealphaenvmapmask")
                : QStringLiteral("$basealphaenvmapmask%1").arg(layerIndex);
            const QString basePhongKey = (layerIndex == 1)
                ? QStringLiteral("$basemapalphaphongmask")
                : QStringLiteral("$basemapalphaphongmask%1").arg(layerIndex);
            if ((vmt.boolParam(baseEnvKey) || vmt.boolParam(basePhongKey)) && !baseVtf.isEmpty()) {
                specSource = baseVtf;
                specChannel = TextureChannel::Alpha;
            }
        }
    }

    if (specSource.isEmpty()) {
        return false;
    }

    addExport(
        plan,
        roughnessRole,
        specSource,
        TextureExportFlavor::RoughnessFromSpecMask,
        QStringLiteral("_rough"),
        specChannel,
        envTint,
        metallic);

    if (allowMetalnessTexture && metallic && !metalnessRole.isEmpty()) {
        addExport(
            plan,
            metalnessRole,
            specSource,
            TextureExportFlavor::MetalnessFromSpecMask,
            QStringLiteral("_metal"),
            specChannel,
            envTint,
            true);
    }

    return true;
}

QString resolveRoughnessValue(
    const VmtMaterial& vmt,
    int layerIndex,
    const QString& roughnessRole,
    const QString& surfaceProp,
    const QHash<QString, QString>& exportedPaths,
    bool forceUnlit = false)
{
    if (forceUnlit) {
        return QString::fromLatin1(kMatteRoughnessVec);
    }

    const QString exported = resolveExportedPath(exportedPaths, roughnessRole);
    if (!exported.isEmpty()) {
        return exported;
    }

    // Priority 1 (scalar): $phongexponent / $phongexponent2
    const QString phongExpKey = (layerIndex == 1)
        ? QStringLiteral("$phongexponent")
        : QStringLiteral("$phongexponent%1").arg(layerIndex);
    if (vmt.hasParam(phongExpKey)) {
        const float expVal = vmt.floatParam(phongExpKey, 5.0f);
        const float r = MaterialTextureSynthesizer::roughnessFromPhongExponent(expVal);
        return formatGrayVec4(r);
    }

    // Priority 2: $surfaceprop physical baseline
    if (const auto baseline = MaterialTextureSynthesizer::surfacePropBaselineRoughness(surfaceProp);
        baseline.has_value()) {
        return formatGrayVec4(*baseline);
    }

    // Priority 3: Mandatory explicit matte fallback
    return QString::fromLatin1(kMatteRoughnessVec);
}

void writeUvTransform(
    OrderedProperties& props,
    const std::optional<VmtTransform>& transform,
    const QString& suffix = {},
    bool includeCenter = true)
{
    if (!transform.has_value() || transform->isIdentity()) {
        return;
    }
    if (includeCenter) {
        props.set(
            QStringLiteral("g_vTexCoordCenter%1").arg(suffix),
            formatVec2(transform->center[0], transform->center[1]));
    }
    props.set(
        QStringLiteral("g_vTexCoordScale%1").arg(suffix),
        formatVec2(transform->scale[0], transform->scale[1]));
    props.set(
        QStringLiteral("g_flTexCoordRotation%1").arg(suffix),
        formatFloat(transform->rotateDeg));
    props.set(
        QStringLiteral("g_vTexCoordOffset%1").arg(suffix),
        formatVec2(transform->translate[0], transform->translate[1]));
}

void writeCommonFlags(
    OrderedProperties& props,
    const VmtMaterial& vmt,
    bool supportsBackfaces,
    bool supportsFog,
    bool supportsDisableZBuffering)
{
    if (supportsBackfaces && vmt.boolParam(QStringLiteral("$nocull"))) {
        props.set(QStringLiteral("F_RENDER_BACKFACES"), QStringLiteral("1"));
    }
    if (supportsDisableZBuffering && vmt.boolParam(QStringLiteral("$ignorez"))) {
        props.set(QStringLiteral("F_DISABLE_Z_BUFFERING"), QStringLiteral("1"));
    }
    if (supportsFog) {
        props.set(
            QStringLiteral("g_bFogEnabled"),
            vmt.boolParam(QStringLiteral("$nofog")) ? QStringLiteral("0") : QStringLiteral("1"));
    }
}

void writeSystemAttributes(
    Core::KeyValues::KeyValuesWriter& writer,
    const VmtMaterial& vmt,
    int maxLayers)
{
    const QString sp1 = vmt.stringParam(QStringLiteral("$surfaceprop")).trimmed();
    const QString sp2 = maxLayers >= 2 ? vmt.stringParam(QStringLiteral("$surfaceprop2")).trimmed() : QString();
    const QString sp3 = maxLayers >= 3 ? vmt.stringParam(QStringLiteral("$surfaceprop3")).trimmed() : QString();
    const QString sp4 = maxLayers >= 4 ? vmt.stringParam(QStringLiteral("$surfaceprop4")).trimmed() : QString();

    if (sp1.isEmpty() && sp2.isEmpty() && sp3.isEmpty() && sp4.isEmpty()) {
        return;
    }

    writer.startSection(QStringLiteral("SystemAttributes"));
    if (!sp1.isEmpty()) {
        writer.writeProperty(QStringLiteral("PhysicsSurfaceProperties"), sp1);
    }
    if (!sp2.isEmpty()) {
        writer.writeProperty(QStringLiteral("PhysicsSurfaceProperties2"), sp2);
    }
    if (!sp3.isEmpty()) {
        writer.writeProperty(QStringLiteral("PhysicsSurfaceProperties3"), sp3);
    }
    if (!sp4.isEmpty()) {
        writer.writeProperty(QStringLiteral("PhysicsSurfaceProperties4"), sp4);
    }
    writer.endSection(QStringLiteral("SystemAttributes"));
}

void writeMapBuilderAttributes(
    Core::KeyValues::KeyValuesWriter& writer,
    const VmtMaterial& vmt,
    bool isWater)
{
    const bool passBullets = vmt.boolParam(QStringLiteral("%compilepassbullets"));
    const bool nonSolid = vmt.boolParam(QStringLiteral("%compilenonsolid"));
    const bool compileWater = isWater && vmt.boolParam(QStringLiteral("%compilewater"));

    if (!passBullets && !nonSolid && !compileWater) {
        return;
    }

    writer.startSection(QStringLiteral("Attributes"));
    if (passBullets) {
        writer.writeProperty(QStringLiteral("mapbuilder.passbullets"), QStringLiteral("1"));
    }
    if (nonSolid || compileWater) {
        writer.writeProperty(QStringLiteral("mapbuilder.nonsolid"), QStringLiteral("1"));
    }
    if (compileWater) {
        writer.writeProperty(QStringLiteral("mapbuilder.water"), QStringLiteral("1"));
    }
    writer.endSection(QStringLiteral("Attributes"));
}

Core::KeyValues::KeyValuesWriter::Options vmatWriterOptions()
{
    Core::KeyValues::KeyValuesWriter::Options opts;
    opts.sectionQuoteStyle = Core::KeyValues::KeyValuesWriter::QuoteStyle::NeverQuote;
    opts.propertyQuoteStyle = Core::KeyValues::KeyValuesWriter::QuoteStyle::AlwaysQuote;
    opts.escapeBackslashes = false;
    opts.escapeQuotes = true;
    opts.indent = "\t";
    opts.propertySeparator = "\t";
    opts.newline = "\n";
    return opts;
}

void applyAnimationOverrides(
    OrderedProperties& props,
    TargetShader targetShader,
    const MaterialAnimationPlan& animPlan)
{
    for (const auto& [featKey, featVal] : animPlan.featureOverrides) {
        props.set(featKey, featVal);
    }

    // Re-enforce FeatureRules after proxy feature overrides.
    if (targetShader == TargetShader::CsgoComplex) {
        if (props.value(QStringLiteral("F_SELF_ILLUM")) == QStringLiteral("1")) {
            // Allow1(F_DETAIL_TEXTURE, F_SELF_ILLUM)
            props.remove(QStringLiteral("F_DETAIL_TEXTURE"));
            props.remove(QStringLiteral("TextureDetail"));
            props.remove(QStringLiteral("TextureDetailMask"));
            props.remove(QStringLiteral("g_vDetailTexCoordScale"));
            props.remove(QStringLiteral("g_flDetailBlendFactor"));

            if (!props.contains(QStringLiteral("g_flSelfIllumAlbedoFactor"))) {
                props.set(QStringLiteral("g_flSelfIllumAlbedoFactor"), QStringLiteral("1.000000"));
            }
            if (!props.contains(QStringLiteral("g_flSelfIllumBrightness"))) {
                props.set(QStringLiteral("g_flSelfIllumBrightness"), QStringLiteral("1.000000"));
            }
            if (!props.contains(QStringLiteral("TextureSelfIllumMask"))) {
                props.set(QStringLiteral("TextureSelfIllumMask"), QString::fromLatin1(kWhiteVec4ZeroW));
            }
        }

        if (props.value(QStringLiteral("F_ADDITIVE_BLEND")) == QStringLiteral("1")) {
            props.set(QStringLiteral("F_TRANSLUCENT"), QStringLiteral("1"));
            // Allow1(F_DETAIL_TEXTURE, F_ADDITIVE_BLEND)
            props.remove(QStringLiteral("F_DETAIL_TEXTURE"));
            props.remove(QStringLiteral("TextureDetail"));
            props.remove(QStringLiteral("TextureDetailMask"));
            props.remove(QStringLiteral("g_vDetailTexCoordScale"));
            props.remove(QStringLiteral("g_flDetailBlendFactor"));
            props.remove(QStringLiteral("F_METALNESS_TEXTURE"));
            props.remove(QStringLiteral("TextureMetalness"));
        }

        if (props.value(QStringLiteral("F_TRANSLUCENT")) == QStringLiteral("1")) {
            // Allow1(F_ALPHA_TEST, F_TRANSLUCENT) and Allow1(F_TRANSLUCENT, F_DO_NOT_CAST_SHADOWS)
            props.remove(QStringLiteral("F_ALPHA_TEST"));
            props.remove(QStringLiteral("g_flAlphaTestReference"));
            props.remove(QStringLiteral("g_flAntiAliasedEdgeStrength"));
            props.remove(QStringLiteral("F_DO_NOT_CAST_SHADOWS"));
        }
    } else if (targetShader == TargetShader::CsgoStaticOverlay) {
        const int blendMode = props.value(QStringLiteral("F_BLEND_MODE"), QStringLiteral("0")).toInt();
        if (blendMode < 0 || blendMode > 2) {
            props.set(QStringLiteral("F_LIT"), QStringLiteral("0"));
        }
    }

    for (const auto& [paramKey, paramVal] : animPlan.staticParamOverrides) {
        props.set(paramKey, paramVal);
    }
}

TargetShader routeTargetShader(
    const VmtMaterial& vmt,
    const ProxyRoutingHints& hints,
    const MaterialTranslatePolicy& policy,
    MaterialUsage usage,
    QStringList& warnings)
{
    if (isToolsMaterialPath(vmt.relativePath)) {
        return TargetShader::Skip;
    }
    if (vmt.boolParam(QStringLiteral("$nodraw"))
        || vmt.boolParam(QStringLiteral("%compilenodraw"))
        || vmt.boolParam(QStringLiteral("$compilenodraw"))) {
        return TargetShader::Skip;
    }

    if (usage == MaterialUsage::SkyboxFace) {
        return TargetShader::Sky;
    }

    const QString& shader = vmt.normalizedShaderName;
    if (shader.isEmpty()) {
        warnings.append(QCoreApplication::translate(
            "MaterialTranslator",
            "Material has an empty shader name; skipping."));
        return TargetShader::Skip;
    }

    if (shader == QStringLiteral("patch")) {
        warnings.append(QCoreApplication::translate(
            "MaterialTranslator",
            "Patch VMT inheritance is not yet supported; skipping material."));
        return TargetShader::Skip;
    }

    if (shader == QStringLiteral("subrect")) {
        warnings.append(QCoreApplication::translate(
            "MaterialTranslator",
            "Subrect VMT requires expanding $material before translation; skipping raw subrect."));
        return TargetShader::Skip;
    }

    if (isSkipShaderName(shader)) {
        return TargetShader::Skip;
    }

    if (shader == QStringLiteral("sky")) {
        return TargetShader::Sky;
    }

    if (shader == QStringLiteral("spritecard")) {
        return TargetShader::ExplicitVtexOnly;
    }

    if (shader == QStringLiteral("sprite")) {
        return (usage == MaterialUsage::ExplicitSprite)
            ? TargetShader::ExplicitVtexOnly
            : TargetShader::CsgoEffects;
    }

    if (hints.hasBreakableSurface) {
        return TargetShader::CsgoGlass;
    }

    if (shader == QStringLiteral("lightmappedgeneric")) {
        if (!vmt.normalizedTexturePath(QStringLiteral("$basetexture2")).isEmpty()) {
            return TargetShader::CsgoEnvironmentBlend;
        }
        if (vmt.boolParam(QStringLiteral("$decal")) || usage == MaterialUsage::WorldOverlay) {
            return TargetShader::CsgoStaticOverlay;
        }
        const bool translucentLike = hasTranslucentLikeFlag(vmt) || hints.hasDynamicAlpha;
        if (translucentLike && isGlassSurfaceProp(vmt)) {
            return TargetShader::CsgoGlass;
        }
        const bool hasSelfIllum = vmt.boolParam(QStringLiteral("$selfillum"))
            || !vmt.normalizedTexturePath(QStringLiteral("$selfillummask")).isEmpty();
        const bool preserveDetail = !vmt.normalizedTexturePath(QStringLiteral("$detail")).isEmpty()
            && policy.detailMode == DetailHandlingMode::PreserveViaComplex;
        if (hasSelfIllum || translucentLike || preserveDetail || hints.hasFrameAnimation) {
            return TargetShader::CsgoComplex;
        }
        return TargetShader::CsgoEnvironment;
    }

    if (shader == QStringLiteral("vertexlitgeneric") || shader == QStringLiteral("unlitgeneric")) {
        return TargetShader::CsgoComplex;
    }

    if (shader == QStringLiteral("worldvertextransition")
        || shader == QStringLiteral("worldtwotextureblend")) {
        return TargetShader::CsgoEnvironmentBlend;
    }

    if (shader == QStringLiteral("lightmapped_4wayblend")) {
        return TargetShader::CsgoLightmapped4WayBlend;
    }

    if (shader == QStringLiteral("water")) {
        return TargetShader::CsgoWaterFancy;
    }

    if (shader == QStringLiteral("decalmodulate")) {
        return TargetShader::CsgoStaticOverlay;
    }

    if (shader == QStringLiteral("modulate")) {
        if (vmt.boolParam(QStringLiteral("$mod2x")) || vmt.boolParam(QStringLiteral("$multiply"))) {
            return TargetShader::CsgoStaticOverlay;
        }
        return TargetShader::CsgoComplex;
    }

    if (shader == QStringLiteral("unlittwotexture")) {
        if (hints.hasDualAsyncScroll) {
            return TargetShader::CsgoEffects;
        }
        return TargetShader::CsgoComplex;
    }

    if (shader == QStringLiteral("refract")) {
        if (!vmt.stringParam(QStringLiteral("$envmap")).isEmpty() || isGlassSurfaceProp(vmt)) {
            return TargetShader::CsgoGlass;
        }
        return TargetShader::CsgoRefract;
    }

    if (shader == QStringLiteral("shatteredglass") || shader == QStringLiteral("windowimposter")) {
        return TargetShader::CsgoGlass;
    }

    if (shader == QStringLiteral("splinerope") || shader == QStringLiteral("cable")) {
        return vmt.boolParam(QStringLiteral("$alphatest"))
            ? TargetShader::CsgoComplex
            : TargetShader::Cables;
    }

    if (shader == QStringLiteral("grass")) {
        return TargetShader::CsgoFoliage;
    }

    if (shader == QStringLiteral("character")) {
        return TargetShader::CsgoCharacter;
    }

    if (shader == QStringLiteral("eyes") || shader == QStringLiteral("eyerefract")) {
        return TargetShader::CsgoEyeball;
    }

    if (shader == QStringLiteral("black")) {
        return TargetShader::CsgoBlackUnlit;
    }

    if (shader == QStringLiteral("lightmappedreflective")) {
        const QString sp = vmt.stringParam(QStringLiteral("$surfaceprop")).trimmed().toLower();
        return (sp == QStringLiteral("water"))
            ? TargetShader::CsgoWaterFancy
            : TargetShader::CsgoEnvironment;
    }

    if (shader == QStringLiteral("solidenergy")
        || shader == QStringLiteral("monitorscreen")
        || shader == QStringLiteral("decalbasetimeslightmapalphablendselfillum")
        || shader == QStringLiteral("teeth")
        || shader == QStringLiteral("basetimesmod2xenvmap")) {
        return TargetShader::CsgoComplex;
    }

    if (shader == QStringLiteral("aftershock") || shader == QStringLiteral("cloak")) {
        return TargetShader::CsgoRefract;
    }

    warnings.append(QCoreApplication::translate(
        "MaterialTranslator",
        "Unrecognized Source 1 shader '%1'; falling back to csgo_complex.vfx.")
                        .arg(vmt.rawShaderName.isEmpty() ? shader : vmt.rawShaderName));
    return TargetShader::CsgoComplex;
}

} // namespace

MaterialPlan MaterialTranslator::plan(
    const VmtMaterial& vmt,
    const QHash<QString, VtfProbeInfo>& vtfProbes,
    const MaterialTranslatePolicy& policy,
    MaterialUsage usage)
{
    MaterialPlan result;
    const ProxyRoutingHints hints = MaterialProxyCompiler::analyzeRoutingHints(vmt);
    result.targetShader = routeTargetShader(vmt, hints, policy, usage, result.warnings);
    result.targetShaderFileName = targetShaderFileName(result.targetShader);

    if (result.targetShader == TargetShader::Skip) {
        return result;
    }

    const QString baseVtf = vmt.normalizedTexturePath(QStringLiteral("$basetexture"));
    const QString bumpVtf = vmt.normalizedTexturePath(QStringLiteral("$bumpmap"));
    const QString normalVtf = vmt.normalizedTexturePath(QStringLiteral("$normalmap"));
    const bool isSsbump = vmt.boolParam(QStringLiteral("$ssbump"));
    const TextureExportFlavor normalFlavor = isSsbump
        ? TextureExportFlavor::SsbumpToNormal
        : TextureExportFlavor::NormalDirectX;

    const bool shaderSupportsAtlas = (result.targetShader == TargetShader::CsgoComplex
        || result.targetShader == TargetShader::CsgoStaticOverlay);
    const bool useFrameAtlas = shaderSupportsAtlas
        && hints.hasFrameAnimation
        && !baseVtf.isEmpty()
        && probeFrameCount(vtfProbes, baseVtf) > 1;
    result.needsFrameAtlas = useFrameAtlas;

    switch (result.targetShader) {
    case TargetShader::ExplicitVtexOnly: {
        result.needsExplicitVtex = true;
        const bool hasAlpha = probeHasAlpha(vtfProbes, baseVtf);
        result.vtexPreset = hasAlpha ? VtexPreset::Sprite2DAlpha : VtexPreset::Sprite2DOpaque;
        addExport(
            result,
            QStringLiteral("SpriteBase"),
            baseVtf,
            hasAlpha ? TextureExportFlavor::StraightRgba : TextureExportFlavor::OpaqueColor);
        return result;
    }

    case TargetShader::Sky: {
        QString skyVtf = vmt.normalizedTexturePath(QStringLiteral("$hdrcompressedtexture"));
        if (skyVtf.isEmpty()) {
            skyVtf = vmt.normalizedTexturePath(QStringLiteral("$hdrbasetexture"));
        }
        if (skyVtf.isEmpty()) {
            skyVtf = baseVtf;
        }
        addExport(result, QStringLiteral("SkyTexture"), skyVtf, TextureExportFlavor::OpaqueColor);
        return result;
    }

    case TargetShader::CsgoBlackUnlit:
        return result;

    case TargetShader::CsgoEnvironment: {
        addExport(result, QStringLiteral("TextureColor1"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!bumpVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal1"), bumpVtf, normalFlavor, QStringLiteral("_normal"));
        }
        const QString sp1 = layerSurfaceProp(vmt, 1);
        (void)planLayerRoughnessAndMetalness(
            vmt, 1,
            QStringLiteral("TextureRoughness1"),
            QStringLiteral("TextureMetalness1"),
            baseVtf, bumpVtf, sp1, true, result);

        if (vmt.boolParam(QStringLiteral("$alphatest")) && !baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            addExport(result, QStringLiteral("TextureTranslucency1"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_trans"));
        }
        if (vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !baseVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureTintMask1"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_tintmask"));
        }

        const auto baseXf = vmt.transformParam(QStringLiteral("$basetexturetransform"));
        const auto bumpXf = vmt.transformParam(QStringLiteral("$bumptransform"));
        if (bumpXf.has_value() && !bumpXf->isIdentity()) {
            const std::array<float, 2> baseScale = baseXf.has_value() ? baseXf->scale : std::array<float, 2>{1.0f, 1.0f};
            if (std::abs(bumpXf->scale[0] - baseScale[0]) > 1e-3f
                || std::abs(bumpXf->scale[1] - baseScale[1]) > 1e-3f) {
                result.warnings.append(QCoreApplication::translate(
                    "MaterialTranslator",
                    "$bumptransform scale differs from $basetexturetransform and is not independently supported by csgo_environment.vfx."));
            }
        }
        return result;
    }

    case TargetShader::CsgoEnvironmentBlend: {
        const QString baseVtf2 = vmt.normalizedTexturePath(QStringLiteral("$basetexture2"));
        const QString bumpVtf2 = vmt.normalizedTexturePath(QStringLiteral("$bumpmap2"));
        const QString blendModVtf = vmt.normalizedTexturePath(QStringLiteral("$blendmodulatetexture"));

        const auto layerTint1 = vmt.colorParam(QStringLiteral("$layertint1"));
        const auto layerTint2 = vmt.colorParam(QStringLiteral("$layertint2"));
        const bool bakeSeparateTints = (layerTint1.has_value() || layerTint2.has_value())
            && !(layerTint1.has_value() && layerTint2.has_value() && colorsApproximatelyEqual(*layerTint1, *layerTint2));

        if (!baseVtf.isEmpty()) {
            TextureExportItem item;
            item.role = QStringLiteral("TextureColor1");
            item.sourceVtfPath = baseVtf;
            if (bakeSeparateTints && layerTint1.has_value()) {
                item.flavor = TextureExportFlavor::LinearTintedColor;
                item.linearTint = colorToLinear(*layerTint1);
                item.targetRelativePngPath = makeTargetPngPath(baseVtf, QStringLiteral("_tinted1"));
            } else {
                item.flavor = TextureExportFlavor::OpaqueColor;
                item.targetRelativePngPath = makeTargetPngPath(baseVtf);
            }
            result.textureExports.push_back(std::move(item));
        }

        if (!baseVtf2.isEmpty()) {
            TextureExportItem item;
            item.role = QStringLiteral("TextureColor2");
            item.sourceVtfPath = baseVtf2;
            if (bakeSeparateTints && layerTint2.has_value()) {
                item.flavor = TextureExportFlavor::LinearTintedColor;
                item.linearTint = colorToLinear(*layerTint2);
                item.targetRelativePngPath = makeTargetPngPath(baseVtf2, QStringLiteral("_tinted2"));
            } else {
                item.flavor = TextureExportFlavor::OpaqueColor;
                item.targetRelativePngPath = makeTargetPngPath(baseVtf2);
            }
            result.textureExports.push_back(std::move(item));
        }

        if (!bumpVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal1"), bumpVtf, normalFlavor, QStringLiteral("_normal"));
        }
        if (!bumpVtf2.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal2"), bumpVtf2, normalFlavor, QStringLiteral("_normal"));
        }
        if (!blendModVtf.isEmpty()) {
            addExport(
                result,
                QStringLiteral("TextureHeight2"),
                blendModVtf,
                TextureExportFlavor::ChannelMask,
                QStringLiteral("_height2"),
                TextureChannel::Green);
        }

        (void)planLayerRoughnessAndMetalness(
            vmt, 1,
            QStringLiteral("TextureRoughness1"),
            QStringLiteral("TextureMetalness1"),
            baseVtf, bumpVtf, layerSurfaceProp(vmt, 1), true, result);
        (void)planLayerRoughnessAndMetalness(
            vmt, 2,
            QStringLiteral("TextureRoughness2"),
            QStringLiteral("TextureMetalness2"),
            baseVtf2, bumpVtf2, layerSurfaceProp(vmt, 2), true, result);
        return result;
    }

    case TargetShader::CsgoLightmapped4WayBlend: {
        for (int layer = 1; layer <= 4; ++layer) {
            const QString baseKey = (layer == 1) ? QStringLiteral("$basetexture") : QStringLiteral("$basetexture%1").arg(layer);
            const QString bumpKey = (layer == 1) ? QStringLiteral("$bumpmap") : QStringLiteral("$bumpmap%1").arg(layer);
            const QString lBase = vmt.normalizedTexturePath(baseKey);
            const QString lBump = vmt.normalizedTexturePath(bumpKey);

            addExport(result, QStringLiteral("TextureLayer%1Color").arg(layer), lBase, TextureExportFlavor::OpaqueColor);
            if (!lBump.isEmpty()) {
                addExport(result, QStringLiteral("TextureLayer%1Normal").arg(layer), lBump, normalFlavor, QStringLiteral("_normal"));
            }
            (void)planLayerRoughnessAndMetalness(
                vmt, layer,
                QStringLiteral("TextureLayer%1Roughness").arg(layer),
                {},
                lBase, lBump, layerSurfaceProp(vmt, layer), false, result);
        }
        const QString detailVtf = vmt.normalizedTexturePath(QStringLiteral("$detail"));
        if (!detailVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureDetail"), detailVtf, TextureExportFlavor::LinearPassThrough);
        }
        return result;
    }

    case TargetShader::CsgoComplex: {
        const ComplexFeatures features = resolveComplexFeatures(vmt);
        const QString tex2Vtf = vmt.normalizedTexturePath(QStringLiteral("$texture2"));
        const bool isTwoTextureCombine = !tex2Vtf.isEmpty()
            && (vmt.normalizedShaderName == QStringLiteral("unlittwotexture")
                || vmt.normalizedShaderName == QStringLiteral("modulate"));

        if (useFrameAtlas) {
            addExport(
                result,
                QStringLiteral("TextureColor"),
                baseVtf,
                TextureExportFlavor::AnimatedColorAtlas,
                QStringLiteral("_atlas"));
        } else if (isTwoTextureCombine && !baseVtf.isEmpty()) {
            TextureExportItem combineItem;
            combineItem.role = QStringLiteral("TextureColor");
            combineItem.sourceVtfPath = baseVtf;
            combineItem.secondaryVtfPath = tex2Vtf;
            combineItem.flavor = TextureExportFlavor::TwoTextureCombine;
            combineItem.secondaryTransform = vmt.transformParam(QStringLiteral("$texture2transform"));
            combineItem.targetRelativePngPath = makeTargetPngPath(baseVtf, QStringLiteral("_combined"));
            result.textureExports.push_back(std::move(combineItem));
        } else {
            addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        }

        if (!features.isUnlitMode) {
            if (!bumpVtf.isEmpty()) {
                addExport(result, QStringLiteral("TextureNormal"), bumpVtf, normalFlavor, QStringLiteral("_normal"));
            }
            (void)planLayerRoughnessAndMetalness(
                vmt, 1,
                QStringLiteral("TextureRoughness"),
                QStringLiteral("TextureMetalness"),
                baseVtf, bumpVtf, layerSurfaceProp(vmt, 1), !features.additiveBlend, result);
        }

        const bool needsAlpha = features.alphaTest || features.translucent || hints.hasDynamicAlpha;
        if (needsAlpha && !baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            if (useFrameAtlas) {
                addExport(
                    result,
                    QStringLiteral("TextureTranslucency"),
                    baseVtf,
                    TextureExportFlavor::AnimatedAlphaAtlas,
                    QStringLiteral("_atlas_trans"));
            } else {
                addExport(
                    result,
                    QStringLiteral("TextureTranslucency"),
                    baseVtf,
                    TextureExportFlavor::AlphaMask,
                    QStringLiteral("_trans"));
            }
        }

        if (features.selfIllum && !features.isUnlitMode) {
            const QString selfIllumMaskVtf = vmt.normalizedTexturePath(QStringLiteral("$selfillummask"));
            if (!selfIllumMaskVtf.isEmpty()) {
                addExport(result, QStringLiteral("TextureSelfIllumMask"), selfIllumMaskVtf, TextureExportFlavor::LuminanceMask, QStringLiteral("_selfillum"));
            } else if (!baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
                addExport(result, QStringLiteral("TextureSelfIllumMask"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_selfillum"));
            }
        }

        if (features.detailTexture) {
            const QString detailVtf = vmt.normalizedTexturePath(QStringLiteral("$detail"));
            addExport(result, QStringLiteral("TextureDetail"), detailVtf, TextureExportFlavor::LinearPassThrough);
        }

        if (!vmt.boolParam(QStringLiteral("$notint")) && vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !baseVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureTintMask"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_tintmask"));
        }
        return result;
    }

    case TargetShader::CsgoStaticOverlay: {
        const StaticOverlayMode mode = resolveStaticOverlayMode(vmt);
        const QString tex2Vtf = vmt.normalizedTexturePath(QStringLiteral("$texture2"));
        const bool isTwoTextureCombine = !tex2Vtf.isEmpty()
            && vmt.normalizedShaderName == QStringLiteral("modulate");

        if (useFrameAtlas) {
            addExport(
                result,
                QStringLiteral("TextureColor"),
                baseVtf,
                TextureExportFlavor::AnimatedColorAtlas,
                QStringLiteral("_atlas"));
        } else if (isTwoTextureCombine && !baseVtf.isEmpty()) {
            TextureExportItem combineItem;
            combineItem.role = QStringLiteral("TextureColor");
            combineItem.sourceVtfPath = baseVtf;
            combineItem.secondaryVtfPath = tex2Vtf;
            combineItem.flavor = TextureExportFlavor::TwoTextureCombine;
            combineItem.secondaryTransform = vmt.transformParam(QStringLiteral("$texture2transform"));
            combineItem.targetRelativePngPath = makeTargetPngPath(baseVtf, QStringLiteral("_combined"));
            result.textureExports.push_back(std::move(combineItem));
        } else {
            addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        }

        const bool needsAlpha = (mode.blendMode == 1 || mode.blendMode == 2 || vmt.boolParam(QStringLiteral("$translucent")));
        if (needsAlpha && !baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            if (useFrameAtlas) {
                addExport(
                    result,
                    QStringLiteral("TextureTranslucency"),
                    baseVtf,
                    TextureExportFlavor::AnimatedAlphaAtlas,
                    QStringLiteral("_atlas_trans"));
            } else {
                addExport(
                    result,
                    QStringLiteral("TextureTranslucency"),
                    baseVtf,
                    TextureExportFlavor::AlphaMask,
                    QStringLiteral("_trans"));
            }
        }

        if (mode.lit) {
            if (!bumpVtf.isEmpty()) {
                addExport(result, QStringLiteral("TextureNormal"), bumpVtf, normalFlavor, QStringLiteral("_normal"));
            }
            (void)planLayerRoughnessAndMetalness(
                vmt, 1,
                QStringLiteral("TextureRoughness"),
                {},
                baseVtf, bumpVtf, layerSurfaceProp(vmt, 1), false, result);
        }

        if (vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !baseVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureTintMask"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_tintmask"));
        }
        return result;
    }

    case TargetShader::CsgoGlass: {
        addExport(result, QStringLiteral("GlassTintColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            addExport(result, QStringLiteral("GlassMaskTranslucency"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_trans"));
        }
        const QString glassNormal = !normalVtf.isEmpty() ? normalVtf : bumpVtf;
        if (!glassNormal.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal"), glassNormal, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        (void)planLayerRoughnessAndMetalness(
            vmt, 1,
            QStringLiteral("TextureRoughness"),
            {},
            baseVtf, glassNormal, layerSurfaceProp(vmt, 1), false, result);
        return result;
    }

    case TargetShader::CsgoWaterFancy: {
        const QString waterNormal = !normalVtf.isEmpty() ? normalVtf : bumpVtf;
        if (!waterNormal.isEmpty()) {
            addExport(result, QStringLiteral("TextureWavesNormal"), waterNormal, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        return result;
    }

    case TargetShader::CsgoRefract: {
        if (!normalVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal"), normalVtf, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        const QString normal2Vtf = vmt.normalizedTexturePath(QStringLiteral("$normalmap2"));
        if (!normal2Vtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal2"), normal2Vtf, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        const QString tintTexVtf = vmt.normalizedTexturePath(QStringLiteral("$refracttinttexture"));
        if (!tintTexVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureTintTexture"), tintTexVtf, TextureExportFlavor::LinearPassThrough);
        }
        return result;
    }

    case TargetShader::CsgoEffects: {
        addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            addExport(result, QStringLiteral("TextureTranslucency"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_trans"));
        }
        const QString tex2Vtf = vmt.normalizedTexturePath(QStringLiteral("$texture2"));
        if (!tex2Vtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureMask1"), tex2Vtf, TextureExportFlavor::OpaqueColor);
        }
        return result;
    }

    case TargetShader::Cables: {
        addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!bumpVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal"), bumpVtf, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        return result;
    }

    case TargetShader::CsgoFoliage: {
        addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            addExport(result, QStringLiteral("TextureTranslucency"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_trans"));
        }
        return result;
    }

    case TargetShader::CsgoCharacter: {
        addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        if (!bumpVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureNormal"), bumpVtf, TextureExportFlavor::NormalDirectX, QStringLiteral("_normal"));
        }
        const QString masks1Vtf = vmt.normalizedTexturePath(QStringLiteral("$masks1"));
        if (!masks1Vtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureRoughness"), masks1Vtf, TextureExportFlavor::ChannelMask, QStringLiteral("_rough"), TextureChannel::Green);
            addExport(result, QStringLiteral("TextureMetalness"), masks1Vtf, TextureExportFlavor::ChannelMask, QStringLiteral("_metal"), TextureChannel::Blue);
        }
        const bool alphaTest = vmt.boolParam(QStringLiteral("$alphatest"));
        const bool translucent = !alphaTest && vmt.boolParam(QStringLiteral("$translucent"));
        if ((alphaTest || translucent) && !baseVtf.isEmpty() && probeHasAlpha(vtfProbes, baseVtf)) {
            addExport(result, QStringLiteral("TextureTranslucency"), baseVtf, TextureExportFlavor::AlphaMask, QStringLiteral("_trans"));
        }
        return result;
    }

    case TargetShader::CsgoEyeball: {
        addExport(result, QStringLiteral("TextureColor"), baseVtf, TextureExportFlavor::OpaqueColor);
        const QString irisVtf = vmt.normalizedTexturePath(QStringLiteral("$iris"));
        if (!irisVtf.isEmpty()) {
            addExport(result, QStringLiteral("TextureIrisMask1"), irisVtf, TextureExportFlavor::LinearPassThrough);
        }
        return result;
    }

    case TargetShader::Skip:
        break;
    }

    return result;
}

Core::Result<VmatBuildOutput> MaterialTranslator::buildVmat(
    const VmtMaterial& vmt,
    const MaterialPlan& plan,
    const QHash<QString, QString>& exportedPaths,
    const MaterialTranslatePolicy& /*policy*/,
    const std::optional<TextureAtlasFrameInfo>& atlasInfo)
{
    VmatBuildOutput output;
    output.targetShader = plan.targetShader;
    output.warnings = plan.warnings;

    if (plan.targetShader == TargetShader::Skip) {
        return Core::Result<VmatBuildOutput>::skipped(
            QCoreApplication::translate("MaterialTranslator", "Material is marked as Skip; no .vmat generated."),
            std::move(output));
    }

    if (plan.targetShader == TargetShader::ExplicitVtexOnly) {
        return Core::Result<VmatBuildOutput>::skipped(
            QCoreApplication::translate("MaterialTranslator", "Material targets an explicit .vtex asset; no .vmat generated."),
            std::move(output));
    }

    const QString shaderFile = !plan.targetShaderFileName.isEmpty()
        ? plan.targetShaderFileName
        : targetShaderFileName(plan.targetShader);
    if (shaderFile.isEmpty()) {
        return Core::Result<VmatBuildOutput>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialTranslator", "Target shader filename is empty."));
    }

    OrderedProperties props;
    props.set(QStringLiteral("shader"), shaderFile);

    const QString sp1 = layerSurfaceProp(vmt, 1);
    int systemAttrMaxLayers = 1;
    bool isWaterAttributes = false;
    bool emitAttributesBlocks = true;

    switch (plan.targetShader) {
    case TargetShader::CsgoEnvironment: {
        const bool alphaTest = vmt.boolParam(QStringLiteral("$alphatest"));
        if (alphaTest) {
            props.set(QStringLiteral("F_ALPHA_TEST"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor1"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor1"), colorPath);
        }
        const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal1"));
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("TextureNormal1"), normalPath);
        }
        props.set(
            QStringLiteral("TextureRoughness1"),
            resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness1"), sp1, exportedPaths));

        const QString metalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureMetalness1"));
        if (!metalPath.isEmpty()) {
            props.set(QStringLiteral("TextureMetalness1"), metalPath);
        }
        props.set(QStringLiteral("TextureAmbientOcclusion1"), QString::fromLatin1(kDefaultAoTexture));

        if (alphaTest) {
            const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency1"));
            if (!transPath.isEmpty()) {
                props.set(QStringLiteral("TextureTranslucency1"), transPath);
            }
            props.set(
                QStringLiteral("g_flAlphaTestReference"),
                formatFloat(vmt.floatParam(QStringLiteral("$alphatestreference"), 0.5f)));
            props.set(QStringLiteral("g_flAntiAliasedEdgeStrength"), QStringLiteral("1.000000"));
        }

        if (vmt.boolParam(QStringLiteral("$blendtintbybasealpha"))) {
            const QString tintMaskPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTintMask1"));
            if (!tintMaskPath.isEmpty()) {
                props.set(QStringLiteral("TextureTintMask1"), tintMaskPath);
            }
        }

        if (const auto tint = combinedTintSrgb(
                vmt.colorParam(QStringLiteral("$color")),
                vmt.colorParam(QStringLiteral("$color2")));
            tint.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4((*tint)[0], (*tint)[1], (*tint)[2], 0.0f));
        }

        if (vmt.hasParam(QStringLiteral("$seamless_scale"))) {
            const float seamless = vmt.floatParam(QStringLiteral("$seamless_scale"), 1.0f);
            props.set(QStringLiteral("g_nUVSet1"), QStringLiteral("0"));
            props.set(QStringLiteral("g_vTexCoordScale1"), formatVec2(seamless, seamless));
        } else {
            writeUvTransform(props, vmt.transformParam(QStringLiteral("$basetexturetransform")), QStringLiteral("1"));
        }
        break;
    }

    case TargetShader::CsgoEnvironmentBlend: {
        systemAttrMaxLayers = 2;
        if (vmt.boolParam(QStringLiteral("$newlayerblending"))) {
            props.set(QStringLiteral("F_USE_NEW_BLENDING"), QStringLiteral("1"));
        }
        const float borderStrength = vmt.floatParam(QStringLiteral("$layerborderstrength"), 0.0f);
        if (borderStrength > 0.0f) {
            props.set(QStringLiteral("F_BLEND_EFFECTS_2"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        const QString color1 = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor1"));
        const QString color2 = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor2"));
        if (!color1.isEmpty()) {
            props.set(QStringLiteral("TextureColor1"), color1);
        }
        if (!color2.isEmpty()) {
            props.set(QStringLiteral("TextureColor2"), color2);
        }

        const QString normal1 = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal1"));
        const QString normal2 = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal2"));
        if (!normal1.isEmpty()) {
            props.set(QStringLiteral("TextureNormal1"), normal1);
        }
        if (!normal2.isEmpty()) {
            props.set(QStringLiteral("TextureNormal2"), normal2);
        }

        const QString height2 = resolveExportedPath(exportedPaths, QStringLiteral("TextureHeight2"));
        if (!height2.isEmpty()) {
            props.set(QStringLiteral("TextureHeight2"), height2);
        }

        const QString sp2 = layerSurfaceProp(vmt, 2);
        props.set(
            QStringLiteral("TextureRoughness1"),
            resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness1"), sp1, exportedPaths));
        props.set(
            QStringLiteral("TextureRoughness2"),
            resolveRoughnessValue(vmt, 2, QStringLiteral("TextureRoughness2"), sp2, exportedPaths));

        const QString metal1 = resolveExportedPath(exportedPaths, QStringLiteral("TextureMetalness1"));
        const QString metal2 = resolveExportedPath(exportedPaths, QStringLiteral("TextureMetalness2"));
        if (!metal1.isEmpty()) {
            props.set(QStringLiteral("TextureMetalness1"), metal1);
        }
        if (!metal2.isEmpty()) {
            props.set(QStringLiteral("TextureMetalness2"), metal2);
        }

        props.set(QStringLiteral("TextureAmbientOcclusion1"), QString::fromLatin1(kDefaultAoTexture));
        props.set(QStringLiteral("TextureAmbientOcclusion2"), QString::fromLatin1(kDefaultAoTexture));

        if (vmt.hasParam(QStringLiteral("$blendsoftness"))) {
            props.set(
                QStringLiteral("g_flBlendSoftness2"),
                formatFloat(vmt.floatParam(QStringLiteral("$blendsoftness"), 0.5f)));
        }

        if (borderStrength > 0.0f) {
            if (vmt.hasParam(QStringLiteral("$layerbordersoftness"))) {
                props.set(
                    QStringLiteral("g_flBorderSoftness2"),
                    formatFloat(vmt.floatParam(QStringLiteral("$layerbordersoftness"), 0.5f)));
            }
            if (vmt.hasParam(QStringLiteral("$layerborderoffset"))) {
                props.set(
                    QStringLiteral("g_flBorderOffset2"),
                    formatFloat(vmt.floatParam(QStringLiteral("$layerborderoffset"), 0.0f)));
            }
            if (const auto borderTint = vmt.colorParam(QStringLiteral("$layerbordertint")); borderTint.has_value()) {
                props.set(
                    QStringLiteral("g_vBorderTint2"),
                    formatVec4(borderTint->r, borderTint->g, borderTint->b, 0.0f));
            }
        }

        const auto colorParam = vmt.colorParam(QStringLiteral("$color"));
        const auto layerTint1 = vmt.colorParam(QStringLiteral("$layertint1"));
        const auto layerTint2 = vmt.colorParam(QStringLiteral("$layertint2"));
        std::optional<VmtColor> equalLayerTint;
        if (layerTint1.has_value() && layerTint2.has_value() && colorsApproximatelyEqual(*layerTint1, *layerTint2)) {
            equalLayerTint = layerTint1;
        }
        if (const auto tint = combinedTintSrgb(colorParam, equalLayerTint); tint.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4((*tint)[0], (*tint)[1], (*tint)[2], 0.0f));
        }

        writeUvTransform(props, vmt.transformParam(QStringLiteral("$basetexturetransform")), QStringLiteral("1"));
        writeUvTransform(props, vmt.transformParam(QStringLiteral("$basetexturetransform2")), QStringLiteral("2"));
        break;
    }

    case TargetShader::CsgoLightmapped4WayBlend: {
        systemAttrMaxLayers = 4;
        const bool hasSpecIndirect = !vmt.stringParam(QStringLiteral("$envmap")).isEmpty()
            || !vmt.normalizedTexturePath(QStringLiteral("$envmapmask")).isEmpty()
            || vmt.boolParam(QStringLiteral("$basealphaenvmapmask"))
            || vmt.boolParam(QStringLiteral("$basemapalphaphongmask"));
        if (hasSpecIndirect) {
            props.set(QStringLiteral("F_SPECULAR_INDIRECT"), QStringLiteral("1"));
        }
        if (vmt.boolParam(QStringLiteral("$phong"))) {
            props.set(QStringLiteral("F_SPECULAR_DIRECT"), QStringLiteral("1"));
        }
        const QString detailPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureDetail"));
        const bool hasDetail = !vmt.normalizedTexturePath(QStringLiteral("$detail")).isEmpty();
        if (hasDetail) {
            props.set(QStringLiteral("F_DETAILTEXTURE"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        for (int layer = 1; layer <= 4; ++layer) {
            const QString colorKey = QStringLiteral("TextureLayer%1Color").arg(layer);
            const QString normalKey = QStringLiteral("TextureLayer%1Normal").arg(layer);
            const QString roughKey = QStringLiteral("TextureLayer%1Roughness").arg(layer);
            const QString aoKey = QStringLiteral("TextureLayer%1AmbientOcclusion").arg(layer);

            const QString colorPath = resolveExportedPath(exportedPaths, colorKey);
            if (!colorPath.isEmpty()) {
                props.set(colorKey, colorPath);
            }
            const QString normalPath = resolveExportedPath(exportedPaths, normalKey);
            if (!normalPath.isEmpty()) {
                props.set(normalKey, normalPath);
            }
            props.set(
                roughKey,
                resolveRoughnessValue(vmt, layer, roughKey, layerSurfaceProp(vmt, layer), exportedPaths));
            props.set(aoKey, QString::fromLatin1(kDefaultAoTexture));
        }

        if (vmt.hasParam(QStringLiteral("$texture2_blendstart"))
            || vmt.hasParam(QStringLiteral("$texture3_blendstart"))
            || vmt.hasParam(QStringLiteral("$texture4_blendstart"))) {
            props.set(
                QStringLiteral("g_vBlendStart"),
                formatVec4(
                    0.0f,
                    vmt.floatParam(QStringLiteral("$texture2_blendstart"), 0.0f),
                    vmt.floatParam(QStringLiteral("$texture3_blendstart"), 0.0f),
                    vmt.floatParam(QStringLiteral("$texture4_blendstart"), 0.0f)));
        }
        if (vmt.hasParam(QStringLiteral("$texture2_blendend"))
            || vmt.hasParam(QStringLiteral("$texture3_blendend"))
            || vmt.hasParam(QStringLiteral("$texture4_blendend"))) {
            props.set(
                QStringLiteral("g_vBlendEnd"),
                formatVec4(
                    1.0f,
                    vmt.floatParam(QStringLiteral("$texture2_blendend"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture3_blendend"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture4_blendend"), 1.0f)));
        }
        if (vmt.hasParam(QStringLiteral("$texture2_lumstart"))
            || vmt.hasParam(QStringLiteral("$texture3_lumstart"))
            || vmt.hasParam(QStringLiteral("$texture4_lumstart"))) {
            props.set(
                QStringLiteral("g_vLumStart"),
                formatVec4(
                    0.0f,
                    vmt.floatParam(QStringLiteral("$texture2_lumstart"), 0.0f),
                    vmt.floatParam(QStringLiteral("$texture3_lumstart"), 0.0f),
                    vmt.floatParam(QStringLiteral("$texture4_lumstart"), 0.0f)));
        }
        if (vmt.hasParam(QStringLiteral("$texture2_lumend"))
            || vmt.hasParam(QStringLiteral("$texture3_lumend"))
            || vmt.hasParam(QStringLiteral("$texture4_lumend"))) {
            props.set(
                QStringLiteral("g_vLumEnd"),
                formatVec4(
                    1.0f,
                    vmt.floatParam(QStringLiteral("$texture2_lumend"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture3_lumend"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture4_lumend"), 1.0f)));
        }
        if (vmt.hasParam(QStringLiteral("$lumblendfactor2"))
            || vmt.hasParam(QStringLiteral("$lumblendfactor3"))
            || vmt.hasParam(QStringLiteral("$lumblendfactor4"))) {
            props.set(
                QStringLiteral("g_vLumBlends"),
                formatVec4(
                    0.0f,
                    vmt.floatParam(QStringLiteral("$lumblendfactor2"), 0.0f),
                    vmt.floatParam(QStringLiteral("$lumblendfactor3"), 0.0f),
                    vmt.floatParam(QStringLiteral("$lumblendfactor4"), 0.0f)));
        }
        if (vmt.hasParam(QStringLiteral("$texture2_uvscale"))
            || vmt.hasParam(QStringLiteral("$texture3_uvscale"))
            || vmt.hasParam(QStringLiteral("$texture4_uvscale"))) {
            props.set(
                QStringLiteral("g_vUVScale"),
                formatVec4(
                    1.0f,
                    vmt.floatParam(QStringLiteral("$texture2_uvscale"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture3_uvscale"), 1.0f),
                    vmt.floatParam(QStringLiteral("$texture4_uvscale"), 1.0f)));
        }

        if (hasDetail) {
            if (!detailPath.isEmpty()) {
                props.set(QStringLiteral("TextureDetail"), detailPath);
            }
            const float dScale = vmt.floatParam(QStringLiteral("$detailscale"), 4.0f);
            props.set(QStringLiteral("g_vDetailScale"), formatVec2(dScale, dScale));
            const float defaultFactor = vmt.floatParam(QStringLiteral("$detailblendfactor"), 1.0f);
            props.set(
                QStringLiteral("g_vDetailBlendFactors"),
                formatVec4(
                    vmt.floatParam(QStringLiteral("$detailblendfactor1"), defaultFactor),
                    vmt.floatParam(QStringLiteral("$detailblendfactor2"), defaultFactor),
                    vmt.floatParam(QStringLiteral("$detailblendfactor3"), defaultFactor),
                    vmt.floatParam(QStringLiteral("$detailblendfactor4"), defaultFactor)));
        }

        if (const auto color = vmt.colorParam(QStringLiteral("$color")); color.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4(color->r, color->g, color->b, 0.0f));
        }
        break;
    }

    case TargetShader::CsgoComplex: {
        const ComplexFeatures features = resolveComplexFeatures(vmt);

        // Feature macros (statically arbitrated)
        if (features.alphaTest) {
            props.set(QStringLiteral("F_ALPHA_TEST"), QStringLiteral("1"));
        } else if (features.translucent) {
            props.set(QStringLiteral("F_TRANSLUCENT"), QStringLiteral("1"));
            if (features.additiveBlend) {
                props.set(QStringLiteral("F_ADDITIVE_BLEND"), QStringLiteral("1"));
            }
        }

        if (features.selfIllum) {
            props.set(QStringLiteral("F_SELF_ILLUM"), QStringLiteral("1"));
        }

        if (features.detailTexture) {
            const int detailBlendMode = vmt.intParam(QStringLiteral("$detailblendmode"), 0);
            props.set(
                QStringLiteral("F_DETAIL_TEXTURE"),
                detailBlendMode == 2 ? QStringLiteral("2") : QStringLiteral("1"));
        }

        const QString metalPath = (!features.isUnlitMode && !features.additiveBlend)
            ? resolveExportedPath(exportedPaths, QStringLiteral("TextureMetalness"))
            : QString();
        if (!metalPath.isEmpty()) {
            props.set(QStringLiteral("F_METALNESS_TEXTURE"), QStringLiteral("1"));
        }

        if (vmt.boolParam(QStringLiteral("$vertexcolor"))) {
            props.set(QStringLiteral("F_PAINT_VERTEX_COLORS"), QStringLiteral("1"));
        }
        if (vmt.boolParam(QStringLiteral("$forceuv2"))) {
            props.set(QStringLiteral("F_SECONDARY_UV"), QStringLiteral("1"));
        }

        const bool noTint = vmt.boolParam(QStringLiteral("$notint"));
        const QString tintMaskPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTintMask"));
        const bool enableTintMask = noTint || (vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !tintMaskPath.isEmpty());
        if (enableTintMask) {
            props.set(QStringLiteral("F_TINT_MASK"), QStringLiteral("1"));
        }

        writeCommonFlags(props, vmt, true, true, true);

        // Textures & PBR slots
        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }

        if (!features.isUnlitMode) {
            const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
            if (!normalPath.isEmpty()) {
                props.set(QStringLiteral("TextureNormal"), normalPath);
            }
        }

        props.set(
            QStringLiteral("TextureRoughness"),
            resolveRoughnessValue(
                vmt, 1, QStringLiteral("TextureRoughness"), sp1, exportedPaths, features.isUnlitMode));

        if (!metalPath.isEmpty()) {
            props.set(QStringLiteral("TextureMetalness"), metalPath);
        } else {
            props.set(QStringLiteral("g_flMetalness"), QStringLiteral("0.000000"));
        }

        if (features.alphaTest) {
            const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
            if (!transPath.isEmpty()) {
                props.set(QStringLiteral("TextureTranslucency"), transPath);
            }
            props.set(
                QStringLiteral("g_flAlphaTestReference"),
                formatFloat(vmt.floatParam(QStringLiteral("$alphatestreference"), 0.5f)));
            props.set(QStringLiteral("g_flAntiAliasedEdgeStrength"), QStringLiteral("1.000000"));
        } else if (features.translucent) {
            const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
            if (!transPath.isEmpty()) {
                props.set(QStringLiteral("TextureTranslucency"), transPath);
            }
        } else {
            // Dynamic alpha proxy may later enable F_TRANSLUCENT; preserve exported translucency mask if present.
            const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
            if (!transPath.isEmpty()) {
                props.set(QStringLiteral("TextureTranslucency"), transPath);
            }
        }

        if (vmt.hasParam(QStringLiteral("$alpha"))) {
            props.set(
                QStringLiteral("g_flOpacityScale"),
                formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$alpha"), 1.0f), 0.0f, 1.0f)));
        }

        if (features.selfIllum) {
            props.set(QStringLiteral("g_flSelfIllumAlbedoFactor"), QStringLiteral("1.000000"));
            props.set(
                QStringLiteral("g_flSelfIllumBrightness"),
                (features.isUnlitMode && features.additiveBlend)
                    ? QStringLiteral("1.200000")
                    : QStringLiteral("1.000000"));
            const QString selfIllumPath = features.isUnlitMode
                ? QString::fromLatin1(kWhiteVec4ZeroW)
                : resolveExportedPath(
                    exportedPaths,
                    QStringLiteral("TextureSelfIllumMask"),
                    QString::fromLatin1(kWhiteVec4ZeroW));
            props.set(QStringLiteral("TextureSelfIllumMask"), selfIllumPath);

            if (const auto selfTint = vmt.colorParam(QStringLiteral("$selfillumtint")); selfTint.has_value()) {
                props.set(
                    QStringLiteral("g_vSelfIllumTint"),
                    formatVec4(selfTint->r, selfTint->g, selfTint->b, 0.0f));
            }
        }

        if (features.detailTexture) {
            const QString detailPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureDetail"));
            if (!detailPath.isEmpty()) {
                props.set(QStringLiteral("TextureDetail"), detailPath);
            }
            props.set(QStringLiteral("TextureDetailMask"), QString::fromLatin1(kDefaultDetailMask));
            if (const auto dVec = vmt.vec2Param(QStringLiteral("$detailscale")); dVec.has_value()) {
                props.set(QStringLiteral("g_vDetailTexCoordScale"), formatVec2((*dVec)[0], (*dVec)[1]));
            } else {
                const float dScale = vmt.floatParam(QStringLiteral("$detailscale"), 4.0f);
                props.set(QStringLiteral("g_vDetailTexCoordScale"), formatVec2(dScale, dScale));
            }
            props.set(
                QStringLiteral("g_flDetailBlendFactor"),
                formatFloat(vmt.floatParam(QStringLiteral("$detailblendfactor"), 0.25f)));
        }

        if (enableTintMask) {
            props.set(
                QStringLiteral("TextureTintMask"),
                noTint ? QString::fromLatin1(kZeroVec4) : tintMaskPath);
        }

        if (const auto tint = combinedTintSrgb(
                vmt.colorParam(QStringLiteral("$color")),
                vmt.colorParam(QStringLiteral("$color2")));
            tint.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4((*tint)[0], (*tint)[1], (*tint)[2], 1.0f));
        }

        writeUvTransform(props, vmt.transformParam(QStringLiteral("$basetexturetransform")));
        break;
    }

    case TargetShader::CsgoStaticOverlay: {
        const StaticOverlayMode mode = resolveStaticOverlayMode(vmt);
        // FeatureRule: F_LIT 1 is strictly limited to F_BLEND_MODE 0, 1, 2.
        const bool allowLit = mode.lit && (mode.blendMode >= 0 && mode.blendMode <= 2);

        props.set(QStringLiteral("F_BLEND_MODE"), QString::number(mode.blendMode));
        props.set(QStringLiteral("F_LIT"), allowLit ? QStringLiteral("1") : QStringLiteral("0"));

        if (vmt.boolParam(QStringLiteral("$vertexcolor")) || vmt.boolParam(QStringLiteral("$vertexalpha"))) {
            props.set(QStringLiteral("F_PAINT_VERTEX_COLORS"), QStringLiteral("1"));
        }
        const QString tintMaskPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTintMask"));
        if (vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !tintMaskPath.isEmpty()) {
            props.set(QStringLiteral("F_TINT_MASK"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }
        const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
        if (!transPath.isEmpty()) {
            props.set(QStringLiteral("TextureTranslucency"), transPath);
        }

        if (allowLit) {
            const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
            if (!normalPath.isEmpty()) {
                props.set(QStringLiteral("TextureNormal"), normalPath);
            }
            props.set(
                QStringLiteral("TextureRoughness"),
                resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness"), sp1, exportedPaths));
        }

        if (mode.blendMode == 2) {
            props.set(
                QStringLiteral("g_flAlphaTestReference"),
                formatFloat(vmt.floatParam(QStringLiteral("$alphatestreference"), 0.5f)));
            props.set(QStringLiteral("g_flAntiAliasedEdgeStrength"), QStringLiteral("1.000000"));
        }

        if (vmt.hasParam(QStringLiteral("$alpha"))) {
            props.set(
                QStringLiteral("g_flOpacityScale"),
                formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$alpha"), 1.0f), 0.0f, 1.0f)));
        }

        if (vmt.boolParam(QStringLiteral("$blendtintbybasealpha")) && !tintMaskPath.isEmpty()) {
            props.set(QStringLiteral("TextureTintMask"), tintMaskPath);
        }

        if (const auto tint = combinedTintSrgb(
                vmt.colorParam(QStringLiteral("$color")),
                vmt.colorParam(QStringLiteral("$color2")));
            tint.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4((*tint)[0], (*tint)[1], (*tint)[2], 0.0f));
        }

        writeUvTransform(props, vmt.transformParam(QStringLiteral("$basetexturetransform")));
        break;
    }

    case TargetShader::CsgoGlass: {
        if (vmt.normalizedShaderName == QStringLiteral("shatteredglass")) {
            props.set(QStringLiteral("F_VERTEX_BREAK_ANIMATION"), QStringLiteral("1"));
        }
        if (vmt.boolParam(QStringLiteral("$vertexcolor"))) {
            props.set(QStringLiteral("F_PAINT_VERTEX_COLORS"), QStringLiteral("1"));
        }
        // FeatureRule: F_DISABLE_Z_BUFFERING requires F_OPAQUE_CUBEMAP_REFRACTION 1 on csgo_glass.vfx.
        writeCommonFlags(props, vmt, true, true, false);

        const QString glassTintPath = resolveExportedPath(exportedPaths, QStringLiteral("GlassTintColor"));
        if (!glassTintPath.isEmpty()) {
            props.set(QStringLiteral("GlassTintColor"), glassTintPath);
        }
        const QString glassTransPath = resolveExportedPath(exportedPaths, QStringLiteral("GlassMaskTranslucency"));
        if (!glassTransPath.isEmpty()) {
            props.set(QStringLiteral("GlassMaskTranslucency"), glassTransPath);
        }
        const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("TextureNormal"), normalPath);
        }
        props.set(
            QStringLiteral("TextureRoughness"),
            resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness"), sp1, exportedPaths));
        break;
    }

    case TargetShader::CsgoWaterFancy: {
        isWaterAttributes = true;
        const bool refractEnabled = !vmt.hasParam(QStringLiteral("$refract"))
            || vmt.boolParam(QStringLiteral("$refract"), true);
        if (refractEnabled) {
            props.set(QStringLiteral("F_REFRACTION"), QStringLiteral("1"));
            props.set(QStringLiteral("F_BLUR_REFRACTION"), QStringLiteral("1"));
        } else {
            props.set(QStringLiteral("F_REFRACTION"), QStringLiteral("0"));
        }

        int reflectionType = 2; // SSR + EnvCube
        if (vmt.boolParam(QStringLiteral("$forcecheap"))) {
            reflectionType = 0;
        } else if (vmt.boolParam(QStringLiteral("$forceenvmap"))) {
            reflectionType = 1;
        }
        props.set(QStringLiteral("F_REFLECTION_TYPE"), QString::number(reflectionType));

        const QString waveNormalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureWavesNormal"));
        if (!waveNormalPath.isEmpty()) {
            props.set(QStringLiteral("TextureWavesNormal"), waveNormalPath);
        }

        if (const auto fogColor = vmt.colorParam(QStringLiteral("$fogcolor")); fogColor.has_value()) {
            props.set(
                QStringLiteral("g_vWaterFogColor"),
                formatVec4(fogColor->r, fogColor->g, fogColor->b, 0.0f));
        }

        const bool fogEnabled = !vmt.hasParam(QStringLiteral("$fogenable"))
            || vmt.boolParam(QStringLiteral("$fogenable"), true);
        if (!fogEnabled) {
            props.set(QStringLiteral("g_flWaterFogStrength"), QStringLiteral("0.000000"));
        } else {
            const float fogStart = vmt.floatParam(QStringLiteral("$fogstart"), 0.0f);
            const float fogEnd = vmt.floatParam(QStringLiteral("$fogend"), 50.0f);
            if (vmt.hasParam(QStringLiteral("$fogend"))) {
                props.set(
                    QStringLiteral("g_flWaterMaxDepth"),
                    formatFloat(std::clamp(fogEnd, 1.0f, 100.0f)));
            }
            const float span = fogEnd - fogStart;
            const float fogStrength = (vmt.hasParam(QStringLiteral("$fogend")) && span > 1e-3f)
                ? std::clamp(50.0f / span, 0.05f, 1.0f)
                : 0.5f;
            props.set(QStringLiteral("g_flWaterFogStrength"), formatFloat(fogStrength));
        }

        if (const auto refractTint = vmt.colorParam(QStringLiteral("$refracttint")); refractTint.has_value()) {
            props.set(
                QStringLiteral("g_vWaterDecayColor"),
                formatVec4(refractTint->r, refractTint->g, refractTint->b, 0.0f));
        }
        props.set(
            QStringLiteral("g_flRefractionLimit"),
            formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$refractamount"), 0.1f), 0.0f, 1.0f)));

        if (const auto reflectTint = vmt.colorParam(QStringLiteral("$reflecttint")); reflectTint.has_value()) {
            props.set(
                QStringLiteral("g_vSimpleSkyReflectionColor"),
                formatVec4(reflectTint->r, reflectTint->g, reflectTint->b, 0.0f));
        }
        props.set(
            QStringLiteral("g_flReflectance"),
            formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$reflectamount"), 0.2f), 0.0f, 1.0f)));

        std::optional<float> wavesSpeed;
        if (const auto scroll1 = vmt.vec2Param(QStringLiteral("$scroll1")); scroll1.has_value()) {
            props.set(QStringLiteral("g_vTexCoordScrollSpeed"), formatVec2((*scroll1)[0], (*scroll1)[1]));
            wavesSpeed = std::clamp(
                std::sqrt((*scroll1)[0] * (*scroll1)[0] + (*scroll1)[1] * (*scroll1)[1]),
                -8.0f,
                8.0f);
        }
        if (vmt.hasParam(QStringLiteral("$flow_timescale"))) {
            wavesSpeed = std::clamp(vmt.floatParam(QStringLiteral("$flow_timescale"), 1.0f), -8.0f, 8.0f);
        }
        props.set(QStringLiteral("g_flWavesSpeed"), formatFloat(wavesSpeed.value_or(1.0f)));

        float waveScale = 15.0f;
        if (vmt.hasParam(QStringLiteral("$flow_normaluvscale"))) {
            waveScale = std::clamp(vmt.floatParam(QStringLiteral("$flow_normaluvscale"), 15.0f), 5.0f, 100.0f);
        } else if (vmt.hasParam(QStringLiteral("$flow_uvscale"))) {
            waveScale = std::clamp(vmt.floatParam(QStringLiteral("$flow_uvscale"), 15.0f), 5.0f, 100.0f);
        }
        props.set(QStringLiteral("g_vWaveScale"), formatVec2(waveScale, waveScale));

        props.set(
            QStringLiteral("g_flWavesNormalStrength"),
            formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$flow_bumpstrength"), 1.0f), 0.0f, 2.0f)));
        break;
    }

    case TargetShader::CsgoRefract: {
        // csgo_refract.vfx does not declare F_RENDER_BACKFACES, g_bFogEnabled, or F_DISABLE_Z_BUFFERING.
        const QString normal2Path = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal2"));
        if (!normal2Path.isEmpty()) {
            props.set(QStringLiteral("F_SECONDARY_NORMAL"), QStringLiteral("1"));
        }
        const QString tintTexPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTintTexture"));
        if (!tintTexPath.isEmpty()) {
            props.set(QStringLiteral("F_TINT_TEXTURE"), QStringLiteral("1"));
        }
        if (vmt.floatParam(QStringLiteral("$bluramount"), 0.0f) > 0.0f) {
            props.set(QStringLiteral("F_BLUR"), QStringLiteral("1"));
        }
        if (vmt.boolParam(QStringLiteral("$vertexcolor"))
            || vmt.boolParam(QStringLiteral("$vertexalpha"))
            || vmt.boolParam(QStringLiteral("$vertexcolormodulate"))) {
            props.set(QStringLiteral("F_VERTEX_COLOR"), QStringLiteral("1"));
        }

        const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("TextureNormal"), normalPath);
        }
        if (!normal2Path.isEmpty()) {
            props.set(QStringLiteral("TextureNormal2"), normal2Path);
        }
        if (!tintTexPath.isEmpty()) {
            props.set(QStringLiteral("TextureTintTexture"), tintTexPath);
        }

        props.set(
            QStringLiteral("g_flRefractScale"),
            formatFloat(vmt.floatParam(QStringLiteral("$refractamount"), 0.1f)));

        if (const auto refractTint = vmt.colorParam(QStringLiteral("$refracttint")); refractTint.has_value()) {
            props.set(
                QStringLiteral("g_vColorTint"),
                formatVec4(refractTint->r, refractTint->g, refractTint->b, 0.0f));
        }

        const auto baseXf = vmt.transformParam(QStringLiteral("$basetexturetransform"));
        if (baseXf.has_value() && !baseXf->isIdentity()) {
            writeUvTransform(props, baseXf, {}, false);
        } else if (vmt.hasParam(QStringLiteral("$scale")) || vmt.hasParam(QStringLiteral("$scaleamount"))) {
            const QString scaleKey = vmt.hasParam(QStringLiteral("$scale"))
                ? QStringLiteral("$scale")
                : QStringLiteral("$scaleamount");
            if (const auto sv = vmt.vec2Param(scaleKey); sv.has_value()) {
                props.set(QStringLiteral("g_vTexCoordScale"), formatVec2((*sv)[0], (*sv)[1]));
            } else {
                const float s = vmt.floatParam(scaleKey, 1.0f);
                props.set(QStringLiteral("g_vTexCoordScale"), formatVec2(s, s));
            }
        }
        break;
    }

    case TargetShader::CsgoEffects: {
        if (vmt.boolParam(QStringLiteral("$additive"))) {
            props.set(QStringLiteral("F_ADDITIVE_BLEND"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }
        const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
        if (!transPath.isEmpty()) {
            props.set(QStringLiteral("TextureTranslucency"), transPath);
        }
        const QString mask1Path = resolveExportedPath(exportedPaths, QStringLiteral("TextureMask1"));
        if (!mask1Path.isEmpty()) {
            props.set(QStringLiteral("TextureMask1"), mask1Path);
            if (const auto tex2Xf = vmt.transformParam(QStringLiteral("$texture2transform"));
                tex2Xf.has_value() && !tex2Xf->isIdentity()) {
                props.set(QStringLiteral("g_vMask1Scale"), formatVec2(tex2Xf->scale[0], tex2Xf->scale[1]));
            }
        }
        if (const auto color = vmt.colorParam(QStringLiteral("$color")); color.has_value()) {
            props.set(QStringLiteral("g_vColorTint"), formatVec4(color->r, color->g, color->b, 0.0f));
        }
        if (vmt.hasParam(QStringLiteral("$alpha"))) {
            props.set(
                QStringLiteral("g_flOpacityScale"),
                formatFloat(std::clamp(vmt.floatParam(QStringLiteral("$alpha"), 1.0f), 0.0f, 1.0f)));
        }
        props.set(
            QStringLiteral("g_flColorBoost"),
            formatFloat(vmt.floatParam(QStringLiteral("$overbrightfactor"), 1.0f)));
        break;
    }

    case TargetShader::Cables: {
        // cables.vfx does not declare F_RENDER_BACKFACES, g_vColorTint, or g_bFogEnabled.
        const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("F_NORMAL_MAP"), QStringLiteral("1"));
        }
        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("TextureNormal"), normalPath);
        }
        const float cableRough = MaterialTextureSynthesizer::surfacePropBaselineRoughness(sp1).value_or(1.0f);
        props.set(QStringLiteral("g_flRoughness"), formatFloat(cableRough));
        break;
    }

    case TargetShader::Sky: {
        emitAttributesBlocks = false;
        const QString skyPath = resolveExportedPath(exportedPaths, QStringLiteral("SkyTexture"));
        if (!skyPath.isEmpty()) {
            props.set(QStringLiteral("SkyTexture"), skyPath);
        }
        break;
    }

    case TargetShader::CsgoFoliage: {
        props.set(QStringLiteral("F_ALPHA_TEST"), QStringLiteral("1"));
        props.set(QStringLiteral("F_RENDER_BACKFACES"), QStringLiteral("1"));
        writeCommonFlags(props, vmt, false, true, true);

        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }
        const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
        if (!transPath.isEmpty()) {
            props.set(QStringLiteral("TextureTranslucency"), transPath);
        }
        props.set(
            QStringLiteral("g_flAlphaTestReference"),
            formatFloat(vmt.floatParam(QStringLiteral("$alphatestreference"), 0.5f)));
        props.set(
            QStringLiteral("TextureRoughness"),
            resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness"), sp1, exportedPaths));
        break;
    }

    case TargetShader::CsgoCharacter: {
        const bool alphaTest = vmt.boolParam(QStringLiteral("$alphatest"));
        const bool translucent = !alphaTest && vmt.boolParam(QStringLiteral("$translucent"));
        if (alphaTest) {
            props.set(QStringLiteral("F_ALPHA_TEST"), QStringLiteral("1"));
        } else if (translucent) {
            props.set(QStringLiteral("F_TRANSLUCENT"), QStringLiteral("1"));
        }
        if (vmt.floatParam(QStringLiteral("$anisotropyamount"), 0.0f) > 0.0f
            && !vmt.boolParam(QStringLiteral("$additive"))) {
            props.set(QStringLiteral("F_ANISOTROPIC_GLOSS"), QStringLiteral("1"));
        }
        writeCommonFlags(props, vmt, true, true, true);

        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
        }
        const QString normalPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureNormal"));
        if (!normalPath.isEmpty()) {
            props.set(QStringLiteral("TextureNormal"), normalPath);
        }
        props.set(
            QStringLiteral("TextureRoughness"),
            resolveRoughnessValue(vmt, 1, QStringLiteral("TextureRoughness"), sp1, exportedPaths));
        props.set(
            QStringLiteral("TextureMetalness"),
            resolveExportedPath(exportedPaths, QStringLiteral("TextureMetalness"), QString::fromLatin1(kZeroVec4)));

        if (alphaTest || translucent) {
            const QString transPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureTranslucency"));
            if (!transPath.isEmpty()) {
                props.set(QStringLiteral("TextureTranslucency"), transPath);
            }
        }
        break;
    }

    case TargetShader::CsgoEyeball: {
        writeCommonFlags(props, vmt, true, true, true);
        const QString colorPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureColor"));
        if (!colorPath.isEmpty()) {
            props.set(QStringLiteral("TextureColor"), colorPath);
            props.set(QStringLiteral("TextureEyeAlbedo1"), colorPath);
        }
        const QString irisPath = resolveExportedPath(exportedPaths, QStringLiteral("TextureIrisMask1"));
        if (!irisPath.isEmpty()) {
            props.set(QStringLiteral("TextureIrisMask1"), irisPath);
        }
        props.set(
            QStringLiteral("g_flEyeBallRadius1"),
            formatFloat(vmt.floatParam(QStringLiteral("$eyeballradius"), 0.6f)));
        break;
    }

    case TargetShader::CsgoBlackUnlit: {
        writeCommonFlags(props, vmt, true, true, true);
        break;
    }

    case TargetShader::ExplicitVtexOnly:
    case TargetShader::Skip:
        break;
    }

    // Compile and merge Step 6 Proxies -> featureOverrides, staticParamOverrides, and DynamicParams.
    const MaterialAnimationPlan animPlan = MaterialProxyCompiler::compile(vmt, plan.targetShader, atlasInfo);
    output.warnings.append(animPlan.warnings);
    if (animPlan.hasAnimation || !animPlan.featureOverrides.isEmpty() || !animPlan.staticParamOverrides.isEmpty()) {
        applyAnimationOverrides(props, plan.targetShader, animPlan);
    }

    Core::KeyValues::KeyValuesWriter writer(vmatWriterOptions());
    writer.startSection(QStringLiteral("Layer0"));
    props.writeTo(writer);

    if (emitAttributesBlocks) {
        writeSystemAttributes(writer, vmt, systemAttrMaxLayers);
        writeMapBuilderAttributes(writer, vmt, isWaterAttributes);
    }

    if (!animPlan.dynamicParams.isEmpty()) {
        writer.startSection(QStringLiteral("DynamicParams"));
        for (const auto& [paramKey, expr] : animPlan.dynamicParams) {
            writer.writeProperty(paramKey, expr);
        }
        writer.endSection(QStringLiteral("DynamicParams"));
    }

    writer.endSection(QStringLiteral("Layer0"));
    output.vmatContent = QString::fromUtf8(writer.finish());
    return Core::Result<VmatBuildOutput>::success(std::move(output));
}

} // namespace Domain::Material
