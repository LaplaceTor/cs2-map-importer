#pragma once

#include <functional>

#include <QString>

#include "Core/Async/CancellationToken.h"
#include "Core/Path/FilesystemPath.h"

namespace Domain::Bsp {

enum class BrushMode : int {
    BrushPlanes = 0,
    OrigFace,
    OrigFacePlus,
    SplitFace,
};

enum class SourceFormat : int {
    Auto = 0,
    Old,
    New,
};

enum class BspLogLevel : int {
    Debug = 0,
    Info,
    Warning,
    Error,
};

using BspLogCallback = std::function<void(BspLogLevel level, const QString& message)>;

struct BspDecompileConfig {
    int defaultAppId = 0; // SourceAppId::kUnknown
    BrushMode brushMode = BrushMode::BrushPlanes;
    SourceFormat sourceFormat = SourceFormat::Auto;
    bool debug = false;

    // entity options
    bool writeAreaportals = true;
    bool writeBrushEntities = true;
    bool writeCubemaps = true;
    bool writeDetails = true;
    bool writeOccluders = true;
    bool writeOverlays = true;
    bool writePointEntities = true;
    bool writeStaticProps = true;
    bool writeLadders = true;
    bool writeVisClusters = false;
    bool fixEntityRot = true;
    bool apForceManualMapping = false;
    bool detailMerge = false;
    float detailMergeThresh = 1.0f;
    int maxCubemapSides = 8;
    int maxOverlaySides = 64;

    // brush options
    bool writeWorldBrushes = true;
    bool writeDisp = true;
    float backfaceDepth = 1.0f;

    // texture options
    QString backfaceTexture;
    QString faceTexture;
    bool fixCubemapTextures = true;
    bool fixToolTextures = true;
    bool nodrawInvisibleSides = false;

    // miscellaneous options
    bool nullOutput = false;
    bool loadLumpFiles = true;
    bool skipProt = false;
    bool writeVisgroups = true;
    bool writeCameras = true;
    bool unpackEmbedded = false;
    bool smartUnpack = true;
    Core::Path::FilesystemPath embeddedUnpackDir; // optional override; if empty, uses <vmfDir>/<bspBaseName>
    Core::Path::FilesystemPath nmoFilePath;       // optional override for NMRiH .nmo

    int vmfDoubleScale = 8;
    int vmfDoubleScaleTextureAxes = 4;
    int vmfDoubleScaleTextureScale = 4;

    Core::Async::CancellationToken token;
    BspLogCallback onLog;

    bool isWriteEntities() const noexcept {
        return writeBrushEntities || writePointEntities;
    }

    void setWriteEntities(bool writeEntities) noexcept {
        writeBrushEntities = writeEntities;
        writePointEntities = writeEntities;
    }

    void log(BspLogLevel level, const QString& msg) const {
        if (onLog) {
            onLog(level, msg);
        }
    }
};

} // namespace Domain::Bsp
