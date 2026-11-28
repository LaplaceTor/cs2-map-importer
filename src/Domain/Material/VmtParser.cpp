#include "Domain/Material/VmtParser.h"

#include "Core/Error/ErrorCode.h"
#include "Core/KeyValues/KeyValuesNode.h"
#include "Core/KeyValues/KeyValuesReader.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <cmath>
#include <utility>
#include <vector>

namespace Domain::Material {

namespace {

constexpr int kMaxSubblockDepth = 4;

std::vector<QStringView> splitWhitespace(QStringView text) {
    std::vector<QStringView> tokens;
    qsizetype i = 0;
    const qsizetype len = text.size();
    while (i < len) {
        while (i < len && text.at(i).isSpace()) {
            ++i;
        }
        if (i >= len) {
            break;
        }
        const qsizetype start = i;
        while (i < len && !text.at(i).isSpace()) {
            ++i;
        }
        tokens.push_back(text.sliced(start, i - start));
    }
    return tokens;
}

bool parseFiniteFloat(QStringView token, float& outValue) noexcept {
    bool ok = false;
    const float value = token.toFloat(&ok);
    if (!ok || !std::isfinite(value)) {
        return false;
    }
    outValue = value;
    return true;
}

QStringView stripEnclosingBrackets(QStringView text) noexcept {
    text = text.trimmed();
    if (text.size() >= 2) {
        const QChar first = text.front();
        const QChar last = text.back();
        if ((first == QLatin1Char('[') && last == QLatin1Char(']')) ||
            (first == QLatin1Char('{') && last == QLatin1Char('}'))) {
            return text.sliced(1, text.size() - 2).trimmed();
        }
    }
    return text;
}

bool isObsoleteSubblock(const QString& blockName) {
    if (blockName.startsWith(QLatin1String("<dx90")) ||
        blockName.startsWith(QLatin1String("<=dx8")) ||
        blockName.startsWith(QLatin1String("<dx8")) ||
        blockName.startsWith(QLatin1String("<=dx7")) ||
        blockName.startsWith(QLatin1String("<dx7")) ||
        blockName == QLatin1String("water_dx60") ||
        blockName == QLatin1String("gpu<2") ||
        blockName == QLatin1String("gpu<=1") ||
        blockName == QLatin1String("gpu<1")) {
        return true;
    }
    static const QRegularExpression kLegacyDxSuffix(QStringLiteral("_dx[678]\\d*$"));
    return kLegacyDxSuffix.match(blockName).hasMatch();
}

bool isModernDx9Subblock(const QString& blockName, const QString& normalizedShaderName) {
    if (blockName == QLatin1String(">=dx90") ||
        blockName == QLatin1String(">=dx90_20b") ||
        blockName == QLatin1String(">=dx95") ||
        blockName == QLatin1String("gpu>=2")) {
        return true;
    }
    if (!normalizedShaderName.isEmpty()) {
        if (blockName == normalizedShaderName + QLatin1String("_dx9") ||
            blockName == normalizedShaderName + QLatin1String("_dx90") ||
            blockName == normalizedShaderName + QLatin1String("_hdr_dx9") ||
            blockName == normalizedShaderName + QLatin1String("_hdr_dx90")) {
            return true;
        }
    }
    return false;
}

void extractProxies(const Core::KeyValues::KeyValuesNode& proxiesNode, QVector<VmtProxyEntry>& outProxies) {
    for (const auto& child : proxiesNode.children()) {
        if (!child.isSection()) {
            continue;
        }
        const QString proxyName = child.name().trimmed().toLower();
        if (proxyName.isEmpty()) {
            continue;
        }

        VmtProxyEntry entry;
        entry.name = proxyName;
        for (const auto& prop : child.children()) {
            if (!prop.isProperty()) {
                continue;
            }
            const QString key = prop.name().trimmed().toLower();
            if (!key.isEmpty()) {
                entry.params.insert(key, prop.value());
            }
        }
        outProxies.push_back(std::move(entry));
    }
}

void mergeModernSubblock(const Core::KeyValues::KeyValuesNode& section, VmtMaterial& material, int depth) {
    if (depth > kMaxSubblockDepth) {
        return;
    }

    for (const auto& child : section.children()) {
        if (child.isProperty()) {
            const QString key = child.name().trimmed().toLower();
            if (!key.isEmpty()) {
                material.params.insert(key, child.value());
            }
        }
    }

    for (const auto& child : section.children()) {
        if (!child.isSection()) {
            continue;
        }
        const QString subName = child.name().trimmed().toLower();
        if (subName == QLatin1String("proxies")) {
            extractProxies(child, material.proxies);
        } else if (!isObsoleteSubblock(subName) && isModernDx9Subblock(subName, material.normalizedShaderName)) {
            mergeModernSubblock(child, material, depth + 1);
        }
    }
}

} // namespace

// ============================================================================
// VmtMaterial typed accessors
// ============================================================================

bool VmtMaterial::hasParam(const QString& key) const {
    return params.contains(key.toLower());
}

QString VmtMaterial::stringParam(const QString& key, const QString& defaultValue) const {
    const auto it = params.constFind(key.toLower());
    return it != params.constEnd() ? it.value() : defaultValue;
}

bool VmtMaterial::boolParam(const QString& key, bool defaultValue) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return defaultValue;
    }

    const QString trimmed = it.value().trimmed().toLower();
    if (trimmed == QLatin1String("1") || trimmed == QLatin1String("true") ||
        trimmed == QLatin1String("yes") || trimmed == QLatin1String("on")) {
        return true;
    }
    if (trimmed == QLatin1String("0") || trimmed == QLatin1String("false") ||
        trimmed == QLatin1String("no") || trimmed == QLatin1String("off")) {
        return false;
    }

    bool ok = false;
    const double numeric = trimmed.toDouble(&ok);
    if (ok && std::isfinite(numeric)) {
        return numeric != 0.0;
    }
    return defaultValue;
}

