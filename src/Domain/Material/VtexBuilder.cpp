#include "Domain/Material/VtexBuilder.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "Core/Error/ErrorCode.h"

namespace Domain::Material {

namespace {

constexpr int kDefaultLutSliceCount = 32;
constexpr int kDefaultLutSliceWidth = 32;
constexpr int kDefaultLutSliceHeight = 32;

struct InputTextureSpec {
    QString name;
    QString fileName;
    QString colorSpace;
    QString typeString;
    QStringList imageProcessors;
    std::optional<int> sliceCount3D;
    std::optional<int> sliceWidth3D;
    std::optional<int> sliceHeight3D;
};

struct OutputChannelSpec {
    QString inputTextureName;
    QString srcChannels;
    QString dstChannels;
    QString mipAlgorithm;
    QString outputColorSpace;
};

Core::Result<void> validateRelativePath(const QString& path)
{
    if (path.isEmpty() || path.trimmed().isEmpty()) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("VtexBuilder", "Texture input path must not be empty"));
    }

    for (const QChar ch : path) {
        if (ch == QLatin1Char('\0') || ch == QLatin1Char('\r') || ch == QLatin1Char('\n')
            || ch == QLatin1Char('"')) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate(
                    "VtexBuilder",
                    "Texture input path contains invalid characters"),
                path);
        }
    }

    if (path.contains(QLatin1Char(':'))) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate(
                "VtexBuilder",
                "Texture input path must be relative and must not contain a drive specifier"),
            path);
    }

    if (path.contains(QLatin1Char('\\'))) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate(
                "VtexBuilder",
                "Texture input path must use forward slashes"),
            path);
    }

    if (path.startsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('/'))) {
        return Core::Result<void>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate(
                "VtexBuilder",
                "Texture input path must be a relative file path"),
            path);
    }

    const QStringList segments = path.split(QLatin1Char('/'));
    for (const QString& segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String("..")) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate(
                    "VtexBuilder",
                    "Texture input path must not contain empty or parent traversal segments"),
                path);
        }
    }

    return Core::Result<void>::success();
}

Core::Result<void> validateMksFileName(const QString& fileName)
{
    const auto baseCheck = validateRelativePath(fileName);
    if (baseCheck.isFailure()) {
        return baseCheck;
    }

    for (const QChar ch : fileName) {
        if (ch.isSpace() || ch == QLatin1Char('(') || ch == QLatin1Char(')')) {
            return Core::Result<void>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate(
                    "VtexBuilder",
                    "MKS frame file name must not contain whitespace or parentheses"),
                fileName);
        }
    }

    return Core::Result<void>::success();
}

void appendImageProcessor(QString& out, const QString& algorithm, const QString& indent)
{
    out += indent + QStringLiteral("\"CDmeImageProcessor\"\n");
    out += indent + QStringLiteral("{\n");
    out += indent + QStringLiteral("\t\"m_algorithm\" \"string\" \"") + algorithm + QStringLiteral("\"\n");
    out += indent + QStringLiteral("\t\"m_stringArg\" \"string\" \"\"\n");
    out += indent + QStringLiteral("\t\"m_vFloat4Arg\" \"vector4\" \"0 0 0 0\"\n");
    out += indent + QStringLiteral("}");
}

