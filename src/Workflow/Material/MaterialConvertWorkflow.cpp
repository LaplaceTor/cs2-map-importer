#include "Workflow/Material/MaterialConvertWorkflow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QString>
#include <QStringList>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Core/FileSystem/FileSystem.h"
#include "Domain/Asset/AssetLocateStrategy.h"
#include "Domain/Material/MaterialConvertTypes.h"
#include "Domain/Material/MaterialTextureSynthesizer.h"
#include "Domain/Material/MaterialTranslator.h"
#include "Domain/Material/SkyboxCubeBuilder.h"
#include "Domain/Material/SkyboxTypes.h"
#include "Domain/Material/TextureIO.h"
#include "Domain/Material/VmtDocument.h"
#include "Domain/Material/VmtParser.h"
#include "Domain/Material/VtexBuilder.h"
#include "Domain/Material/VtfCodec.h"
#include "Domain/Package/PackArchive.h"
#include "Domain/Package/PackArchivePool.h"
#include "Domain/Package/VpkIndex.h"
#include "Domain/Tool/Cs2PathLayout.h"
#include "Domain/Tool/ResourceCompilerTool.h"

namespace Workflow::Material {

namespace {

struct SubrectCrop {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool isValid() const noexcept {
        return width > 0 && height > 0;
    }
};

struct LoadedAsset {
    Domain::Asset::AssetMatchKind kind = Domain::Asset::AssetMatchKind::NotFound;
    QByteArray bytes;
};

QString normalizeMaterialRelativePath(const QString& rawPath)
{
    QString norm = rawPath.trimmed();
    norm.replace(u'\\', u'/');
    while (norm.startsWith(u'/')) {
        norm.remove(0, 1);
    }
    norm = norm.toLower();
    if (norm.isEmpty()) {
        return {};
    }
    if (!norm.startsWith(QStringLiteral("materials/"))) {
        norm.prepend(QStringLiteral("materials/"));
    }
    if (!norm.endsWith(QStringLiteral(".vmt"))) {
        norm.append(QStringLiteral(".vmt"));
    }
    return norm;
}

QString normalizeSkyboxName(const QString& rawName)
{
    QString norm = rawName.trimmed();
    norm.replace(u'\\', u'/');
    while (norm.startsWith(u'/')) {
        norm.remove(0, 1);
    }
    norm = norm.toLower();
    if (norm.startsWith(QStringLiteral("materials/skybox/"))) {
        norm.remove(0, 17);
    } else if (norm.startsWith(QStringLiteral("skybox/"))) {
        norm.remove(0, 7);
    }
    if (norm.endsWith(QStringLiteral(".vmt"))) {
        norm.chop(4);
    } else if (norm.endsWith(QStringLiteral(".vmat"))) {
        norm.chop(5);
    }
    return norm;
}

QString vmtToVmatRelativePath(const QString& normalizedVmtPath)
{
    if (normalizedVmtPath.endsWith(QStringLiteral(".vmt"), Qt::CaseInsensitive)) {
        return normalizedVmtPath.left(normalizedVmtPath.size() - 4) + QStringLiteral(".vmat");
    }
    return normalizedVmtPath + QStringLiteral(".vmat");
}

QString vmtToVtexRelativePath(const QString& normalizedVmtPath)
{
    if (normalizedVmtPath.endsWith(QStringLiteral(".vmt"), Qt::CaseInsensitive)) {
        return normalizedVmtPath.left(normalizedVmtPath.size() - 4) + QStringLiteral(".vtex");
    }
    return normalizedVmtPath + QStringLiteral(".vtex");
}

QString vmtStemRelativePath(const QString& normalizedVmtPath)
{
    if (normalizedVmtPath.endsWith(QStringLiteral(".vmt"), Qt::CaseInsensitive)) {
        return normalizedVmtPath.left(normalizedVmtPath.size() - 4);
    }
    return normalizedVmtPath;
}

QString vtfAssetRelativePath(const QString& normalizedTexturePath)
{
    return QStringLiteral("materials/%1.vtf").arg(normalizedTexturePath);
}

QString insertSuffixBeforeExtension(const QString& relPath, const QString& suffix)
{
    const int dotIdx = relPath.lastIndexOf(u'.');
    const int slashIdx = relPath.lastIndexOf(u'/');
    if (dotIdx > slashIdx) {
        return relPath.left(dotIdx) + suffix + relPath.mid(dotIdx);
    }
    return relPath + suffix;
}

QString formatOptionalVec3(const std::optional<std::array<float, 3>>& vec)
{
    if (!vec.has_value()) {
        return QStringLiteral("none");
    }
    return QStringLiteral("%1,%2,%3")
        .arg(static_cast<double>((*vec)[0]), 0, 'f', 4)
        .arg(static_cast<double>((*vec)[1]), 0, 'f', 4)
        .arg(static_cast<double>((*vec)[2]), 0, 'f', 4);
}

QString formatOptionalTransform(const std::optional<Domain::Material::VmtTransform>& t)
{
    if (!t.has_value() || t->isIdentity()) {
        return QStringLiteral("id");
    }
    return QStringLiteral("%1,%2;%3,%4;%5;%6,%7")
        .arg(static_cast<double>(t->center[0]), 0, 'f', 4)
        .arg(static_cast<double>(t->center[1]), 0, 'f', 4)
        .arg(static_cast<double>(t->scale[0]), 0, 'f', 4)
        .arg(static_cast<double>(t->scale[1]), 0, 'f', 4)
        .arg(static_cast<double>(t->rotateDeg), 0, 'f', 4)
        .arg(static_cast<double>(t->translate[0]), 0, 'f', 4)
        .arg(static_cast<double>(t->translate[1]), 0, 'f', 4);
}

QString buildExportCacheKey(
    const Domain::Material::TextureExportItem& item,
    const std::optional<SubrectCrop>& crop)
{
    QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(item.sourceVtfPath)
        .arg(item.secondaryVtfPath)
        .arg(static_cast<int>(item.flavor))
        .arg(static_cast<int>(item.sourceChannel))
        .arg(formatOptionalVec3(item.envmapTint))
        .arg(item.isMetallic ? 1 : 0)
        .arg(formatOptionalVec3(item.linearTint))
        .arg(formatOptionalTransform(item.secondaryTransform));

    if (crop.has_value() && crop->isValid()) {
        key += QStringLiteral("|crop:%1,%2,%3,%4")
            .arg(crop->x)
            .arg(crop->y)
            .arg(crop->width)
            .arg(crop->height);
    }
    return key;
}

Domain::Material::UvTransform2D toUvTransform2D(const std::optional<Domain::Material::VmtTransform>& vmtTransform)
{
    Domain::Material::UvTransform2D uv;
    if (vmtTransform.has_value()) {
        uv.center = vmtTransform->center;
        uv.scale = vmtTransform->scale;
        uv.rotateDeg = vmtTransform->rotateDeg;
        uv.translate = vmtTransform->translate;
    }
    return uv;
}

void cleanupNewlyCreatedFiles(
    const QStringList& newlyCreatedFiles,
    const Core::Path::FilesystemPath& expectedBaseDir,
    const Common::ImportContext& context)
{
    int removed = 0;
    for (const QString& path : newlyCreatedFiles) {
        if (!expectedBaseDir.isEmpty() && !Core::Path::FilesystemPath(path).isSubpathOf(expectedBaseDir)) {
            context.warning(QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Skipping rollback cleanup of file outside destination directory: %1").arg(path));
            continue;
        }
        QFile file(path);
        if (file.exists()) {
            if (file.remove()) {
                ++removed;
            } else {
                context.warning(QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Failed to remove newly created artifact during rollback: %1 (%2)")
                    .arg(path, file.errorString()));
            }
        }
    }
    if (removed > 0) {
        context.info(QCoreApplication::translate(
            "MaterialConvertWorkflow",
            "Rolled back %1 newly created file(s)").arg(removed));
    }
}

class MaterialWorkflowSession {
public:
    MaterialWorkflowSession(
        const MaterialConvertOptions& options,
        Domain::Package::PackArchivePool& pool,
        const Common::ImportContext& context,
        MaterialConvertResult& result)
        : m_options(options)
        , m_pool(pool)
        , m_context(context)
        , m_result(result)
    {
    }

    Core::Result<LoadedAsset> locateAndReadAsset(const QString& relativePath, bool checkCs2Native)
    {
        const Domain::Package::VpkIndex* effectiveCs2Index =
            (checkCs2Native && !m_options.overwriteNativeAssets) ? m_options.cs2Index.get() : nullptr;

        auto locateRes = Domain::Asset::AssetLocateStrategy::locate(
            relativePath,
            m_options.searchTargets,
            m_pool,
            m_options.vpkIndex.get(),
            effectiveCs2Index,
            m_context.token());

        if (locateRes.isCancelled()) {
            return Core::Result<LoadedAsset>::cancelled(locateRes.message());
        }
        if (locateRes.isFailure()) {
            return Core::Result<LoadedAsset>::failure(locateRes.error(), locateRes.message());
        }

        const auto& outcome = locateRes.value();
        if (outcome.kind == Domain::Asset::AssetMatchKind::NativeCs2
            || outcome.kind == Domain::Asset::AssetMatchKind::NotFound
            || !outcome.location.has_value()) {
            return Core::Result<LoadedAsset>::success(LoadedAsset{outcome.kind, {}});
        }

        const auto& loc = *outcome.location;
        if (!loc.isInsidePack) {
            Core::Error::ExecutionContext readCtx{
                .stage = QStringLiteral("Reading loose asset"),
                .resourcePath = loc.looseFilePath.toString()
            };
            auto bytesRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<QByteArray> {
                return Core::Result<QByteArray>::success(
                    Core::FileSystem::FileSystem::readAll(loc.looseFilePath.toString()));
            }, readCtx);
            if (bytesRes.isCancelled()) {
                return Core::Result<LoadedAsset>::cancelled(bytesRes.message());
            }
            if (bytesRes.isFailure()) {
                return Core::Result<LoadedAsset>::failure(bytesRes.error(), bytesRes.message());
            }
            return Core::Result<LoadedAsset>::success(LoadedAsset{outcome.kind, std::move(bytesRes.value())});
        }

        auto archiveRes = m_pool.getOrOpen(loc.sourceTargetPath, m_context.token());
        if (archiveRes.isCancelled()) {
            return Core::Result<LoadedAsset>::cancelled(archiveRes.message());
        }
        if (archiveRes.isFailure()) {
            return Core::Result<LoadedAsset>::failure(archiveRes.error(), archiveRes.message());
        }

        auto entryRes = archiveRes.value()->readEntry(loc.relativePath);
        if (entryRes.isCancelled()) {
            return Core::Result<LoadedAsset>::cancelled(entryRes.message());
        }
        if (entryRes.isFailure()) {
            return Core::Result<LoadedAsset>::failure(entryRes.error(), entryRes.message());
        }

        const auto& rawBytes = entryRes.value();
        QByteArray byteArray(
            reinterpret_cast<const char*>(rawBytes.data()),
            static_cast<qsizetype>(rawBytes.size()));
        return Core::Result<LoadedAsset>::success(LoadedAsset{outcome.kind, std::move(byteArray)});
    }

    Core::Result<Core::Path::FilesystemPath> resolveDestPath(const QString& relPath)
    {
        auto resolvedOpt = m_options.destContentDir.resolveBelow(relPath);
        if (!resolvedOpt.has_value()) {
            return Core::Result<Core::Path::FilesystemPath>::failure(
                Core::Error::ErrorCode::InvalidPath,
                QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Destination path traverses outside content directory: %1").arg(relPath),
                relPath);
        }
        return Core::Result<Core::Path::FilesystemPath>::success(*resolvedOpt);
    }

    Core::Result<QString> writePngFile(const QString& relPngPath, const QImage& image)
    {
        auto targetRes = resolveDestPath(relPngPath);
        if (targetRes.isFailure()) {
            return Core::Result<QString>::failure(targetRes.error(), targetRes.message());
        }
        const Core::Path::FilesystemPath targetPath = targetRes.value();
        const bool existedBefore = targetPath.exists();

        Core::Error::ExecutionContext dirCtx{
            .stage = QStringLiteral("Creating texture output directory"),
            .targetPath = targetPath.parentPath().toString()
        };
        auto dirRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
            Core::FileSystem::FileSystem::createDirectory(targetPath.parentPath().toString());
            return Core::Result<void>::success();
        }, dirCtx);
        if (dirRes.isFailure()) {
            return Core::Result<QString>::failure(dirRes.error(), dirRes.message());
        }

