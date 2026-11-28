#pragma once

#include "Core/Result/Result.h"
#include "Domain/Material/VmtDocument.h"

#include <QByteArray>
#include <QString>

#include <array>
#include <optional>

namespace Domain::Material {

/**
 * @brief Pure in-process parser for Source 1 Valve Material Type (.vmt) files.
 *
 * Uses Core::KeyValues with the VMT lexical dialect, merges modern DX9+
 * conditional subblocks, skips legacy DX6/DX7/DX8 subblocks, preserves all
 * top-level parameters including custom `$var` variables, and extracts
 * `Proxies` entries in declaration order.
 */
class VmtParser {
public:
    static Core::Result<VmtMaterial> parse(const QByteArray& utf8Content, const QString& relativeVmtPath = {});
    static Core::Result<VmtMaterial> parseString(const QString& content, const QString& relativeVmtPath = {});

    static QString normalizeShaderName(const QString& rawShaderName);
    static std::optional<VmtColor> parseColor(const QString& rawValue);
    static std::optional<std::array<float, 2>> parseVec2(const QString& rawValue);
    static std::optional<std::array<float, 3>> parseVec3(const QString& rawValue);
    static std::optional<VmtTransform> parseTransform(const QString& rawValue);
    static QString normalizeTexturePath(const QString& rawPath);
};

} // namespace Domain::Material