void appendInputTexture(QString& out, const InputTextureSpec& spec, bool hasTrailingComma)
{
    out += QStringLiteral("\t\t\"CDmeInputTexture\"\n");
    out += QStringLiteral("\t\t{\n");
    out += QStringLiteral("\t\t\t\"m_name\" \"string\" \"") + spec.name + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\"m_fileName\" \"string\" \"") + spec.fileName + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\"m_colorSpace\" \"string\" \"") + spec.colorSpace + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\"m_typeString\" \"string\" \"") + spec.typeString + QStringLiteral("\"\n");

    if (spec.sliceCount3D.has_value()) {
        out += QStringLiteral("\t\t\t\"m_n3DSliceCount\" \"int\" \"")
            + QString::number(*spec.sliceCount3D) + QStringLiteral("\"\n");
    }
    if (spec.sliceWidth3D.has_value()) {
        out += QStringLiteral("\t\t\t\"m_n3DSliceWidth\" \"int\" \"")
            + QString::number(*spec.sliceWidth3D) + QStringLiteral("\"\n");
    }
    if (spec.sliceHeight3D.has_value()) {
        out += QStringLiteral("\t\t\t\"m_n3DSliceHeight\" \"int\" \"")
            + QString::number(*spec.sliceHeight3D) + QStringLiteral("\"\n");
    }

    out += QStringLiteral("\t\t\t\"m_imageProcessorArray\" \"element_array\"\n");
    out += QStringLiteral("\t\t\t[\n");
    for (int i = 0; i < spec.imageProcessors.size(); ++i) {
        appendImageProcessor(out, spec.imageProcessors.at(i), QStringLiteral("\t\t\t\t"));
        if (i + 1 < spec.imageProcessors.size()) {
            out += QLatin1Char(',');
        }
        out += QLatin1Char('\n');
    }
    out += QStringLiteral("\t\t\t]\n");
    out += QStringLiteral("\t\t}");
    if (hasTrailingComma) {
        out += QLatin1Char(',');
    }
    out += QLatin1Char('\n');
}

void appendOutputChannel(QString& out, const OutputChannelSpec& spec, bool hasTrailingComma)
{
    out += QStringLiteral("\t\t\"CDmeTextureOutputChannel\"\n");
    out += QStringLiteral("\t\t{\n");
    out += QStringLiteral("\t\t\t\"m_inputTextureArray\" \"string_array\"\n");
    out += QStringLiteral("\t\t\t[\n");
    out += QStringLiteral("\t\t\t\t\"") + spec.inputTextureName + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t]\n");
    out += QStringLiteral("\t\t\t\"m_srcChannels\" \"string\" \"") + spec.srcChannels + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\"m_dstChannels\" \"string\" \"") + spec.dstChannels + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\"m_mipAlgorithm\" \"CDmeImageProcessor\"\n");
    out += QStringLiteral("\t\t\t{\n");
    out += QStringLiteral("\t\t\t\t\"m_algorithm\" \"string\" \"") + spec.mipAlgorithm + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t\t\t\"m_stringArg\" \"string\" \"\"\n");
    out += QStringLiteral("\t\t\t\t\"m_vFloat4Arg\" \"vector4\" \"0 0 0 0\"\n");
    out += QStringLiteral("\t\t\t}\n");
    out += QStringLiteral("\t\t\t\"m_outputColorSpace\" \"string\" \"") + spec.outputColorSpace + QStringLiteral("\"\n");
    out += QStringLiteral("\t\t}");
    if (hasTrailingComma) {
        out += QLatin1Char(',');
    }
    out += QLatin1Char('\n');
}

} // namespace

