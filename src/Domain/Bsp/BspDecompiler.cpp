#include "Domain/Bsp/BspDecompiler.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <optional>

#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Domain/Bsp/Internal/BspDecompilerModules.h"
#include "Domain/Bsp/Internal/BspFileReader.h"
#include "Domain/Bsp/Internal/BspMappers.h"
#include "Domain/Bsp/Internal/BspProtection.h"
#include "Domain/Bsp/Internal/BspStructs.h"
#include "Domain/Bsp/Internal/SourceAppDb.h"
#include "Domain/Bsp/Internal/TextureSource.h"
#include "Domain/Bsp/Internal/VmfWriter.h"
#include "Domain/Bsp/Internal/Winding.h"
#include "Domain/Package/BspPackExtractor.h"

namespace Domain::Bsp {

namespace {

Core::Path::FilesystemPath replaceExtension(
    const Core::Path::FilesystemPath& path,
    const QString& newExt)
{
    const QFileInfo info(path.toString());
    const QString baseName = info.completeBaseName();
    return path.parentPath().join(baseName + newExt);
}

Core::Result<QByteArray> decompileInternal(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath* outputVmfPath,
    const BspDecompileConfig& config,
    const Core::Path::FilesystemPath& expectedBaseDir,
    BspDecompileResult* outMetadata)
{
    if (config.token.isCancelled()) {
        return Core::Result<QByteArray>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    if (bspPath.isEmpty() || !bspPath.isValid()) {
        return Core::Result<QByteArray>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspDecompiler", "Input BSP path is empty or invalid"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Decompiling BSP"),
        .resourcePath = bspPath.toString(),
        .targetPath = outputVmfPath != nullptr ? outputVmfPath->toString() : QString{}
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<QByteArray> {
        config.log(BspLogLevel::Info, QStringLiteral("Loading %1").arg(bspPath.toString()));

        auto bspRes = Internal::BspFile::open(
            bspPath,
            config.defaultAppId,
            config.loadLumpFiles,
            config);
        if (bspRes.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(bspRes.message());
        }
        if (bspRes.isFailure()) {
            return Core::Result<QByteArray>::failure(bspRes.error(), bspRes.message());
        }

        Internal::BspFile bspFile = std::move(bspRes.value());

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        BspDecompileResult metadata;
        if (outputVmfPath != nullptr) {
            metadata.outputVmfPath = *outputVmfPath;
        }

        if (config.unpackEmbedded) {
            Core::Path::FilesystemPath unpackDir = config.embeddedUnpackDir;
            if (unpackDir.isEmpty()) {
                if (outputVmfPath != nullptr && !outputVmfPath->isEmpty()) {
                    unpackDir = replaceExtension(*outputVmfPath, QString{});
                } else {
                    unpackDir = replaceExtension(bspPath, QString{});
                }
            }

            Domain::Package::BspExtractOptions extractOptions{
                .token = config.token,
                .smartUnpack = config.smartUnpack,
            };

            auto extractRes = Domain::Package::BspPackExtractor::extractAll(
                bspPath,
                unpackDir,
                extractOptions);
            if (extractRes.isCancelled()) {
                return Core::Result<QByteArray>::cancelled(extractRes.message());
            }
            if (extractRes.isFailure()) {
                return Core::Result<QByteArray>::failure(extractRes.error(), extractRes.message());
            }
            metadata.extractedEmbeddedFiles = extractRes.valueOr(0);
        }

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        Internal::BspFileReader reader(bspFile, config);
        reader.loadAll();

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        std::optional<Internal::NmoData> nmoData;
        if (reader.getBspFile().appId() == Internal::SourceAppId::kNoMoreRoomInHell) {
            const Core::Path::FilesystemPath nmoPath = !config.nmoFilePath.isEmpty()
                ? config.nmoFilePath
                : replaceExtension(bspPath, QStringLiteral(".nmo"));

            if (nmoPath.exists()) {
                auto nmoRes = Internal::NmoFileParser::load(nmoPath);
                if (nmoRes.isFailure()) {
                    return Core::Result<QByteArray>::failure(nmoRes.error(), nmoRes.message());
                }
                nmoData = std::move(nmoRes.value());

                if (outputVmfPath != nullptr && !outputVmfPath->isEmpty() && !config.nullOutput) {
                    const Core::Path::FilesystemPath nmosPath =
                        replaceExtension(*outputVmfPath, QStringLiteral(".nmos"));
                    auto nmosRes = Internal::NmoFileParser::writeAsNmos(
                        *nmoData,
                        nmosPath,
                        expectedBaseDir);
                    if (nmosRes.isFailure()) {
                        return Core::Result<QByteArray>::failure(nmosRes.error(), nmosRes.message());
                    }
                }
            } else {
                config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("Missing .nmo file! If the bsp is for the objective game mode, its objectives will be missing."));
            }
        }

        const int appId = reader.getBspFile().appId();
        const QString gameName = Internal::SourceAppDb::instance()
            .getName(appId)
            .value_or(QString::number(appId));

        metadata.bspVersion = reader.getBspFile().version();
        metadata.detectedAppId = appId;
        metadata.detectedGameName = gameName;

        if (!config.debug) {
            config.log(BspLogLevel::Info, QStringLiteral("BSP version: %1").arg(metadata.bspVersion));
            config.log(BspLogLevel::Info, QStringLiteral("Game: %1").arg(gameName));
        }

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        Internal::VmfWriter writer(
            config.vmfDoubleScale,
            config.vmfDoubleScaleTextureAxes,
            config.vmfDoubleScaleTextureScale,
            config.nullOutput,
            &config);

        Internal::WindingFactory windingFactory = Internal::WindingFactory::forAppId(appId);
        Internal::BrushBounds brushBounds(windingFactory);

        const auto apReallocationData = Internal::AreaportalMapper::createReallocationData(
            reader.getData(), config, windingFactory);
        const auto occReallocationData = Internal::OccluderMapper::createReallocationData(
            reader.getData(), config, windingFactory);

        Internal::TextureSource texsrc(reader, config);
        Internal::BspProtection bspprot(reader, brushBounds, texsrc, config.fixCubemapTextures, config);
        Internal::VmfMeta vmfmeta(reader, writer, config);
        Internal::BrushSideFaceMapper brushSideFaceMapper(reader, windingFactory, config);
        Internal::BrushSource brushsrc(
            reader,
            writer,
            config,
            texsrc,
            bspprot,
            vmfmeta,
            brushSideFaceMapper,
            windingFactory,
            occReallocationData);
        Internal::FaceSource facesrc(
            reader,
            writer,
            config,
            texsrc,
            vmfmeta,
            windingFactory,
            occReallocationData);
        Internal::EntitySource entsrc(
            reader,
            writer,
            config,
            brushsrc,
            facesrc,
            texsrc,
            bspprot,
            vmfmeta,
            brushSideFaceMapper,
            windingFactory,
            brushBounds,
            apReallocationData,
            occReallocationData);

        if (nmoData.has_value()) {
            entsrc.setNmo(*nmoData);
        }

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        if (!config.skipProt) {
            if (bspprot.check()) {
                metadata.isProtected = true;
                metadata.protectionMethods = bspprot.getProtectionMethods();

                config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("%1 contains anti-decompiling flags or is obfuscated!")
                        .arg(reader.getBspFile().name()));
                config.log(BspLogLevel::Warning, QStringLiteral("Detected methods:"));
                for (const QString& method : metadata.protectionMethods) {
                    config.log(BspLogLevel::Warning, method);
                }
            }
        }

        if (config.brushMode == BrushMode::BrushPlanes) {
            brushSideFaceMapper.load();
        }

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        vmfmeta.appendComment(
            QStringLiteral("Decompiled by BSPSource v%1 from %2")
                .arg(BspDecompiler::kVersion, reader.getBspFile().name()));

        vmfmeta.writeWorldHeader();

        if (config.writeWorldBrushes) {
            switch (config.brushMode) {
                case BrushMode::BrushPlanes:
                    brushsrc.writeBrushes();
                    break;
                case BrushMode::OrigFace:
                    facesrc.writeOrigFaces();
                    break;
                case BrushMode::OrigFacePlus:
                    facesrc.writeOrigFacesPlus();
                    break;
                case BrushMode::SplitFace:
                    facesrc.writeFaces();
                    break;
            }

            if (config.brushMode == BrushMode::BrushPlanes) {
                facesrc.writeDispFaces();
            }
        }

        vmfmeta.writeWorldFooter();

        if (config.token.isCancelled()) {
            return Core::Result<QByteArray>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        if (config.isWriteEntities()) {
            entsrc.writeEntities();

            if (config.writeBrushEntities) {
                if (config.writeDetails && config.brushMode == BrushMode::BrushPlanes) {
                    entsrc.writeDetails();
                }
                if (config.writeVisClusters) {
                    entsrc.writeVisClusters();
                }
            }

            if (config.writePointEntities) {
                if (config.writeOverlays) {
                    entsrc.writeOverlays();
                }
                if (config.writeStaticProps) {
                    entsrc.writeStaticProps();
                }
                if (config.writeCubemaps) {
                    entsrc.writeCubemaps();
                }
                if (config.writeLadders && appId != Internal::SourceAppId::kCounterStrikeGo) {
                    entsrc.writeLadders();
                }
            }
        }

        if (config.writeVisgroups) {
            vmfmeta.writeVisgroups();
        }

        if (config.writeCameras) {
            vmfmeta.writeCameras();
        }

        QByteArray buffer = writer.finish();

        if (outMetadata != nullptr) {
            *outMetadata = std::move(metadata);
        }

        config.log(BspLogLevel::Info, QStringLiteral("Finished decompiling %1.").arg(bspPath.toString()));
        return Core::Result<QByteArray>::success(std::move(buffer));
    }, ctx);
}

} // namespace

