#pragma once

#include <cstddef>
#include <vector>

#include <QString>
#include <QStringView>

#include "Core/Path/FilesystemPath.h"
#include "Core/Result/Result.h"
#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspStructs.h"

namespace Domain::Bsp::Internal {

struct BspLumpBuffer {
    int index = 0;
    LumpType type = LumpType::LUMP_UNKNOWN;
    int offset = 0;
    int length = 0;
    int version = 0;
    int fourCC = 0;
    std::vector<std::byte> data;
};

struct BspGameLumpBuffer {
    int fourCC = 0;
    QString name;
    int flags = 0;
    int version = 0;
    int offset = 0;
    int length = 0;
    std::vector<std::byte> data;
};

class BspFile {
public:
    static constexpr int kHeaderLumps = 64;
    static constexpr int kHeaderLumpsTf = 128;
    static constexpr int kHeaderSize = 1036;
    static constexpr int kMaxLumpFiles = 128;

    static Core::Result<BspFile> open(
        const Core::Path::FilesystemPath& file,
        int defaultAppId,
        bool loadExternalLumpFiles,
        const BspDecompileConfig& config);

    const Core::Path::FilesystemPath& file() const noexcept { return m_file; }
    const QString& name() const noexcept { return m_name; }
    int version() const noexcept { return m_version; }
    int revision() const noexcept { return m_mapRev; }
    int appId() const noexcept { return m_appId; }
    void setAppId(int appId) noexcept { m_appId = appId; }
    bool isBigEndian() const noexcept { return m_bigEndian; }

    bool canReadLump(LumpType type) const noexcept;
    const BspLumpBuffer* lump(LumpType type) const noexcept;
    const BspGameLumpBuffer* gameLump(QStringView sid) const noexcept;

    void uncompress(const BspDecompileConfig& config);

private:
    void loadLumps(const std::vector<std::byte>& fileBytes, std::size_t& cursor, const BspDecompileConfig& config);
    void loadGameLumps(const BspDecompileConfig& config);
    void loadLumpFiles(const BspDecompileConfig& config);
    void loadTitanfallLumpFiles(const BspDecompileConfig& config);
    void loadTitanfallEntityFiles(const BspDecompileConfig& config);

    Core::Path::FilesystemPath m_file;
    QString m_name;
    int m_version = 0;
    int m_mapRev = 0;
    int m_appId = 0;
    bool m_bigEndian = false;
    std::vector<BspLumpBuffer> m_lumps;
    std::vector<BspGameLumpBuffer> m_gameLumps;
};

class BspFileReader {
public:
    BspFileReader(BspFile& bspFile, const BspDecompileConfig& config);

    void loadAll();
    void loadPlanes();
    void loadBrushes();
    void loadBrushSides();
    void loadVertices();
    void loadClipPortalVertices();
    void loadEdges();
    void loadFaces();
    void loadOriginalFaces();
    void loadModels();
    void loadSurfaceEdges();
    void loadStaticProps();
    void loadCubemaps();
    void loadDispInfos();
    void loadDispVertices();
    void loadDispTriangleTags();
    void loadDispMultiBlend();
    void loadTexInfo();
    void loadTexData();
    void loadEntities();
    void loadNodes();
    void loadLeaves();
    void loadLeafFaces();
    void loadLeafBrushes();
    void loadOverlays();
    void loadAreaportals();
    void loadOccluders();
    void loadFlags();
    void loadPrimitives();
    void loadPrimIndices();
    void loadPrimVerts();

    BspFile& bspFile() noexcept { return m_bspFile; }
    const BspFile& bspFile() const noexcept { return m_bspFile; }
    BspData& data() noexcept { return m_bspData; }
    const BspData& data() const noexcept { return m_bspData; }

private:
    int appId() const noexcept { return m_bspFile.appId(); }
    void loadTexDataStrings();

    BspFile& m_bspFile;
    BspDecompileConfig m_config;
    BspData m_bspData;

    bool m_loadedPlanes = false;
    bool m_loadedBrushes = false;
    bool m_loadedBrushSides = false;
    bool m_loadedVertices = false;
    bool m_loadedClipPortalVertices = false;
    bool m_loadedEdges = false;
    bool m_loadedFaces = false;
    bool m_loadedOriginalFaces = false;
    bool m_loadedModels = false;
    bool m_loadedSurfaceEdges = false;
    bool m_loadedStaticProps = false;
    bool m_loadedCubemaps = false;
    bool m_loadedDispInfos = false;
    bool m_loadedDispVertices = false;
    bool m_loadedDispTriangleTags = false;
    bool m_loadedDispMultiBlend = false;
    bool m_loadedTexInfo = false;
    bool m_loadedTexData = false;
    bool m_loadedTexDataStrings = false;
    bool m_loadedEntities = false;
    bool m_loadedNodes = false;
    bool m_loadedLeaves = false;
    bool m_loadedLeafFaces = false;
    bool m_loadedLeafBrushes = false;
    bool m_loadedOverlays = false;
    bool m_loadedOverlayFades = false;
    bool m_loadedOverlaySysLevels = false;
    bool m_loadedAreaportals = false;
    bool m_loadedOccluders = false;
    bool m_loadedFlags = false;
    bool m_loadedPrimitives = false;
    bool m_loadedPrimIndices = false;
    bool m_loadedPrimVerts = false;
};

class NmoFileParser {
public:
    static Core::Result<NmoData> load(const Core::Path::FilesystemPath& nmoPath);
    static Core::Result<void> writeAsNmos(
        const NmoData& nmo,
        const Core::Path::FilesystemPath& nmosPath,
        const Core::Path::FilesystemPath& expectedBaseDir = {});
};

} // namespace Domain::Bsp::Internal