int VmtMaterial::intParam(const QString& key, int defaultValue) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return defaultValue;
    }

    const QString trimmed = it.value().trimmed();
    bool ok = false;
    const int intVal = trimmed.toInt(&ok);
    if (ok) {
        return intVal;
    }

    const double dblVal = trimmed.toDouble(&ok);
    if (ok && std::isfinite(dblVal)) {
        return static_cast<int>(dblVal);
    }
    return defaultValue;
}

float VmtMaterial::floatParam(const QString& key, float defaultValue) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return defaultValue;
    }

    const QStringView stripped = stripEnclosingBrackets(it.value());
    float value = 0.0f;
    if (parseFiniteFloat(stripped, value)) {
        return value;
    }
    return defaultValue;
}

std::optional<VmtColor> VmtMaterial::colorParam(const QString& key) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return std::nullopt;
    }
    return VmtParser::parseColor(it.value());
}

std::optional<std::array<float, 2>> VmtMaterial::vec2Param(const QString& key) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return std::nullopt;
    }
    return VmtParser::parseVec2(it.value());
}

std::optional<std::array<float, 3>> VmtMaterial::vec3Param(const QString& key) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return std::nullopt;
    }
    return VmtParser::parseVec3(it.value());
}

std::optional<VmtTransform> VmtMaterial::transformParam(const QString& key) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return std::nullopt;
    }
    return VmtParser::parseTransform(it.value());
}

QString VmtMaterial::normalizedTexturePath(const QString& key) const {
    const auto it = params.constFind(key.toLower());
    if (it == params.constEnd()) {
        return {};
    }
    return VmtParser::normalizeTexturePath(it.value());
}

// ============================================================================
// VmtParser
// ============================================================================