Core::Result<BspDecompileResult> BspDecompiler::decompile(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath& outputVmfPath,
    const BspDecompileConfig& config,
    const Core::Path::FilesystemPath& expectedBaseDir)
{
    if (config.token.isCancelled()) {
        return Core::Result<BspDecompileResult>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    if (!config.nullOutput && (outputVmfPath.isEmpty() || !outputVmfPath.isValid())) {
        return Core::Result<BspDecompileResult>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspDecompiler", "Output VMF path is empty or invalid"));
    }

    BspDecompileResult metadata;
    auto bufferRes = decompileInternal(bspPath, &outputVmfPath, config, expectedBaseDir, &metadata);
    if (bufferRes.isCancelled()) {
        return Core::Result<BspDecompileResult>::cancelled(bufferRes.message());
    }
    if (bufferRes.isFailure()) {
        return Core::Result<BspDecompileResult>::failure(bufferRes.error(), bufferRes.message());
    }

    if (config.token.isCancelled()) {
        return Core::Result<BspDecompileResult>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    if (!config.nullOutput) {
        Core::Error::ExecutionContext writeCtx{
            .stage = QStringLiteral("Writing decompiled VMF"),
            .resourcePath = bspPath.toString(),
            .targetPath = outputVmfPath.toString()
        };

        auto writeRes = Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
            Core::FileSystem::AtomicFile::writeAtomic(
                outputVmfPath.toString(),
                bufferRes.value(),
                expectedBaseDir);
            return Core::Result<void>::success();
        }, writeCtx);

        if (writeRes.isFailure()) {
            return Core::Result<BspDecompileResult>::failure(writeRes.error(), writeRes.message());
        }
    }

    return Core::Result<BspDecompileResult>::success(std::move(metadata));
}

Core::Result<QByteArray> BspDecompiler::decompileToBuffer(
    const Core::Path::FilesystemPath& bspPath,
    const BspDecompileConfig& config,
    BspDecompileResult* outMetadata)
{
    return decompileInternal(bspPath, nullptr, config, {}, outMetadata);
}

} // namespace Domain::Bsp