        auto writeRes = Domain::Material::TextureIO::writeImage(targetPath, image, m_options.destContentDir);
        if (writeRes.isFailure()) {
            return Core::Result<QString>::failure(writeRes.error(), writeRes.message());
        }

        recordGeneratedFile(targetPath.toString(), !existedBefore);
        return Core::Result<QString>::success(targetPath.toString());
    }

    Core::Result<QString> writeTextFile(const QString& relPath, const QString& textContent)
    {
        auto targetRes = resolveDestPath(relPath);
        if (targetRes.isFailure()) {
            return Core::Result<QString>::failure(targetRes.error(), targetRes.message());
        }
        const Core::Path::FilesystemPath targetPath = targetRes.value();
        const bool existedBefore = targetPath.exists();

        Core::Error::ExecutionContext writeCtx{
            .stage = QStringLiteral("Writing material asset file"),
            .targetPath = targetPath.toString()
        };
        auto writeRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
            Core::FileSystem::FileSystem::createDirectory(targetPath.parentPath().toString());
            Core::FileSystem::AtomicFile::writeAtomic(
                targetPath.toString(),
                textContent.toUtf8(),
                m_options.destContentDir);
            return Core::Result<void>::success();
        }, writeCtx);

        if (writeRes.isFailure()) {
            return Core::Result<QString>::failure(writeRes.error(), writeRes.message());
        }

        recordGeneratedFile(targetPath.toString(), !existedBefore);
        return Core::Result<QString>::success(targetPath.toString());
    }

    Core::Result<void> probeVtf(
        const QString& normalizedVtfPath,
        const std::optional<SubrectCrop>& crop,
        QHash<QString, Domain::Material::VtfProbeInfo>& outProbes)
    {
        if (normalizedVtfPath.isEmpty()) {
            return Core::Result<void>::success();
        }
        if (!crop.has_value()) {
            const auto cached = m_vtfProbeCache.constFind(normalizedVtfPath);
            if (cached != m_vtfProbeCache.constEnd()) {
                outProbes.insert(normalizedVtfPath, cached.value());
                return Core::Result<void>::success();
            }
        }

        auto loadedRes = locateAndReadAsset(vtfAssetRelativePath(normalizedVtfPath), /*checkCs2Native=*/false);
        if (loadedRes.isCancelled()) {
            return Core::Result<void>::cancelled(loadedRes.message());
        }
        if (loadedRes.isFailure() || loadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
            return Core::Result<void>::success();
        }

        const QByteArray& bytes = loadedRes.value().bytes;
        const std::span<const std::byte> dataSpan(
            reinterpret_cast<const std::byte*>(bytes.constData()),
            static_cast<std::size_t>(bytes.size()));

        auto headerRes = Domain::Material::VtfCodec::readHeaderFromMemory(dataSpan, normalizedVtfPath);
        if (headerRes.isFailure()) {
            return Core::Result<void>::success();
        }

        Domain::Material::VtfProbeInfo probe;
        probe.header = headerRes.value();
        if (probe.header->hasAlphaChannel) {
            auto imgRes = Domain::Material::VtfCodec::readSlice(dataSpan, {}, normalizedVtfPath);
            if (imgRes.isSuccess()) {
                QImage img = imgRes.value();
                if (crop.has_value() && crop->isValid()) {
                    auto cropRes = Domain::Material::MaterialTextureSynthesizer::cropSubrect(
                        img, crop->x, crop->y, crop->width, crop->height, m_context.token());
                    if (cropRes.isCancelled()) {
                        return Core::Result<void>::cancelled(cropRes.message());
                    }
                    if (cropRes.isSuccess()) {
                        img = std::move(cropRes.value());
                    }
                }
                probe.hasNonOpaqueAlpha = Domain::Material::MaterialTextureSynthesizer::hasNonOpaqueAlpha(img);
            }
        }

        if (!crop.has_value()) {
            m_vtfProbeCache.insert(normalizedVtfPath, probe);
        }
        outProbes.insert(normalizedVtfPath, probe);
        return Core::Result<void>::success();
    }

    Core::Result<QImage> decodeVtfFrame0(const QString& normalizedVtfPath)
    {
        auto loadedRes = locateAndReadAsset(vtfAssetRelativePath(normalizedVtfPath), /*checkCs2Native=*/false);
        if (loadedRes.isCancelled()) {
            return Core::Result<QImage>::cancelled(loadedRes.message());
        }
        if (loadedRes.isFailure()) {
            return Core::Result<QImage>::failure(loadedRes.error(), loadedRes.message());
        }
        if (loadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
            return Core::Result<QImage>::failure(
                Core::Error::ErrorCode::FileNotFound,
                QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "VTF texture not found: %1").arg(vtfAssetRelativePath(normalizedVtfPath)),
                normalizedVtfPath);
        }

        const QByteArray& bytes = loadedRes.value().bytes;
        const std::span<const std::byte> dataSpan(
            reinterpret_cast<const std::byte*>(bytes.constData()),
            static_cast<std::size_t>(bytes.size()));
        return Domain::Material::VtfCodec::readSlice(dataSpan, {}, normalizedVtfPath);
    }

    Core::Result<QString> exportPlannedTexture(
        const Domain::Material::TextureExportItem& item,
        const std::optional<SubrectCrop>& crop)
    {
        if (m_context.isCancelled()) {
            return Core::Result<QString>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        const QString cacheKey = buildExportCacheKey(item, crop);
        const auto cachedIt = m_exportedTextureCache.constFind(cacheKey);
        if (cachedIt != m_exportedTextureCache.constEnd()) {
            return Core::Result<QString>::success(cachedIt.value());
        }

        auto srcRes = decodeVtfFrame0(item.sourceVtfPath);
        if (srcRes.isCancelled()) {
            return Core::Result<QString>::cancelled(srcRes.message());
        }
        if (srcRes.isFailure()) {
            return Core::Result<QString>::failure(srcRes.error(), srcRes.message());
        }

        QImage srcImg = std::move(srcRes.value());
        if (crop.has_value() && crop->isValid()) {
            auto cropRes = Domain::Material::MaterialTextureSynthesizer::cropSubrect(
                srcImg, crop->x, crop->y, crop->width, crop->height, m_context.token());
            if (cropRes.isCancelled()) {
                return Core::Result<QString>::cancelled(cropRes.message());
            }
            if (cropRes.isFailure()) {
                return Core::Result<QString>::failure(cropRes.error(), cropRes.message());
            }
            srcImg = std::move(cropRes.value());
        }

        using Flavor = Domain::Material::TextureExportFlavor;
        using Synth = Domain::Material::MaterialTextureSynthesizer;
        Core::Result<QImage> synthRes = Core::Result<QImage>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialConvertWorkflow", "Unsupported texture export flavor"));

        switch (item.flavor) {
        case Flavor::OpaqueColor:
        case Flavor::NormalDirectX:
        case Flavor::LinearPassThrough:
            synthRes = Synth::exportOpaqueColor(srcImg, m_context.token());
            break;

        case Flavor::StraightRgba:
            synthRes = Synth::exportStraightRgba(srcImg, m_context.token());
            break;

        case Flavor::AlphaMask:
            synthRes = Synth::extractAlphaMask(srcImg, m_context.token());
            break;

        case Flavor::SsbumpToNormal:
            synthRes = Synth::convertSsbumpToNormal(srcImg, m_context.token());
            break;

        case Flavor::RoughnessFromSpecMask:
            synthRes = Synth::synthesizeRoughnessFromSpecMask(
                srcImg, item.sourceChannel, item.envmapTint, item.isMetallic, m_context.token());
            break;

        case Flavor::RoughnessFromPhongExp:
            synthRes = Synth::synthesizeRoughnessFromPhongExponentTexture(srcImg, m_context.token());
            break;

        case Flavor::MetalnessFromSpecMask:
            synthRes = Synth::synthesizeMetalnessFromSpecMask(
                srcImg, item.sourceChannel, item.envmapTint, m_context.token());
            break;

        case Flavor::ChannelMask:
            synthRes = Synth::extractChannelMask(srcImg, item.sourceChannel, m_context.token());
            break;

        case Flavor::LuminanceMask:
            synthRes = Synth::extractLuminanceMask(srcImg, m_context.token());
            break;

        case Flavor::LinearTintedColor: {
            const std::array<float, 3> tint = item.linearTint.value_or(std::array<float, 3>{1.0f, 1.0f, 1.0f});
            synthRes = Synth::bakeLinearTint(srcImg, tint, false, m_context.token());
            break;
        }

        case Flavor::TwoTextureCombine: {
            auto secRes = decodeVtfFrame0(item.secondaryVtfPath);
            if (secRes.isCancelled()) {
                return Core::Result<QString>::cancelled(secRes.message());
            }
            if (secRes.isFailure()) {
                return Core::Result<QString>::failure(secRes.error(), secRes.message());
            }
            auto combRes = Synth::combineTwoTextures(
                srcImg,
                secRes.value(),
                toUvTransform2D(item.secondaryTransform),
                m_context.token());
            if (combRes.isCancelled()) {
                return Core::Result<QString>::cancelled(combRes.message());
            }
            if (combRes.isFailure()) {
                return Core::Result<QString>::failure(combRes.error(), combRes.message());
            }
            synthRes = Core::Result<QImage>::success(std::move(combRes.value().colorImage));
            break;
        }
        }

        if (synthRes.isCancelled()) {
            return Core::Result<QString>::cancelled(synthRes.message());
        }
        if (synthRes.isFailure()) {
            return Core::Result<QString>::failure(synthRes.error(), synthRes.message());
        }

        QString targetRelPng = item.targetRelativePngPath;
        if (crop.has_value() && crop->isValid()) {
            const QString cropSuffix = QStringLiteral("_subrect_%1_%2_%3_%4")
                .arg(crop->x).arg(crop->y).arg(crop->width).arg(crop->height);
            targetRelPng = insertSuffixBeforeExtension(targetRelPng, cropSuffix);
        }

        // Disambiguate if another distinct flavor/parameter variant already claimed the same PNG path.
        const auto existingKeyIt = m_writtenPngKeyByPath.constFind(targetRelPng);
        if (existingKeyIt != m_writtenPngKeyByPath.constEnd() && existingKeyIt.value() != cacheKey) {
            const QByteArray digest = QCryptographicHash::hash(cacheKey.toUtf8(), QCryptographicHash::Sha1).toHex();
            const QString hashSuffix = QStringLiteral("_") + QString::fromLatin1(digest.left(8));
            targetRelPng = insertSuffixBeforeExtension(targetRelPng, hashSuffix);
        }

        auto writeRes = writePngFile(targetRelPng, synthRes.value());
        if (writeRes.isFailure()) {
            return Core::Result<QString>::failure(writeRes.error(), writeRes.message());
        }

        m_exportedTextureCache.insert(cacheKey, targetRelPng);
        m_writtenPngKeyByPath.insert(targetRelPng, cacheKey);
        return Core::Result<QString>::success(targetRelPng);
    }

    Core::Result<void> convertExplicitSpriteParsed(
        const QString& vmtRelPath,
        const Domain::Material::VmtMaterial& vmt)
    {
        const QString baseVtf = vmt.normalizedTexturePath(QStringLiteral("$basetexture"));
        if (baseVtf.isEmpty()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Explicit sprite material '%1' is missing $basetexture").arg(vmtRelPath);
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        auto vtfLoadedRes = locateAndReadAsset(vtfAssetRelativePath(baseVtf), /*checkCs2Native=*/false);
        if (vtfLoadedRes.isCancelled()) {
            return Core::Result<void>::cancelled(vtfLoadedRes.message());
        }
        if (vtfLoadedRes.isFailure() || vtfLoadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to locate VTF '%1' for explicit sprite '%2'")
                .arg(vtfAssetRelativePath(baseVtf), vmtRelPath);
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        const QByteArray& vtfBytes = vtfLoadedRes.value().bytes;
        const std::span<const std::byte> vtfSpan(
            reinterpret_cast<const std::byte*>(vtfBytes.constData()),
            static_cast<std::size_t>(vtfBytes.size()));

        auto headerRes = Domain::Material::VtfCodec::readHeaderFromMemory(vtfSpan, baseVtf);
        if (headerRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to read VTF header '%1' for sprite '%2': %3")
                .arg(baseVtf, vmtRelPath, headerRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        const Domain::Material::VtfHeaderInfo header = headerRes.value();
        const QString stemRelPath = vmtStemRelativePath(vmtRelPath);
        const QString vtexRelPath = vmtToVtexRelativePath(vmtRelPath);
        const QString stemFileName = QFileInfo(stemRelPath).fileName();

        QString vtexInputRelPath;
        bool hasAlpha = false;

        // Track B.1: Sheet resource (VTF_RSRC_SHEET)
        bool handledAsSheet = false;
        if (header.hasSheetResource) {
            auto sheetRes = Domain::Material::VtfCodec::readSheetFromMemory(vtfSpan, baseVtf);
            if (sheetRes.isSuccess() && sheetRes.value().has_value() && !sheetRes.value()->sequences.empty()) {
                auto frame0Res = Domain::Material::VtfCodec::readSlice(vtfSpan, {}, baseVtf);
                if (frame0Res.isFailure()) {
                    const QString err = QCoreApplication::translate(
                        "MaterialConvertWorkflow",
                        "Failed to decode sheet VTF '%1': %2").arg(baseVtf, frame0Res.message());
                    m_result.errors.append(err);
                    m_result.failedCount += 1;
                    m_context.warning(err);
                    return Core::Result<void>::success();
                }

                auto rgbaRes = Domain::Material::MaterialTextureSynthesizer::exportStraightRgba(
                    frame0Res.value(), m_context.token());
                if (rgbaRes.isCancelled()) {
                    return Core::Result<void>::cancelled(rgbaRes.message());
                }
                if (rgbaRes.isFailure()) {
                    m_result.errors.append(rgbaRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(rgbaRes.message());
                    return Core::Result<void>::success();
                }

                const QImage& rgbaImg = rgbaRes.value();
                hasAlpha = Domain::Material::MaterialTextureSynthesizer::hasNonOpaqueAlpha(rgbaImg);

                const QString sheetPngRel = stemRelPath + QStringLiteral(".png");
                auto pngWriteRes = writePngFile(sheetPngRel, rgbaImg);
                if (pngWriteRes.isFailure()) {
                    m_result.errors.append(pngWriteRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(pngWriteRes.message());
                    return Core::Result<void>::success();
                }

                auto mksRes = Domain::Material::VtexBuilder::buildSheetMks(
                    stemFileName + QStringLiteral(".png"),
                    rgbaImg.width(),
                    rgbaImg.height(),
                    *sheetRes.value());
                if (mksRes.isFailure()) {
                    m_result.errors.append(mksRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(mksRes.message());
                    return Core::Result<void>::success();
                }

                const QString mksRelPath = stemRelPath + QStringLiteral(".mks");
                auto mksWriteRes = writeTextFile(mksRelPath, mksRes.value());
                if (mksWriteRes.isFailure()) {
                    m_result.errors.append(mksWriteRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(mksWriteRes.message());
                    return Core::Result<void>::success();
                }

                vtexInputRelPath = mksRelPath;
                handledAsSheet = true;
            }
        }

        // Track B.2: Multi-frame VTF (frameCount > 1)
        if (!handledAsSheet && header.frameCount > 1) {
            QStringList frameFileNames;
            frameFileNames.reserve(header.frameCount);

            for (std::uint16_t f = 0; f < header.frameCount; ++f) {
                if (m_context.isCancelled()) {
                    return Core::Result<void>::cancelled(
                        QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
                }

                Domain::Material::VtfSliceSelector sel;
                sel.frame = f;
                auto frameRes = Domain::Material::VtfCodec::readSlice(vtfSpan, sel, baseVtf);
                if (frameRes.isFailure()) {
                    const QString err = QCoreApplication::translate(
                        "MaterialConvertWorkflow",
                        "Failed to decode frame %1 of sprite VTF '%2': %3")
                        .arg(f).arg(baseVtf, frameRes.message());
                    m_result.errors.append(err);
                    m_result.failedCount += 1;
                    m_context.warning(err);
                    return Core::Result<void>::success();
                }

                auto rgbaRes = Domain::Material::MaterialTextureSynthesizer::exportStraightRgba(
                    frameRes.value(), m_context.token());
                if (rgbaRes.isCancelled()) {
                    return Core::Result<void>::cancelled(rgbaRes.message());
                }
                if (rgbaRes.isFailure()) {
                    m_result.errors.append(rgbaRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(rgbaRes.message());
                    return Core::Result<void>::success();
                }

                const QImage& rgbaImg = rgbaRes.value();
                if (!hasAlpha && Domain::Material::MaterialTextureSynthesizer::hasNonOpaqueAlpha(rgbaImg)) {
                    hasAlpha = true;
                }

                const QString frameSuffix = QStringLiteral("_%1.png").arg(f, 3, 10, QLatin1Char('0'));
                const QString frameRelPath = stemRelPath + frameSuffix;
                auto pngWriteRes = writePngFile(frameRelPath, rgbaImg);
                if (pngWriteRes.isFailure()) {
                    m_result.errors.append(pngWriteRes.message());
                    m_result.failedCount += 1;
                    m_context.warning(pngWriteRes.message());
                    return Core::Result<void>::success();
                }
                frameFileNames.append(stemFileName + frameSuffix);
            }

            auto mksRes = Domain::Material::VtexBuilder::buildMultiFrameMks(frameFileNames, true);
            if (mksRes.isFailure()) {
                m_result.errors.append(mksRes.message());
                m_result.failedCount += 1;
                m_context.warning(mksRes.message());
                return Core::Result<void>::success();
            }

            const QString mksRelPath = stemRelPath + QStringLiteral(".mks");
            auto mksWriteRes = writeTextFile(mksRelPath, mksRes.value());
            if (mksWriteRes.isFailure()) {
                m_result.errors.append(mksWriteRes.message());
                m_result.failedCount += 1;
                m_context.warning(mksWriteRes.message());
                return Core::Result<void>::success();
            }

            vtexInputRelPath = mksRelPath;
        } else if (!handledAsSheet) {
            // Track B.3: Single-frame sprite
            auto frame0Res = Domain::Material::VtfCodec::readSlice(vtfSpan, {}, baseVtf);
            if (frame0Res.isFailure()) {
                const QString err = QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Failed to decode sprite VTF '%1': %2").arg(baseVtf, frame0Res.message());
                m_result.errors.append(err);
                m_result.failedCount += 1;
                m_context.warning(err);
                return Core::Result<void>::success();
            }

            auto rgbaRes = Domain::Material::MaterialTextureSynthesizer::exportStraightRgba(
                frame0Res.value(), m_context.token());
            if (rgbaRes.isCancelled()) {
                return Core::Result<void>::cancelled(rgbaRes.message());
            }
            if (rgbaRes.isFailure()) {
                m_result.errors.append(rgbaRes.message());
                m_result.failedCount += 1;
                m_context.warning(rgbaRes.message());
                return Core::Result<void>::success();
            }

            const QImage& rgbaImg = rgbaRes.value();
            hasAlpha = Domain::Material::MaterialTextureSynthesizer::hasNonOpaqueAlpha(rgbaImg);

            const QString pngRelPath = stemRelPath + QStringLiteral(".png");
            auto pngWriteRes = writePngFile(pngRelPath, rgbaImg);
            if (pngWriteRes.isFailure()) {
                m_result.errors.append(pngWriteRes.message());
                m_result.failedCount += 1;
                m_context.warning(pngWriteRes.message());
                return Core::Result<void>::success();
            }

            vtexInputRelPath = pngRelPath;
        }

        Domain::Material::VtexBuildRequest vtexReq;
        vtexReq.preset = hasAlpha
            ? Domain::Material::VtexPreset::Sprite2DAlpha
            : Domain::Material::VtexPreset::Sprite2DOpaque;
        vtexReq.inputRelativePath = vtexInputRelPath;
        vtexReq.sourceVtfHeader = header;

        auto vtexRes = Domain::Material::VtexBuilder::buildVtex(vtexReq);
        if (vtexRes.isFailure()) {
            m_result.errors.append(vtexRes.message());
            m_result.failedCount += 1;
            m_context.warning(vtexRes.message());
            return Core::Result<void>::success();
        }

        auto vtexWriteRes = writeTextFile(vtexRelPath, vtexRes.value());
        if (vtexWriteRes.isFailure()) {
            m_result.errors.append(vtexWriteRes.message());
            m_result.failedCount += 1;
            m_context.warning(vtexWriteRes.message());
            return Core::Result<void>::success();
        }

        addCompilableFile(vtexWriteRes.value());
        m_result.convertedSprites += 1;
        return Core::Result<void>::success();
    }

    Core::Result<void> processRegularMaterial(const QString& rawVmtPath)
    {
        if (m_context.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        const QString vmtRelPath = normalizeMaterialRelativePath(rawVmtPath);
        if (vmtRelPath.isEmpty()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Empty material relative path specified");
            m_result.errors.append(err);
            m_result.failedCount += 1;
            return Core::Result<void>::success();
        }

        auto loadedRes = locateAndReadAsset(vmtRelPath, /*checkCs2Native=*/true);
        if (loadedRes.isCancelled()) {
            return Core::Result<void>::cancelled(loadedRes.message());
        }
        if (loadedRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to read material '%1': %2").arg(vmtRelPath, loadedRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        if (loadedRes.value().kind == Domain::Asset::AssetMatchKind::NativeCs2) {
            m_result.skippedNative += 1;
            return Core::Result<void>::success();
        }
        if (loadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Material VMT not found: %1").arg(vmtRelPath);
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        auto parseRes = Domain::Material::VmtParser::parse(loadedRes.value().bytes, vmtRelPath);
        if (parseRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to parse VMT '%1': %2").arg(vmtRelPath, parseRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        Domain::Material::VmtMaterial effectiveVmt = std::move(parseRes.value());
        m_result.warnings.append(effectiveVmt.warnings);

        // Expand Subrect ($material + $pos/$size crop)
        std::optional<SubrectCrop> subrectCrop;
        if (effectiveVmt.normalizedShaderName == QStringLiteral("subrect")) {
            QSet<QString> visited;
            visited.insert(vmtRelPath);

            int depth = 0;
            constexpr int kMaxSubrectDepth = 4;
            while (effectiveVmt.normalizedShaderName == QStringLiteral("subrect") && depth < kMaxSubrectDepth) {
                ++depth;
                const auto posOpt = effectiveVmt.vec2Param(QStringLiteral("$pos"));
                const auto sizeOpt = effectiveVmt.vec2Param(QStringLiteral("$size"));
                if (!subrectCrop.has_value() && sizeOpt.has_value()) {
                    SubrectCrop c;
                    c.x = posOpt.has_value() ? static_cast<int>(std::lround((*posOpt)[0])) : 0;
                    c.y = posOpt.has_value() ? static_cast<int>(std::lround((*posOpt)[1])) : 0;
                    c.width = static_cast<int>(std::lround((*sizeOpt)[0]));
                    c.height = static_cast<int>(std::lround((*sizeOpt)[1]));
                    if (c.isValid()) {
                        subrectCrop = c;
                    }
                }

                const QString parentRaw = effectiveVmt.stringParam(QStringLiteral("$material"));
                const QString parentVmtRel = normalizeMaterialRelativePath(parentRaw);
                if (parentVmtRel.isEmpty() || visited.contains(parentVmtRel)) {
                    const QString warn = QCoreApplication::translate(
                        "MaterialConvertWorkflow",
                        "Subrect '%1' has invalid or cyclic $material reference: %2")
                        .arg(vmtRelPath, parentRaw);
                    m_result.warnings.append(warn);
                    m_result.skippedUnsupported += 1;
                    return Core::Result<void>::success();
                }
                visited.insert(parentVmtRel);

                auto parentLoadedRes = locateAndReadAsset(parentVmtRel, /*checkCs2Native=*/false);
                if (parentLoadedRes.isCancelled()) {
                    return Core::Result<void>::cancelled(parentLoadedRes.message());
                }
                if (parentLoadedRes.isFailure()
                    || parentLoadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
                    const QString err = QCoreApplication::translate(
                        "MaterialConvertWorkflow",
                        "Subrect '%1' parent material not found: %2")
                        .arg(vmtRelPath, parentVmtRel);
                    m_result.errors.append(err);
                    m_result.failedCount += 1;
                    m_context.warning(err);
                    return Core::Result<void>::success();
                }

                auto parentParseRes = Domain::Material::VmtParser::parse(parentLoadedRes.value().bytes, vmtRelPath);
                if (parentParseRes.isFailure()) {
                    const QString err = QCoreApplication::translate(
                        "MaterialConvertWorkflow",
                        "Failed to parse parent VMT '%1' for subrect '%2': %3")
                        .arg(parentVmtRel, vmtRelPath, parentParseRes.message());
                    m_result.errors.append(err);
                    m_result.failedCount += 1;
                    m_context.warning(err);
                    return Core::Result<void>::success();
                }

                effectiveVmt = std::move(parentParseRes.value());
                effectiveVmt.relativePath = vmtRelPath;
                m_result.warnings.append(effectiveVmt.warnings);
            }
        }

        // Probe referenced VTF headers and alpha metadata
        QHash<QString, Domain::Material::VtfProbeInfo> vtfProbes;
        const QString baseVtf = effectiveVmt.normalizedTexturePath(QStringLiteral("$basetexture"));
        if (!baseVtf.isEmpty()) {
            auto probeRes = probeVtf(baseVtf, subrectCrop, vtfProbes);
            if (probeRes.isCancelled()) {
                return Core::Result<void>::cancelled(probeRes.message());
            }
        }
        const QStringList extraBaseKeys = {
            QStringLiteral("$basetexture2"),
            QStringLiteral("$basetexture3"),
            QStringLiteral("$basetexture4")
        };
        for (const QString& key : extraBaseKeys) {
            const QString texPath = effectiveVmt.normalizedTexturePath(key);
            if (!texPath.isEmpty()) {
                auto probeRes = probeVtf(texPath, std::nullopt, vtfProbes);
                if (probeRes.isCancelled()) {
                    return Core::Result<void>::cancelled(probeRes.message());
                }
            }
        }

        // Plan translation
        const Domain::Material::MaterialPlan plan = Domain::Material::MaterialTranslator::plan(
            effectiveVmt,
            vtfProbes,
            m_options.translatePolicy,
            Domain::Material::MaterialUsage::WorldSurface);

        m_result.warnings.append(plan.warnings);

        if (plan.targetShader == Domain::Material::TargetShader::Skip) {
            m_result.skippedUnsupported += 1;
            return Core::Result<void>::success();
        }
        if (plan.targetShader == Domain::Material::TargetShader::ExplicitVtexOnly) {
            return convertExplicitSpriteParsed(vmtRelPath, effectiveVmt);
        }

        // Synthesize and export all planned PNG textures (regular .vmat NEVER emits .mks)
        QHash<QString, QString> exportedPaths;
        for (const auto& item : plan.textureExports) {
            if (m_context.isCancelled()) {
                return Core::Result<void>::cancelled(
                    QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
            }

            const std::optional<SubrectCrop> itemCrop =
                (subrectCrop.has_value() && item.sourceVtfPath == baseVtf) ? subrectCrop : std::nullopt;

            auto exportRes = exportPlannedTexture(item, itemCrop);
            if (exportRes.isCancelled()) {
                return Core::Result<void>::cancelled(exportRes.message());
            }
            if (exportRes.isFailure()) {
                const QString warn = QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Failed to export texture '%1' (role '%2') for '%3': %4")
                    .arg(item.sourceVtfPath, item.role, vmtRelPath, exportRes.message());
                m_result.warnings.append(warn);
                m_context.warning(warn);
                continue;
            }
            exportedPaths.insert(item.role, exportRes.value());
        }

        // Build and write .vmat
        auto vmatRes = Domain::Material::MaterialTranslator::buildVmat(
            effectiveVmt, plan, exportedPaths, m_options.translatePolicy);
        if (vmatRes.isSkipped()) {
            m_result.skippedUnsupported += 1;
            return Core::Result<void>::success();
        }
        if (vmatRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to build .vmat for '%1': %2").arg(vmtRelPath, vmatRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        const QString vmatRelPath = vmtToVmatRelativePath(vmtRelPath);
        auto writeRes = writeTextFile(vmatRelPath, vmatRes.value().vmatContent);
        if (writeRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to write .vmat '%1': %2").arg(vmatRelPath, writeRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        addCompilableFile(writeRes.value());
        m_result.convertedMaterials += 1;
        return Core::Result<void>::success();
    }

    Core::Result<void> processExplicitSprite(const QString& rawVmtPath)
    {
        if (m_context.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        const QString vmtRelPath = normalizeMaterialRelativePath(rawVmtPath);
        if (vmtRelPath.isEmpty()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Empty explicit sprite VMT path specified");
            m_result.errors.append(err);
            m_result.failedCount += 1;
            return Core::Result<void>::success();
        }

        auto loadedRes = locateAndReadAsset(vmtRelPath, /*checkCs2Native=*/true);
        if (loadedRes.isCancelled()) {
            return Core::Result<void>::cancelled(loadedRes.message());
        }
        if (loadedRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to read sprite VMT '%1': %2").arg(vmtRelPath, loadedRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        if (loadedRes.value().kind == Domain::Asset::AssetMatchKind::NativeCs2) {
            m_result.skippedNative += 1;
            return Core::Result<void>::success();
        }
        if (loadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Explicit sprite VMT not found: %1").arg(vmtRelPath);
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        auto parseRes = Domain::Material::VmtParser::parse(loadedRes.value().bytes, vmtRelPath);
        if (parseRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to parse sprite VMT '%1': %2").arg(vmtRelPath, parseRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        m_result.warnings.append(parseRes.value().warnings);
        return convertExplicitSpriteParsed(vmtRelPath, parseRes.value());
    }

    Core::Result<void> processSkybox(const QString& rawSkyName)
    {
        if (m_context.isCancelled()) {
            return Core::Result<void>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        const QString skyName = normalizeSkyboxName(rawSkyName);
        if (skyName.isEmpty()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Empty skybox name specified");
            m_result.errors.append(err);
            m_result.failedCount += 1;
            return Core::Result<void>::success();
        }

        const QString skyVmatRel = QStringLiteral("materials/skybox/%1.vmat").arg(skyName);
        if (!m_options.overwriteNativeAssets && m_options.cs2Index) {
            const QString skyVmtProbe = QStringLiteral("materials/skybox/%1.vmt").arg(skyName);
            if (m_options.cs2Index->hasCs2NativeAsset(skyVmtProbe)
                || m_options.cs2Index->hasCs2NativeAsset(skyVmatRel)) {
                m_result.skippedNative += 1;
                return Core::Result<void>::success();
            }
        }

        static constexpr std::array<Domain::Material::CubeFace, Domain::Material::CubeFaceCount> kFaces = {
            Domain::Material::CubeFace::Front,
            Domain::Material::CubeFace::Back,
            Domain::Material::CubeFace::Left,
            Domain::Material::CubeFace::Right,
            Domain::Material::CubeFace::Up,
            Domain::Material::CubeFace::Down,
        };

        Domain::Material::SkyboxFaces faces;

        for (Domain::Material::CubeFace face : kFaces) {
            if (m_context.isCancelled()) {
                return Core::Result<void>::cancelled(
                    QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
            }

            const QString faceSuffix = Domain::Material::cubeFaceSuffix(face);
            const QString faceVmtRel = QStringLiteral("materials/skybox/%1%2.vmt").arg(skyName, faceSuffix);

            auto vmtLoadedRes = locateAndReadAsset(faceVmtRel, /*checkCs2Native=*/false);
            if (vmtLoadedRes.isCancelled()) {
                return Core::Result<void>::cancelled(vmtLoadedRes.message());
            }
            if (vmtLoadedRes.isFailure() || vmtLoadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
                const QString warn = QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Skybox '%1' is missing face VMT: %2").arg(skyName, faceVmtRel);
                m_result.warnings.append(warn);
                m_context.warning(warn);
                continue;
            }

            auto parseRes = Domain::Material::VmtParser::parse(vmtLoadedRes.value().bytes, faceVmtRel);
            if (parseRes.isFailure()) {
                const QString warn = QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Failed to parse skybox face VMT '%1': %2").arg(faceVmtRel, parseRes.message());
                m_result.warnings.append(warn);
                m_context.warning(warn);
                continue;
            }

            const Domain::Material::VmtMaterial& faceVmt = parseRes.value();
            Domain::Material::SkyboxFaceTransform faceTransform;
            if (const auto colorOpt = faceVmt.colorParam(QStringLiteral("$color")); colorOpt.has_value()) {
                faceTransform.colorScale = {colorOpt->r, colorOpt->g, colorOpt->b};
            }
            if (const auto transformOpt = faceVmt.transformParam(QStringLiteral("$basetexturetransform"));
                transformOpt.has_value()) {
                faceTransform.uvCenter = transformOpt->center;
                faceTransform.uvScale = transformOpt->scale;
                faceTransform.uvRotateDeg = transformOpt->rotateDeg;
                faceTransform.uvTranslate = transformOpt->translate;
            }

            struct SkyTexCandidate {
                QString path;
                bool isCompressedHdr = false;
                bool isHdrBase = false;
            };
            std::vector<SkyTexCandidate> candidates;
            if (const QString p = faceVmt.normalizedTexturePath(QStringLiteral("$hdrcompressedtexture")); !p.isEmpty()) {
                candidates.push_back({p, true, false});
            }
            if (const QString p = faceVmt.normalizedTexturePath(QStringLiteral("$hdrbasetexture")); !p.isEmpty()) {
                candidates.push_back({p, false, true});
            }
            if (const QString p = faceVmt.normalizedTexturePath(QStringLiteral("$basetexture")); !p.isEmpty()) {
                candidates.push_back({p, false, false});
            }

            QImage decodedFace;
            for (const auto& cand : candidates) {
                auto vtfLoadedRes = locateAndReadAsset(vtfAssetRelativePath(cand.path), /*checkCs2Native=*/false);
                if (vtfLoadedRes.isCancelled()) {
                    return Core::Result<void>::cancelled(vtfLoadedRes.message());
                }
                if (vtfLoadedRes.isFailure() || vtfLoadedRes.value().kind == Domain::Asset::AssetMatchKind::NotFound) {
                    continue;
                }

                const QByteArray& vtfBytes = vtfLoadedRes.value().bytes;
                const std::span<const std::byte> vtfSpan(
                    reinterpret_cast<const std::byte*>(vtfBytes.constData()),
                    static_cast<std::size_t>(vtfBytes.size()));

                Domain::Material::VtfHeaderInfo headerInfo;
                auto headerRes = Domain::Material::VtfCodec::readHeaderFromMemory(vtfSpan, cand.path);
                if (headerRes.isSuccess()) {
                    headerInfo = headerRes.value();
                }

                const bool useHdrPath = cand.isCompressedHdr || cand.isHdrBase || headerInfo.isHdr;
                if (useHdrPath) {
                    Domain::Material::VtfSliceSelector sel;
                    sel.treatAsCompressedHdr = cand.isCompressedHdr;
                    auto floatRes = Domain::Material::VtfCodec::readFloatSlice(vtfSpan, sel, cand.path);
                    if (floatRes.isFailure()) {
                        continue;
                    }
                    auto preRes = Domain::Material::SkyboxCubeBuilder::preprocessHdrFace(
                        floatRes.value(), faceTransform, 0);
                    if (preRes.isSuccess()) {
                        decodedFace = std::move(preRes.value());
                        break;
                    }
                } else {
                    auto ldrRes = Domain::Material::VtfCodec::readSlice(vtfSpan, {}, cand.path);
                    if (ldrRes.isFailure()) {
                        continue;
                    }
                    auto preRes = Domain::Material::SkyboxCubeBuilder::preprocessFace(
                        ldrRes.value(), faceTransform, 0);
                    if (preRes.isSuccess()) {
                        decodedFace = std::move(preRes.value());
                        break;
                    }
                }
            }

            if (decodedFace.isNull()) {
                const QString warn = QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Failed to decode any texture for skybox face '%1'").arg(faceVmtRel);
                m_result.warnings.append(warn);
                m_context.warning(warn);
                continue;
            }

            faces.setFace(face, std::move(decodedFace));
        }

        if (!faces.hasAnyFace()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "No valid skybox faces were found for skybox '%1'").arg(skyName);
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        const QString skyPngRel = QStringLiteral("materials/skybox/%1cube.png").arg(skyName);
        Domain::Material::SkyboxBuildOptions skyOpts;
        skyOpts.skyboxName = skyName;
        skyOpts.skyTextureRelativePath = skyPngRel;
        skyOpts.allowNonSquareFaces = true;

        auto buildRes = Domain::Material::SkyboxCubeBuilder::build(faces, skyOpts);
        if (buildRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to build skybox cubemap '%1': %2").arg(skyName, buildRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        auto pngWriteRes = writePngFile(skyPngRel, buildRes.value().cubeImage);
        if (pngWriteRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to write skybox texture '%1': %2").arg(skyPngRel, pngWriteRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        auto vmatWriteRes = writeTextFile(skyVmatRel, buildRes.value().vmatContent);
        if (vmatWriteRes.isFailure()) {
            const QString err = QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Failed to write skybox .vmat '%1': %2").arg(skyVmatRel, vmatWriteRes.message());
            m_result.errors.append(err);
            m_result.failedCount += 1;
            m_context.warning(err);
            return Core::Result<void>::success();
        }

        addCompilableFile(vmatWriteRes.value());
        m_result.convertedSkyboxes += 1;
        return Core::Result<void>::success();
    }

    const QStringList& newlyCreatedFiles() const noexcept { return m_newlyCreatedFiles; }
    const QStringList& compilableFiles() const noexcept { return m_compilableFiles; }

private:
    void recordGeneratedFile(const QString& fullPath, bool isNewlyCreated)
    {
        if (!m_generatedFileSet.contains(fullPath)) {
            m_generatedFileSet.insert(fullPath);
            m_result.generatedFiles.append(fullPath);
        }
        if (isNewlyCreated && !m_newlyCreatedFileSet.contains(fullPath)) {
            m_newlyCreatedFileSet.insert(fullPath);
            m_newlyCreatedFiles.append(fullPath);
        }
    }

    void addCompilableFile(const QString& fullPath)
    {
        if (!m_compilableFileSet.contains(fullPath)) {
            m_compilableFileSet.insert(fullPath);
            m_compilableFiles.append(fullPath);
        }
    }

    const MaterialConvertOptions& m_options;
    Domain::Package::PackArchivePool& m_pool;
    const Common::ImportContext& m_context;
    MaterialConvertResult& m_result;

    QHash<QString, Domain::Material::VtfProbeInfo> m_vtfProbeCache;
    QHash<QString, QString> m_exportedTextureCache;
    QHash<QString, QString> m_writtenPngKeyByPath;

    QSet<QString> m_generatedFileSet;
    QSet<QString> m_newlyCreatedFileSet;
    QStringList m_newlyCreatedFiles;
    QSet<QString> m_compilableFileSet;
    QStringList m_compilableFiles;
};

} // namespace

Core::Result<MaterialConvertResult> MaterialConvertWorkflow::execute(
    const MaterialConvertOptions& options,
    const Common::ImportContext& context)
{
    if (auto cancelCheck = context.checkCancelled(
            QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        cancelCheck.isCancelled()) {
        return Core::Result<MaterialConvertResult>::cancelled(cancelCheck.message());
    }

    if (options.destContentDir.isEmpty()) {
        return Core::Result<MaterialConvertResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialConvertWorkflow", "Destination content directory is empty"));
    }

    if (options.compileResources && options.cs2BaseDir.isEmpty()) {
        return Core::Result<MaterialConvertResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialConvertWorkflow", "CS2 base directory is required when compiling resources"));
    }

    const int totalRequested = options.materialRelativePaths.size()
        + options.explicitSpriteVmtPaths.size()
        + options.skyboxNames.size();
    if (totalRequested == 0) {
        return Core::Result<MaterialConvertResult>::failure(
            Core::Error::ErrorCode::InvalidArgument,
            QCoreApplication::translate("MaterialConvertWorkflow", "No materials, sprites, or skyboxes were specified for conversion"));
    }

    // Ensure destination content directory exists before Tier 2 handle validation.
    Core::Error::ExecutionContext destDirCtx{
        .stage = QStringLiteral("Creating destination content directory"),
        .targetPath = options.destContentDir.toString()
    };
    auto destDirRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
        Core::FileSystem::FileSystem::createDirectory(options.destContentDir.toString());
        return Core::Result<void>::success();
    }, destDirCtx);
    if (destDirRes.isFailure()) {
        return Core::Result<MaterialConvertResult>::failure(destDirRes.error(), destDirRes.message());
    }

    Domain::Package::PackArchivePool localPool;
    Domain::Package::PackArchivePool& activePool = options.archivePool ? *options.archivePool : localPool;

    MaterialConvertResult result;
    result.totalRequested = totalRequested;

    MaterialWorkflowSession session(options, activePool, context, result);

    const double conversionWeight = options.compileResources ? 0.75 : 1.0;
    int processedCount = 0;

    auto rollbackIfEnabled = [&]() {
        if (options.cleanupOnFailure) {
            cleanupNewlyCreatedFiles(session.newlyCreatedFiles(), options.destContentDir, context);
        }
    };

    context.info(QCoreApplication::translate(
        "MaterialConvertWorkflow",
        "Starting material conversion (%1 material(s), %2 sprite(s), %3 skybox(es))")
        .arg(options.materialRelativePaths.size())
        .arg(options.explicitSpriteVmtPaths.size())
        .arg(options.skyboxNames.size()));

    // Track A: Regular VMT materials
    for (const QString& vmtPath : options.materialRelativePaths) {
        if (context.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        auto itemRes = session.processRegularMaterial(vmtPath);
        if (itemRes.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(itemRes.message());
        }

        ++processedCount;
        context.updateProgress(conversionWeight * (static_cast<double>(processedCount) / totalRequested));
    }

    // Track B: Explicit Sprite VMTs -> .vtex (+ .mks when multi-frame or sheet)
    for (const QString& spriteVmtPath : options.explicitSpriteVmtPaths) {
        if (context.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        auto itemRes = session.processExplicitSprite(spriteVmtPath);
        if (itemRes.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(itemRes.message());
        }

        ++processedCount;
        context.updateProgress(conversionWeight * (static_cast<double>(processedCount) / totalRequested));
    }

    // Track C: Skyboxes -> 4x3 .png + sky.vfx .vmat
    for (const QString& skyboxName : options.skyboxNames) {
        if (context.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(
                QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
        }

        auto itemRes = session.processSkybox(skyboxName);
        if (itemRes.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(itemRes.message());
        }

        ++processedCount;
        context.updateProgress(conversionWeight * (static_cast<double>(processedCount) / totalRequested));
    }

    if (context.isCancelled()) {
        rollbackIfEnabled();
        return Core::Result<MaterialConvertResult>::cancelled(
            QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion was cancelled"));
    }

    // Single batch compilation via ResourceCompilerTool
    const QStringList& compilableFiles = session.compilableFiles();
    if (options.compileResources && !compilableFiles.isEmpty()) {
        const Core::Path::FilesystemPath rcExe =
            Domain::Tool::Cs2PathLayout::resourceCompilerExecutable(options.cs2BaseDir);

        auto compileRes = context.runStep(
            QCoreApplication::translate(
                "MaterialConvertWorkflow",
                "Compiling %1 generated material/texture resource(s)")
                .arg(compilableFiles.size()),
            1.0,
            [&]() -> Core::Result<Domain::Tool::ResourceCompilerToolResult> {
                Domain::Tool::ResourceCompilerOptions rcOptions;
                rcOptions.gameDir = Domain::Tool::Cs2PathLayout::gameDirectory(options.cs2BaseDir);
                rcOptions.inputFiles = compilableFiles;
                rcOptions.forceCompile = true;
                rcOptions.verbose = true;
                rcOptions.cancellationToken = context.token();

                return Domain::Tool::ResourceCompilerTool::compileResources(
                    rcExe, rcOptions, context.loggingContext());
            });

        if (compileRes.isCancelled()) {
            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::cancelled(compileRes.message());
        }

        if (compileRes.isFailure()) {
            const auto& rcData = compileRes.valueOr(Domain::Tool::ResourceCompilerToolResult{});
            result.compiledAssetPaths = rcData.compiledAssetPaths;
            result.compiledCount = !rcData.compiledAssetPaths.isEmpty()
                ? rcData.compiledAssetPaths.size()
                : rcData.compiledCount;
            result.failedCount += rcData.failedCount;
            result.errors.append(rcData.compileErrors);
            if (rcData.compileErrors.isEmpty()) {
                result.errors.append(compileRes.message());
            }

            rollbackIfEnabled();
            return Core::Result<MaterialConvertResult>::failure(
                compileRes.error(),
                QCoreApplication::translate(
                    "MaterialConvertWorkflow",
                    "Resource compilation failed: %1").arg(compileRes.message()),
                result);
        }

        const auto& rcData = compileRes.value();
        result.compiledAssetPaths = rcData.compiledAssetPaths;
        result.compiledCount = !rcData.compiledAssetPaths.isEmpty()
            ? rcData.compiledAssetPaths.size()
            : rcData.compiledCount;
        result.failedCount += rcData.failedCount;
        result.errors.append(rcData.compileErrors);
    }

    context.updateProgress(1.0);
    context.info(QCoreApplication::translate(
        "MaterialConvertWorkflow",
        "Material conversion completed: %1 material(s), %2 sprite(s), %3 skybox(es), %4 skipped native, %5 skipped unsupported, %6 failed, %7 compiled")
        .arg(result.convertedMaterials)
        .arg(result.convertedSprites)
        .arg(result.convertedSkyboxes)
        .arg(result.skippedNative)
        .arg(result.skippedUnsupported)
        .arg(result.failedCount)
        .arg(result.compiledCount));

    return Core::Result<MaterialConvertResult>::success(
        std::move(result),
        QCoreApplication::translate("MaterialConvertWorkflow", "Material conversion completed"));
}

} // namespace Workflow::Material