Core::Result<VmtMaterial> VmtParser::parse(const QByteArray& utf8Content, const QString& relativeVmtPath) {
    Core::KeyValues::KeyValuesReader reader(utf8Content, Core::KeyValues::KeyValuesReadOptions::vmt());
    Core::KeyValues::KeyValuesNode docRoot;
    const auto readRes = reader.readDocument(docRoot);
    if (!readRes.isSuccess()) {
        QString details = readRes.details();
        if (!relativeVmtPath.isEmpty()) {
            details = details.isEmpty()
                ? relativeVmtPath
                : QStringLiteral("%1: %2").arg(relativeVmtPath, details);
        }
        return Core::Result<VmtMaterial>::failure(
            readRes.errorCode(),
            QCoreApplication::translate("VmtParser", "Failed to parse VMT KeyValues syntax"),
            details);
    }

    const Core::KeyValues::KeyValuesNode* shaderSection = nullptr;
    for (const auto& child : docRoot.children()) {
        if (child.isSection()) {
            shaderSection = &child;
            break;
        }
    }

    if (shaderSection == nullptr || shaderSection->name().trimmed().isEmpty()) {
        return Core::Result<VmtMaterial>::failure(
            Core::Error::ErrorCode::InvalidFile,
            QCoreApplication::translate("VmtParser", "VMT content does not contain a root shader section"),
            relativeVmtPath);
    }

    VmtMaterial material;
    material.relativePath = relativeVmtPath;
    material.rawShaderName = shaderSection->name().trimmed();
    material.normalizedShaderName = normalizeShaderName(material.rawShaderName);

    if (material.normalizedShaderName == QLatin1String("patch")) {
        material.warnings.append(
            QCoreApplication::translate("VmtParser", "Patch shader is not recursively resolved in this phase"));
    }

    // Pass 1: preserve all top-level properties (including custom $var variables).
    for (const auto& child : shaderSection->children()) {
        if (!child.isProperty()) {
            continue;
        }
        const QString key = child.name().trimmed().toLower();
        if (!key.isEmpty()) {
            material.params.insert(key, child.value());
        }
    }

    // Pass 2: arbitrate subblocks (merge DX9+, skip legacy DX6/7/8, extract ordered Proxies).
    for (const auto& child : shaderSection->children()) {
        if (!child.isSection()) {
            continue;
        }
        const QString blockName = child.name().trimmed().toLower();
        if (blockName == QLatin1String("proxies")) {
            extractProxies(child, material.proxies);
            continue;
        }
        if (isObsoleteSubblock(blockName)) {
            continue;
        }
        if (isModernDx9Subblock(blockName, material.normalizedShaderName)) {
            mergeModernSubblock(child, material, 1);
        }
    }

    return Core::Result<VmtMaterial>::success(std::move(material));
}

Core::Result<VmtMaterial> VmtParser::parseString(const QString& content, const QString& relativeVmtPath) {
    return parse(content.toUtf8(), relativeVmtPath);
}

QString VmtParser::normalizeShaderName(const QString& rawShaderName) {
    QString normalized = rawShaderName.trimmed().toLower();
    static const QRegularExpression kDxSuffixRegex(QStringLiteral("_(?:hdr_)?dx\\d+$"));
    normalized.remove(kDxSuffixRegex);
    return normalized;
}

std::optional<VmtColor> VmtParser::parseColor(const QString& rawValue) {
    QStringView trimmed = QStringView(rawValue).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }

    bool isByteScale = false;
    if (trimmed.size() >= 2 && trimmed.front() == QLatin1Char('{') && trimmed.back() == QLatin1Char('}')) {
        isByteScale = true;
        trimmed = trimmed.sliced(1, trimmed.size() - 2).trimmed();
    } else if (trimmed.size() >= 2 && trimmed.front() == QLatin1Char('[') && trimmed.back() == QLatin1Char(']')) {
        trimmed = trimmed.sliced(1, trimmed.size() - 2).trimmed();
    }

    const std::vector<QStringView> tokens = splitWhitespace(trimmed);
    if (tokens.size() != 1 && tokens.size() != 3 && tokens.size() != 4) {
        return std::nullopt;
    }

    std::array<float, 4> values = {1.0f, 1.0f, 1.0f, 1.0f};
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!parseFiniteFloat(tokens[i], values[i])) {
            return std::nullopt;
        }
    }

    VmtColor color;
    color.isByteScale = isByteScale;
    const float scale = isByteScale ? (1.0f / 255.0f) : 1.0f;

    if (tokens.size() == 1) {
        const float v = values[0] * scale;
        color.r = v;
        color.g = v;
        color.b = v;
        color.a = 1.0f;
    } else if (tokens.size() == 3) {
        color.r = values[0] * scale;
        color.g = values[1] * scale;
        color.b = values[2] * scale;
        color.a = 1.0f;
    } else {
        color.r = values[0] * scale;
        color.g = values[1] * scale;
        color.b = values[2] * scale;
        color.a = values[3] * scale;
    }

    return color;
}

