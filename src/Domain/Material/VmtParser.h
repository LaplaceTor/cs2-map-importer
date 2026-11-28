#pragma once

#include "Core/Result/Result.h"
#include "Domain/Material/VmtDocument.h"

#include <QByteArray>
#include <QString>

#include <array>
#include <functional>
#include <optional>

namespace Domain::Material {

/**
 * @brief Callback supplying raw UTF-8 VMT bytes for a normalized `materials/.../*.vmt` include path.
 */
using VmtIncludeResolver = std::function<Core::Result<QByteArray>(const QString& normalizedVmtPath)>;

/**
 * @brief Pure in-process parser for Source 1 Valve Material Type (.vmt) files.
 *
 * Uses Core::KeyValues with the VMT lexical dialect, recursively flattens
 * `Patch` inheritance (`include` / `insert` / `replace`) when an include
 * resolver is provided, merges modern DX9+ conditional subblocks, skips legacy
 * DX6/DX7/DX8 subblocks, preserves all top-level parameters including custom
 * `$var` variables, and extracts `Proxies` entries in declaration order.
 */
class VmtParser {
public:
    static Core::Result<VmtMaterial> parse(
        const QByteArray& utf8Content,
        const QString& relativeVmtPath = {},
        const VmtIncludeResolver& includeResolver = {});
    static Core::Result<VmtMaterial> parseString(
        const QString& content,
        const QString& relativeVmtPath = {},
        const VmtIncludeResolver& includeResolver = {});

    static QString normalizeShaderName(const QString& rawShaderName);
    static QString normalizeVmtPath(const QString& rawPath);
    static std::optional<VmtColor> parseColor(const QString& rawValue);
    static std::optional<std::array<float, 2>> parseVec2(const QString& rawValue);
    static std::optional<std::array<float, 3>> parseVec3(const QString& rawValue);
    static std::optional<VmtTransform> parseTransform(const QString& rawValue);
    static QString normalizeTexturePath(const QString& rawPath);
};

} // namespace Domain::Material