Core::Result<QString> VtexBuilder::buildVtex(const VtexBuildRequest& request)
{
    if (const auto pathCheck = validateRelativePath(request.inputRelativePath); pathCheck.isFailure()) {
        return Core::Result<QString>::failure(pathCheck.error());
    }

    if (request.preset == VtexPreset::DualInputColorTrans) {
        if (const auto secondaryCheck = validateRelativePath(request.secondaryRelativePath);
            secondaryCheck.isFailure()) {
            return Core::Result<QString>::failure(secondaryCheck.error());
        }
    }

    const bool hasHeader = request.sourceVtfHeader.has_value();
    const bool noMipFlag = hasHeader && request.sourceVtfHeader->hasFlag(VtfFlag::NoMip);
    const bool noLodFlag = hasHeader && request.sourceVtfHeader->hasFlag(VtfFlag::NoLod);

    std::vector<InputTextureSpec> inputs;
    std::vector<OutputChannelSpec> outputs;
    QString outputTypeString = QStringLiteral("2D");
    QString outputFormat = QStringLiteral("BC7");
    bool clampS = false;
    bool clampT = false;
    bool clampU = false;
    bool noLod = false;

    const auto resolveMip = [noMipFlag](const QString& defaultMip) -> QString {
        return noMipFlag ? QStringLiteral("None") : defaultMip;
    };

    switch (request.preset) {
    case VtexPreset::Sprite2DAlpha: {
        inputs.push_back(InputTextureSpec{
            QStringLiteral("InputTexture0"),
            request.inputRelativePath,
            QStringLiteral("srgb"),
            QStringLiteral("2D"),
            {QStringLiteral("DilateColorInTransparentPixels")},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("InputTexture0"),
            QStringLiteral("rgba"),
            QStringLiteral("rgba"),
            resolveMip(QStringLiteral("Box")),
            QStringLiteral("srgb"),
        });
        clampS = true;
        clampT = true;
        clampU = false;
        noLod = true;
        break;
    }

    case VtexPreset::Sprite2DOpaque: {
        inputs.push_back(InputTextureSpec{
            QStringLiteral("InputTexture0"),
            request.inputRelativePath,
            QStringLiteral("srgb"),
            QStringLiteral("2D"),
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("InputTexture0"),
            QStringLiteral("rgb"),
            QStringLiteral("rgb"),
            resolveMip(QStringLiteral("Box")),
            QStringLiteral("srgb"),
        });
        clampS = true;
        clampT = true;
        clampU = false;
        noLod = true;
        break;
    }

    case VtexPreset::DualInputColorTrans: {
        inputs.push_back(InputTextureSpec{
            QStringLiteral("Color"),
            request.inputRelativePath,
            QStringLiteral("srgb"),
            QStringLiteral("2D"),
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        inputs.push_back(InputTextureSpec{
            QStringLiteral("Trans"),
            request.secondaryRelativePath,
            QStringLiteral("linear"),
            QStringLiteral("2D"),
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        const QString mipAlg = resolveMip(QStringLiteral("Box"));
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("Color"),
            QStringLiteral("rgb"),
            QStringLiteral("rgb"),
            mipAlg,
            QStringLiteral("srgb"),
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("Trans"),
            QStringLiteral("r"),
            QStringLiteral("a"),
            mipAlg,
            QStringLiteral("linear"),
        });
        break;
    }

    case VtexPreset::NormalMap2D: {
        // Hard guardrail: only NormalizeNormals is emitted; Y-channel inversion is
        // performed during PNG export (never emit LegacySource1InvertNormals).
        inputs.push_back(InputTextureSpec{
            QStringLiteral("InputTexture0"),
            request.inputRelativePath,
            QStringLiteral("linear"),
            QStringLiteral("2D"),
            {QStringLiteral("NormalizeNormals")},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("InputTexture0"),
            QStringLiteral("rgb"),
            QStringLiteral("rgb"),
            resolveMip(QStringLiteral("MipNormal")),
            QStringLiteral("linear"),
        });
        break;
    }

    case VtexPreset::Cubemap2D: {
        outputTypeString = QStringLiteral("CUBE");
        inputs.push_back(InputTextureSpec{
            QStringLiteral("InputTexture0"),
            request.inputRelativePath,
            QStringLiteral("srgb"),
            QStringLiteral("CUBE"),
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("InputTexture0"),
            QStringLiteral("rgb"),
            QStringLiteral("rgb"),
            resolveMip(QStringLiteral("Box")),
            QStringLiteral("srgb"),
        });
        break;
    }

    case VtexPreset::Volume3DLut: {
        outputTypeString = QStringLiteral("3D");
        outputFormat = QStringLiteral("RGBA8888");
        int sliceWidth = kDefaultLutSliceWidth;
        int sliceHeight = kDefaultLutSliceHeight;
        int sliceCount = kDefaultLutSliceCount;
        if (hasHeader) {
            if (request.sourceVtfHeader->width > 0) {
                sliceWidth = static_cast<int>(request.sourceVtfHeader->width);
            }
            if (request.sourceVtfHeader->height > 0) {
                sliceHeight = static_cast<int>(request.sourceVtfHeader->height);
            }
            if (request.sourceVtfHeader->depth > 1
                || (request.sourceVtfHeader->depth == 1
                    && request.sourceVtfHeader->width > 0
                    && request.sourceVtfHeader->height > 0)) {
                sliceCount = static_cast<int>(request.sourceVtfHeader->depth);
            }
        }
        inputs.push_back(InputTextureSpec{
            QStringLiteral("InputTexture0"),
            request.inputRelativePath,
            QStringLiteral("linear"),
            QStringLiteral("3D"),
            {},
            sliceCount,
            sliceWidth,
            sliceHeight,
        });
        outputs.push_back(OutputChannelSpec{
            QStringLiteral("InputTexture0"),
            QStringLiteral("rgba"),
            QStringLiteral("rgba"),
            QStringLiteral("None"),
            QStringLiteral("linear"),
        });
        clampS = true;
        clampT = true;
        clampU = true;
        noLod = true;
        break;
    }

    default:
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("VtexBuilder", "Unsupported VTEX preset"));
    }

    if (hasHeader) {
        if (request.sourceVtfHeader->hasFlag(VtfFlag::ClampS)) {
            clampS = true;
        }
        if (request.sourceVtfHeader->hasFlag(VtfFlag::ClampT)) {
            clampT = true;
        }
        if (request.sourceVtfHeader->hasFlag(VtfFlag::ClampU)) {
            clampU = true;
        }
    }

    if (noLodFlag || request.forceNoLod) {
        noLod = true;
    }

    QString out;
    out.reserve(1024);
    out += QStringLiteral("<!-- dmx encoding keyvalues2_noids 1 format vtex 1 -->\n");
    out += QStringLiteral("\"CDmeVtex\"\n");
    out += QStringLiteral("{\n");

    out += QStringLiteral("\t\"m_inputTextureArray\" \"element_array\"\n");
    out += QStringLiteral("\t[\n");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        appendInputTexture(out, inputs[i], i + 1 < inputs.size());
    }
    out += QStringLiteral("\t]\n");

    out += QStringLiteral("\t\"m_outputTypeString\" \"string\" \"") + outputTypeString + QStringLiteral("\"\n");
    out += QStringLiteral("\t\"m_outputFormat\" \"string\" \"") + outputFormat + QStringLiteral("\"\n");
    out += QStringLiteral("\t\"m_outputClearColor\" \"vector4\" \"0 0 0 0\"\n");
    out += QStringLiteral("\t\"m_nOutputMinDimension\" \"int\" \"0\"\n");
    out += QStringLiteral("\t\"m_nOutputMaxDimension\" \"int\" \"0\"\n");

    out += QStringLiteral("\t\"m_textureOutputChannelArray\" \"element_array\"\n");
    out += QStringLiteral("\t[\n");
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        appendOutputChannel(out, outputs[i], i + 1 < outputs.size());
    }
    out += QStringLiteral("\t]\n");

    out += QStringLiteral("\t\"m_vClamp\" \"vector3\" \"%1 %2 %3\"\n")
               .arg(clampS ? 1 : 0)
               .arg(clampT ? 1 : 0)
               .arg(clampU ? 1 : 0);
    out += QStringLiteral("\t\"m_bNoLod\" \"bool\" \"%1\"\n").arg(noLod ? 1 : 0);
    out += QStringLiteral("}\n");

    return Core::Result<QString>::success(std::move(out));
}