std::optional<std::array<float, 2>> VmtParser::parseVec2(const QString& rawValue) {
    const QStringView inner = stripEnclosingBrackets(rawValue);
    if (inner.isEmpty()) {
        return std::nullopt;
    }

    const std::vector<QStringView> tokens = splitWhitespace(inner);
    if (tokens.size() == 1) {
        float v = 0.0f;
        if (!parseFiniteFloat(tokens[0], v)) {
            return std::nullopt;
        }
        return std::array<float, 2>{v, v};
    }
    if (tokens.size() == 2) {
        float x = 0.0f;
        float y = 0.0f;
        if (!parseFiniteFloat(tokens[0], x) || !parseFiniteFloat(tokens[1], y)) {
            return std::nullopt;
        }
        return std::array<float, 2>{x, y};
    }
    return std::nullopt;
}

std::optional<std::array<float, 3>> VmtParser::parseVec3(const QString& rawValue) {
    const QStringView inner = stripEnclosingBrackets(rawValue);
    if (inner.isEmpty()) {
        return std::nullopt;
    }

    const std::vector<QStringView> tokens = splitWhitespace(inner);
    if (tokens.size() == 1) {
        float v = 0.0f;
        if (!parseFiniteFloat(tokens[0], v)) {
            return std::nullopt;
        }
        return std::array<float, 3>{v, v, v};
    }
    if (tokens.size() == 3) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!parseFiniteFloat(tokens[0], x) ||
            !parseFiniteFloat(tokens[1], y) ||
            !parseFiniteFloat(tokens[2], z)) {
            return std::nullopt;
        }
        return std::array<float, 3>{x, y, z};
    }
    return std::nullopt;
}

std::optional<VmtTransform> VmtParser::parseTransform(const QString& rawValue) {
    const std::vector<QStringView> tokens = splitWhitespace(QStringView(rawValue).trimmed());
    if (tokens.empty()) {
        return std::nullopt;
    }

    VmtTransform transform;
    bool parsedAnyClause = false;
    size_t i = 0;

    while (i < tokens.size()) {
        const QString keyword = tokens[i].toString().toLower();
        if (keyword == QLatin1String("center")) {
            if (i + 2 >= tokens.size() ||
                !parseFiniteFloat(tokens[i + 1], transform.center[0]) ||
                !parseFiniteFloat(tokens[i + 2], transform.center[1])) {
                return std::nullopt;
            }
            i += 3;
            parsedAnyClause = true;
        } else if (keyword == QLatin1String("scale")) {
            if (i + 2 >= tokens.size() ||
                !parseFiniteFloat(tokens[i + 1], transform.scale[0]) ||
                !parseFiniteFloat(tokens[i + 2], transform.scale[1])) {
                return std::nullopt;
            }
            i += 3;
            parsedAnyClause = true;
        } else if (keyword == QLatin1String("rotate")) {
            if (i + 1 >= tokens.size() ||
                !parseFiniteFloat(tokens[i + 1], transform.rotateDeg)) {
                return std::nullopt;
            }
            i += 2;
            parsedAnyClause = true;
        } else if (keyword == QLatin1String("translate")) {
            if (i + 2 >= tokens.size() ||
                !parseFiniteFloat(tokens[i + 1], transform.translate[0]) ||
                !parseFiniteFloat(tokens[i + 2], transform.translate[1])) {
                return std::nullopt;
            }
            i += 3;
            parsedAnyClause = true;
        } else {
            return std::nullopt;
        }
    }

    if (!parsedAnyClause) {
        return std::nullopt;
    }
    return transform;
}

QString VmtParser::normalizeTexturePath(const QString& rawPath) {
    QString normalized = rawPath.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalized = normalized.toLower();

    // Collapse consecutive slashes.
    QString collapsed;
    collapsed.reserve(normalized.size());
    bool prevSlash = false;
    for (const QChar ch : normalized) {
        if (ch == QLatin1Char('/')) {
            if (!prevSlash) {
                collapsed.append(ch);
            }
            prevSlash = true;
        } else {
            collapsed.append(ch);
            prevSlash = false;
        }
    }
    normalized = std::move(collapsed);

    while (normalized.startsWith(QLatin1Char('/'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }

    if (normalized.startsWith(QLatin1String("materials/"))) {
        normalized.remove(0, 10);
    }
    while (normalized.startsWith(QLatin1Char('/'))) {
        normalized.remove(0, 1);
    }

    if (normalized.endsWith(QLatin1String(".vtf"))) {
        normalized.chop(4);
    }

    return normalized;
}

} // namespace Domain::Material
