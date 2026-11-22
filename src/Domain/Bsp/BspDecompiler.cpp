#include "Domain/Bsp/BspDecompiler.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <array>
#include <optional>
#include <utility>

#include "Core/Error/ErrorCode.h"
#include "Core/Error/ExecutionGuard.h"
#include "Core/FileSystem/AtomicFile.h"
#include "Core/Temp/TempFile.h"
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

constexpr qint64 kStreamCopyChunkSize = 65536;

Core::Path::FilesystemPath replaceExtension(
    const Core::Path::FilesystemPath& path,
    const QString& newExt)
{
    const QFileInfo info(path.toString());
    const QString baseName = info.completeBaseName();
    return path.parentPath().join(baseName + newExt);
}

void writeVmfHeaderSections(
    const Internal::VmfMeta& vmfmeta,
    Internal::VmfWriter& headerWriter,
    const BspDecompileConfig& config)
{
    vmfmeta.writeVersionInfo(headerWriter);
    if (config.writeVisgroups) {
        vmfmeta.writeVisgroups(headerWriter);
    }
    vmfmeta.writeViewSettings(headerWriter);
}

Core::Result<void> decompileWithWriter(
    Internal::BspFileReader& reader,
    Internal::VmfWriter& bodyWriter,
    Internal::VmfMeta& vmfmeta,
    const BspDecompileConfig& config,
    const std::optional<Internal::NmoData>& nmoData,
    BspDecompileResult& metadata)
{
    const int appId = reader.bspFile().appId();
    Internal::WindingFactory windingFactory = Internal::WindingFactory::forAppId(appId);
    Internal::BrushBounds brushBounds(windingFactory);

    const auto apReallocationData = Internal::AreaportalMapper::createReallocationData(
        reader.data(), config, windingFactory);
    const auto occReallocationData = Internal::OccluderMapper::createReallocationData(
        reader.data(), config, windingFactory);

    Internal::TextureSource texsrc(reader, config);
    Internal::BspProtection bspprot(reader, brushBounds, texsrc, config.fixCubemapTextures, config);
    Internal::BrushSideFaceMapper brushSideFaceMapper(reader, windingFactory, config);
    Internal::BrushSource brushsrc(
        reader,
        bodyWriter,
        config,
        texsrc,
        bspprot,
        vmfmeta,
        brushSideFaceMapper,
        windingFactory,
        occReallocationData);
    Internal::FaceSource facesrc(
        reader,
        bodyWriter,
        config,
        texsrc,
        vmfmeta,
        windingFactory,
        occReallocationData);
    Internal::EntitySource entsrc(
        reader,
        bodyWriter,
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
        return Core::Result<void>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    if (!config.skipProt) {
        if (bspprot.check()) {
            metadata.isProtected = true;
            metadata.protectionMethods = bspprot.protectionMethods();

            config.log(
                BspLogLevel::Warning,
                QStringLiteral("%1 contains anti-decompiling flags or is obfuscated!")
                    .arg(reader.bspFile().name()));
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
        return Core::Result<void>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    vmfmeta.appendComment(
        QStringLiteral("Decompiled by BSPSource v%1 from %2")
            .arg(BspDecompiler::kVersion, reader.bspFile().name()));

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
        return Core::Result<void>::cancelled(
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

    if (config.writeCameras) {
        vmfmeta.writeCameras();
    }
    vmfmeta.writeCordon();

    return Core::Result<void>::success();
}

template <typename ResultT, typename FinalizeFn>
Core::Result<ResultT> runDecompilePipeline(
    const Core::Path::FilesystemPath& bspPath,
    const Core::Path::FilesystemPath* outputVmfPath,
    const BspDecompileConfig& config,
    const Core::Path::FilesystemPath& expectedBaseDir,
    FinalizeFn&& finalize)
{
    if (config.token.isCancelled()) {
        return Core::Result<ResultT>::cancelled(
            QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
    }

    if (bspPath.isEmpty() || !bspPath.isValid()) {
        return Core::Result<ResultT>::failure(
            Core::Error::ErrorCode::InvalidPath,
            QCoreApplication::translate("BspDecompiler", "Input BSP path is empty or invalid"));
    }

    Core::Error::ExecutionContext ctx{
        .stage = QStringLiteral("Decompiling BSP"),
        .resourcePath = bspPath.toString(),
        .targetPath = outputVmfPath != nullptr ? outputVmfPath->toString() : QString{}
    };

    return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<ResultT> {
        config.log(BspLogLevel::Info, QStringLiteral("Loading %1").arg(bspPath.toString()));

        auto bspRes = Internal::BspFile::open(
            bspPath,
            config.defaultAppId,
            config.loadLumpFiles,
            config);
        if (bspRes.isCancelled()) {
            return Core::Result<ResultT>::cancelled(bspRes.message());
        }
        if (bspRes.isFailure()) {
            return Core::Result<ResultT>::failure(bspRes.error(), bspRes.message());
        }

        Internal::BspFile bspFile = std::move(bspRes.value());

        if (config.token.isCancelled()) {
            return Core::Result<ResultT>::cancelled(
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
                return Core::Result<ResultT>::cancelled(extractRes.message());
            }
            if (extractRes.isFailure()) {
                return Core::Result<ResultT>::failure(extractRes.error(), extractRes.message());
            }
            metadata.extractedEmbeddedFiles = extractRes.valueOr(0);
        }

        if (config.token.isCancelled()) {
            return Core::Result<ResultT>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        Internal::BspFileReader reader(bspFile, config);
        reader.loadAll();

        if (config.token.isCancelled()) {
            return Core::Result<ResultT>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        std::optional<Internal::NmoData> nmoData;
        if (reader.bspFile().appId() == Internal::SourceAppId::kNoMoreRoomInHell) {
            const Core::Path::FilesystemPath nmoPath = !config.nmoFilePath.isEmpty()
                ? config.nmoFilePath
                : replaceExtension(bspPath, QStringLiteral(".nmo"));

            if (nmoPath.exists()) {
                auto nmoRes = Internal::NmoFileParser::load(nmoPath);
                if (nmoRes.isFailure()) {
                    return Core::Result<ResultT>::failure(nmoRes.error(), nmoRes.message());
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
                        return Core::Result<ResultT>::failure(nmosRes.error(), nmosRes.message());
                    }
                }
            } else {
                config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("Missing .nmo file! If the bsp is for the objective game mode, its objectives will be missing."));
            }
        }

        const int appId = reader.bspFile().appId();
        const QString gameName = Internal::SourceAppDb::instance()
            .appName(appId)
            .value_or(QString::number(appId));

        metadata.bspVersion = reader.bspFile().version();
        metadata.detectedAppId = appId;
        metadata.detectedGameName = gameName;

        if (!config.debug) {
            config.log(BspLogLevel::Info, QStringLiteral("BSP version: %1").arg(metadata.bspVersion));
            config.log(BspLogLevel::Info, QStringLiteral("Game: %1").arg(gameName));
        }

        if (config.token.isCancelled()) {
            return Core::Result<ResultT>::cancelled(
                QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
        }

        return finalize(reader, nmoData, metadata);
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

    return runDecompilePipeline<BspDecompileResult>(
        bspPath,
        &outputVmfPath,
        config,
        expectedBaseDir,
        [&](Internal::BspFileReader& reader,
            const std::optional<Internal::NmoData>& nmoData,
            BspDecompileResult& metadata) -> Core::Result<BspDecompileResult> {
            if (config.nullOutput) {
                Internal::VmfWriter bodyWriter(
                    config.vmfDoubleScale,
                    config.vmfDoubleScaleTextureAxes,
                    config.vmfDoubleScaleTextureScale,
                    true,
                    &config);
                Internal::VmfMeta vmfmeta(reader, bodyWriter, config);

                auto bodyRes = decompileWithWriter(reader, bodyWriter, vmfmeta, config, nmoData, metadata);
                if (bodyRes.isCancelled()) {
                    return Core::Result<BspDecompileResult>::cancelled(bodyRes.message());
                }
                if (bodyRes.isFailure()) {
                    return Core::Result<BspDecompileResult>::failure(bodyRes.error(), bodyRes.message());
                }

                Internal::VmfWriter headerWriter(
                    config.vmfDoubleScale,
                    config.vmfDoubleScaleTextureAxes,
                    config.vmfDoubleScaleTextureScale,
                    true,
                    &config);
                writeVmfHeaderSections(vmfmeta, headerWriter, config);

                (void)headerWriter.finish();
                (void)bodyWriter.finish();

                config.log(BspLogLevel::Info, QStringLiteral("Finished decompiling %1.").arg(bspPath.toString()));
                return Core::Result<BspDecompileResult>::success(std::move(metadata));
            }

            const Core::Path::FilesystemPath tempBaseDir(QDir::tempPath());
            Core::Temp::TempFile tempFile = Core::Temp::TempFile::create(
                QDir(tempBaseDir.toString()).filePath(QStringLiteral("vmf_body_XXXXXX.tmp")));
            QFile bodyFile(tempFile.path());
            if (!bodyFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
                return Core::Result<BspDecompileResult>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("BspDecompiler", "Failed to open temporary VMF body file: %1")
                        .arg(bodyFile.errorString()));
            }
            if (!Core::Path::FilesystemPath::verifyFileWithinBase(bodyFile, tempBaseDir)) {
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(
                    Core::Error::ErrorCode::InvalidPath,
                    QCoreApplication::translate(
                        "BspDecompiler",
                        "Security boundary violation: Temporary VMF body file escaped system temp directory"));
            }

            Internal::VmfWriter bodyWriter(
                &bodyFile,
                config.vmfDoubleScale,
                config.vmfDoubleScaleTextureAxes,
                config.vmfDoubleScaleTextureScale,
                false,
                &config);
            Internal::VmfMeta vmfmeta(reader, bodyWriter, config);

            auto bodyRes = decompileWithWriter(reader, bodyWriter, vmfmeta, config, nmoData, metadata);
            if (bodyRes.isCancelled()) {
                bodyFile.close();
                return Core::Result<BspDecompileResult>::cancelled(bodyRes.message());
            }
            if (bodyRes.isFailure()) {
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(bodyRes.error(), bodyRes.message());
            }

            auto flushBodyRes = bodyWriter.flush();
            if (flushBodyRes.isFailure()) {
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(flushBodyRes.error(), flushBodyRes.message());
            }

            if (config.token.isCancelled()) {
                bodyFile.close();
                return Core::Result<BspDecompileResult>::cancelled(
                    QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
            }

            Core::FileSystem::AtomicFile atomic(outputVmfPath.toString(), expectedBaseDir);
            atomic.open();
            QIODevice* outDevice = atomic.device();
            if (outDevice == nullptr) {
                atomic.rollback();
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(
                    Core::Error::ErrorCode::WriteFailed,
                    QCoreApplication::translate("BspDecompiler", "Failed to obtain output device for '%1'")
                        .arg(outputVmfPath.toString()));
            }

            Internal::VmfWriter headerWriter(
                outDevice,
                config.vmfDoubleScale,
                config.vmfDoubleScaleTextureAxes,
                config.vmfDoubleScaleTextureScale,
                false,
                &config);
            writeVmfHeaderSections(vmfmeta, headerWriter, config);

            auto flushHeaderRes = headerWriter.flush();
            if (flushHeaderRes.isFailure()) {
                atomic.rollback();
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(flushHeaderRes.error(), flushHeaderRes.message());
            }

            if (!bodyFile.seek(0)) {
                atomic.rollback();
                bodyFile.close();
                return Core::Result<BspDecompileResult>::failure(
                    Core::Error::ErrorCode::ReadFailed,
                    QCoreApplication::translate("BspDecompiler", "Failed to rewind temporary VMF body file: %1")
                        .arg(bodyFile.errorString()));
            }

            std::array<char, kStreamCopyChunkSize> copyBuf;
            while (!bodyFile.atEnd()) {
                if (config.token.isCancelled()) {
                    atomic.rollback();
                    bodyFile.close();
                    return Core::Result<BspDecompileResult>::cancelled(
                        QCoreApplication::translate("BspDecompiler", "BSP decompilation cancelled"));
                }

                const qint64 bytesRead = bodyFile.read(copyBuf.data(), static_cast<qint64>(copyBuf.size()));
                if (bytesRead < 0) {
                    atomic.rollback();
                    bodyFile.close();
                    return Core::Result<BspDecompileResult>::failure(
                        Core::Error::ErrorCode::ReadFailed,
                        QCoreApplication::translate("BspDecompiler", "Failed to read temporary VMF body file: %1")
                            .arg(bodyFile.errorString()));
                }
                if (bytesRead == 0) {
                    break;
                }

                qint64 offset = 0;
                while (offset < bytesRead) {
                    const qint64 written = outDevice->write(copyBuf.data() + offset, bytesRead - offset);
                    if (written <= 0) {
                        atomic.rollback();
                        bodyFile.close();
                        return Core::Result<BspDecompileResult>::failure(
                            Core::Error::ErrorCode::WriteFailed,
                            QCoreApplication::translate("BspDecompiler", "Failed to write VMF output '%1': %2")
                                .arg(outputVmfPath.toString(), outDevice->errorString()));
                    }
                    offset += written;
                }
            }

            bodyFile.close();
            atomic.commit();

            config.log(BspLogLevel::Info, QStringLiteral("Finished decompiling %1.").arg(bspPath.toString()));
            return Core::Result<BspDecompileResult>::success(std::move(metadata));
        });
}

Core::Result<QByteArray> BspDecompiler::decompileToBuffer(
    const Core::Path::FilesystemPath& bspPath,
    const BspDecompileConfig& config,
    BspDecompileResult* outMetadata)
{
    return runDecompilePipeline<QByteArray>(
        bspPath,
        nullptr,
        config,
        {},
        [&](Internal::BspFileReader& reader,
            const std::optional<Internal::NmoData>& nmoData,
            BspDecompileResult& metadata) -> Core::Result<QByteArray> {
            Internal::VmfWriter bodyWriter(
                config.vmfDoubleScale,
                config.vmfDoubleScaleTextureAxes,
                config.vmfDoubleScaleTextureScale,
                config.nullOutput,
                &config);
            Internal::VmfMeta vmfmeta(reader, bodyWriter, config);

            auto bodyRes = decompileWithWriter(reader, bodyWriter, vmfmeta, config, nmoData, metadata);
            if (bodyRes.isCancelled()) {
                return Core::Result<QByteArray>::cancelled(bodyRes.message());
            }
            if (bodyRes.isFailure()) {
                return Core::Result<QByteArray>::failure(bodyRes.error(), bodyRes.message());
            }

            Internal::VmfWriter headerWriter(
                config.vmfDoubleScale,
                config.vmfDoubleScaleTextureAxes,
                config.vmfDoubleScaleTextureScale,
                config.nullOutput,
                &config);
            writeVmfHeaderSections(vmfmeta, headerWriter, config);

            QByteArray header = headerWriter.finish();
            QByteArray body = bodyWriter.finish();
            header.reserve(header.size() + body.size());
            header.append(body);

            if (outMetadata != nullptr) {
                *outMetadata = std::move(metadata);
            }

            config.log(BspLogLevel::Info, QStringLiteral("Finished decompiling %1.").arg(bspPath.toString()));
            return Core::Result<QByteArray>::success(std::move(header));
        });
}

} // namespace Domain::Bsp