Core::Result<QString> VtexBuilder::buildMultiFrameMks(
    const QStringList& frameFileNames,
    bool loop)
{
    if (frameFileNames.isEmpty()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("VtexBuilder", "Multi-frame MKS requires at least one frame file"));
    }

    for (const QString& fileName : frameFileNames) {
        if (const auto check = validateMksFileName(fileName); check.isFailure()) {
            return Core::Result<QString>::failure(check.error());
        }
    }

    QString out;
    out.reserve(64 + frameFileNames.size() * 32);
    out += QStringLiteral("packmode flat\n\n");
    out += QStringLiteral("sequence 0\n");
    if (loop) {
        out += QStringLiteral("loop\n");
    }
    for (const QString& fileName : frameFileNames) {
        out += QStringLiteral("frame ") + fileName + QStringLiteral(" 1\n");
    }

    return Core::Result<QString>::success(std::move(out));
}

Core::Result<QString> VtexBuilder::buildSheetMks(
    const QString& sheetImageFileName,
    int imageWidth,
    int imageHeight,
    const VtfSheetData& sheetData)
{
    if (const auto check = validateMksFileName(sheetImageFileName); check.isFailure()) {
        return Core::Result<QString>::failure(check.error());
    }

    if (imageWidth <= 0 || imageHeight <= 0) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate(
                "VtexBuilder",
                "Sheet image dimensions must be positive"));
    }

    if (sheetData.sequences.empty()) {
        return Core::Result<QString>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate(
                "VtexBuilder",
                "Sheet metadata must contain at least one sequence"));
    }

    for (const VtfSheetSequence& seq : sheetData.sequences) {
        if (seq.frames.empty()) {
            return Core::Result<QString>::failure(
                Core::Error::ErrorCode::InvalidArgument,
                QCoreApplication::translate(
                    "VtexBuilder",
                    "Sheet sequence must contain at least one frame"),
                QStringLiteral("sequence %1").arg(seq.sequenceNumber));
        }
    }

    QString out;
    out.reserve(256);
    out += QStringLiteral("packmode flat\n");

    const double widthD = static_cast<double>(imageWidth);
    const double heightD = static_cast<double>(imageHeight);

    for (const VtfSheetSequence& seq : sheetData.sequences) {
        out += QStringLiteral("\nsequence %1\n").arg(seq.sequenceNumber);
        out += seq.clamp
            ? QStringLiteral("clamp-extendlastframe\n")
            : QStringLiteral("loop\n");

        for (const VtfSheetFrame& frame : seq.frames) {
            const double uMin = std::isfinite(frame.uvRect[0]) ? static_cast<double>(frame.uvRect[0]) : 0.0;
            const double vMin = std::isfinite(frame.uvRect[1]) ? static_cast<double>(frame.uvRect[1]) : 0.0;
            const double uMax = std::isfinite(frame.uvRect[2]) ? static_cast<double>(frame.uvRect[2]) : 1.0;
            const double vMax = std::isfinite(frame.uvRect[3]) ? static_cast<double>(frame.uvRect[3]) : 1.0;

            const int x0 = std::clamp(static_cast<int>(std::floor(uMin * widthD)), 0, imageWidth - 1);
            const int y0 = std::clamp(static_cast<int>(std::floor(vMin * heightD)), 0, imageHeight - 1);
            const int x1 = std::clamp(static_cast<int>(std::ceil(uMax * widthD)) - 1, x0, imageWidth - 1);
            const int y1 = std::clamp(static_cast<int>(std::ceil(vMax * heightD)) - 1, y0, imageHeight - 1);

            const float duration = (std::isfinite(frame.duration) && frame.duration > 0.0f)
                ? frame.duration
                : 1.0f;

            out += QStringLiteral("frame %1(%2,%3,%4,%5) %6\n")
                       .arg(sheetImageFileName)
                       .arg(x0)
                       .arg(y0)
                       .arg(x1)
                       .arg(y1)
                       .arg(QString::number(duration, 'g', 6));
        }
    }

    return Core::Result<QString>::success(std::move(out));
}

} // namespace Domain::Material
