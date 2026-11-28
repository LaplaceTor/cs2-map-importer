#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <cmath>
#include <optional>

namespace Domain::Material {

/**
 * @brief Source 1 VMT 2D texture coordinate transform ($basetexturetransform, etc.).
 */
struct VmtTransform {
    std::array<float, 2> center = {0.5f, 0.5f};
    std::array<float, 2> scale = {1.0f, 1.0f};
    float rotateDeg = 0.0f;
    std::array<float, 2> translate = {0.0f, 0.0f};

    bool isIdentity() const noexcept {
        constexpr float kEps = 1e-5f;
        return std::abs(scale[0] - 1.0f) <= kEps
            && std::abs(scale[1] - 1.0f) <= kEps
            && std::abs(rotateDeg) <= kEps
            && std::abs(translate[0]) <= kEps
            && std::abs(translate[1]) <= kEps;
    }
};

/**
 * @brief Source 1 VMT color value normalized to [0..1] float range.
 */
struct VmtColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    bool isByteScale = false;
};

/**
 * @brief Single ordered entry inside a VMT `Proxies` block.
 */
struct VmtProxyEntry {
    QString name;
    QHash<QString, QString> params;

    QString param(const QString& key, const QString& defaultValue = {}) const {
        const auto it = params.constFind(key.toLower());
        return it != params.constEnd() ? it.value() : defaultValue;
    }

    bool hasParam(const QString& key) const {
        return params.contains(key.toLower());
    }
};

/**
 * @brief Parsed Source 1 VMT material document.
 *
 * Preserves all top-level and merged DX9+ parameters (lowercase keys, including
 * user-defined `$var` variables) and ordered `Proxies` declarations.
 */
struct VmtMaterial {
    QString relativePath;
    QString rawShaderName;
    QString normalizedShaderName;
    QHash<QString, QString> params;
    QVector<VmtProxyEntry> proxies;
    QStringList patchIncludeChain;
    QStringList warnings;

    bool hadProxies() const noexcept { return !proxies.isEmpty(); }
    bool wasPatched() const noexcept { return !patchIncludeChain.isEmpty(); }

    bool hasParam(const QString& key) const;
    QString stringParam(const QString& key, const QString& defaultValue = {}) const;
    bool boolParam(const QString& key, bool defaultValue = false) const;
    int intParam(const QString& key, int defaultValue = 0) const;
    float floatParam(const QString& key, float defaultValue = 0.0f) const;
    std::optional<VmtColor> colorParam(const QString& key) const;
    std::optional<std::array<float, 2>> vec2Param(const QString& key) const;
    std::optional<std::array<float, 3>> vec3Param(const QString& key) const;
    std::optional<VmtTransform> transformParam(const QString& key) const;
    QString normalizedTexturePath(const QString& key) const;
};

} // namespace Domain::Material
