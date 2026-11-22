#include "Domain/Bsp/Internal/BspDecompilerModules.h"

#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <numbers>
#include <string_view>
#include <utility>

#include "Domain/Bsp/Internal/SourceAppDb.h"

namespace Domain::Bsp::Internal {

namespace {

constexpr double kAreaEps = 1.0;
constexpr std::array<int, 7> kTriConv = {0, 0, 1, 0, 0, 0, 9};

bool usesNonObjectBrushLadders(int appId) noexcept
{
    return appId == SourceAppId::kCounterStrikeGo;
}

QString formatBrushFlagsDebug(const QSet<BrushFlag>& flags)
{
    struct Entry {
        BrushFlag flag;
        QLatin1StringView name;
    };
    static constexpr std::array<Entry, 29> kEntries = {{
        {BrushFlag::CONTENTS_SOLID, "CONTENTS_SOLID"},
        {BrushFlag::CONTENTS_WINDOW, "CONTENTS_WINDOW"},
        {BrushFlag::CONTENTS_AUX, "CONTENTS_AUX"},
        {BrushFlag::CONTENTS_GRATE, "CONTENTS_GRATE"},
        {BrushFlag::CONTENTS_SLIME, "CONTENTS_SLIME"},
        {BrushFlag::CONTENTS_WATER, "CONTENTS_WATER"},
        {BrushFlag::CONTENTS_BLOCKLOS, "CONTENTS_BLOCKLOS"},
        {BrushFlag::CONTENTS_OPAQUE, "CONTENTS_OPAQUE"},
        {BrushFlag::CONTENTS_TESTFOGVOLUME, "CONTENTS_TESTFOGVOLUME"},
        {BrushFlag::CONTENTS_TEAM1, "CONTENTS_TEAM1"},
        {BrushFlag::CONTENTS_TEAM2, "CONTENTS_TEAM2"},
        {BrushFlag::CONTENTS_IGNORE_NODRAW_OPAQUE, "CONTENTS_IGNORE_NODRAW_OPAQUE"},
        {BrushFlag::CONTENTS_MOVEABLE, "CONTENTS_MOVEABLE"},
        {BrushFlag::CONTENTS_AREAPORTAL, "CONTENTS_AREAPORTAL"},
        {BrushFlag::CONTENTS_PLAYERCLIP, "CONTENTS_PLAYERCLIP"},
        {BrushFlag::CONTENTS_MONSTERCLIP, "CONTENTS_MONSTERCLIP"},
        {BrushFlag::CONTENTS_CURRENT_0, "CONTENTS_CURRENT_0"},
        {BrushFlag::CONTENTS_CURRENT_90, "CONTENTS_CURRENT_90"},
        {BrushFlag::CONTENTS_CURRENT_180, "CONTENTS_CURRENT_180"},
        {BrushFlag::CONTENTS_CURRENT_270, "CONTENTS_CURRENT_270"},
        {BrushFlag::CONTENTS_CURRENT_UP, "CONTENTS_CURRENT_UP"},
        {BrushFlag::CONTENTS_CURRENT_DOWN, "CONTENTS_CURRENT_DOWN"},
        {BrushFlag::CONTENTS_ORIGIN, "CONTENTS_ORIGIN"},
        {BrushFlag::CONTENTS_MONSTER, "CONTENTS_MONSTER"},
        {BrushFlag::CONTENTS_DEBRIS, "CONTENTS_DEBRIS"},
        {BrushFlag::CONTENTS_DETAIL, "CONTENTS_DETAIL"},
        {BrushFlag::CONTENTS_TRANSLUCENT, "CONTENTS_TRANSLUCENT"},
        {BrushFlag::CONTENTS_LADDER, "CONTENTS_LADDER"},
        {BrushFlag::CONTENTS_HITBOX, "CONTENTS_HITBOX"},
    }};

    QString out = QStringLiteral("[");
    bool first = true;
    for (const auto& e : kEntries) {
        if (flags.contains(e.flag)) {
            if (!first) {
                out += QStringLiteral(", ");
            }
            out += e.name;
            first = false;
        }
    }
    out += QLatin1Char(']');
    return out;
}

QString formatSurfaceFlagsDebug(const QSet<SurfaceFlag>& flags)
{
    struct Entry {
        SurfaceFlag flag;
        QLatin1StringView name;
    };
    static constexpr std::array<Entry, 16> kEntries = {{
        {SurfaceFlag::SURF_LIGHT, "SURF_LIGHT"},
        {SurfaceFlag::SURF_SKY2D, "SURF_SKY2D"},
        {SurfaceFlag::SURF_SKY, "SURF_SKY"},
        {SurfaceFlag::SURF_WARP, "SURF_WARP"},
        {SurfaceFlag::SURF_TRANS, "SURF_TRANS"},
        {SurfaceFlag::SURF_NOPORTAL, "SURF_NOPORTAL"},
        {SurfaceFlag::SURF_TRIGGER, "SURF_TRIGGER"},
        {SurfaceFlag::SURF_NODRAW, "SURF_NODRAW"},
        {SurfaceFlag::SURF_HINT, "SURF_HINT"},
        {SurfaceFlag::SURF_SKIP, "SURF_SKIP"},
        {SurfaceFlag::SURF_NOLIGHT, "SURF_NOLIGHT"},
        {SurfaceFlag::SURF_BUMPLIGHT, "SURF_BUMPLIGHT"},
        {SurfaceFlag::SURF_NOSHADOWS, "SURF_NOSHADOWS"},
        {SurfaceFlag::SURF_NODECALS, "SURF_NODECALS"},
        {SurfaceFlag::SURF_NOCHOP, "SURF_NOCHOP"},
        {SurfaceFlag::SURF_HITBOX, "SURF_HITBOX"},
    }};

    QString out = QStringLiteral("[");
    bool first = true;
    for (const auto& e : kEntries) {
        if (flags.contains(e.flag)) {
            if (!first) {
                out += QStringLiteral(", ");
            }
            out += e.name;
            first = false;
        }
    }
    out += QLatin1Char(']');
    return out;
}

QString formatFloatArray4(const float (&arr)[4])
{
    return QStringLiteral("[%1, %2, %3, %4]")
        .arg(QString::number(arr[0], 'g', 9),
             QString::number(arr[1], 'g', 9),
             QString::number(arr[2], 'g', 9),
             QString::number(arr[3], 'g', 9));
}

void appendJavaFloat(QString& out, float value)
{
    if (std::isnan(value)) {
        out += QStringLiteral("NaN");
        return;
    }
    if (std::isinf(value)) {
        out += (value < 0.0f) ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
        return;
    }

    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf) - 3, value);
    if (ec == std::errc{}) {
        std::string_view sv(buf, static_cast<std::size_t>(ptr - buf));
        out += QString::fromLatin1(sv.data(), static_cast<qsizetype>(sv.size()));
        if (sv.find('.') == std::string_view::npos
            && sv.find('e') == std::string_view::npos
            && sv.find('E') == std::string_view::npos) {
            out += QStringLiteral(".0");
        }
        return;
    }

    out += QString::number(static_cast<double>(value), 'g', 9);
}

QString joinSortedIntSet(const std::vector<int>& values)
{
    QString out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out += QLatin1Char(' ');
        }
        out += QString::number(values[i]);
    }
    return out;
}

double radiansToDegrees(double rad) noexcept
{
    return rad * (180.0 / std::numbers::pi);
}

} // namespace

// ============================================================================
// BrushSource
// ============================================================================

BrushSource::BrushSource(
    BspFileReader& reader,
    VmfWriter& writer,
    const BspDecompileConfig& config,
    TextureSource& texsrc,
    const BspProtection& bspprot,
    VmfMeta& vmfmeta,
    const BrushSideFaceMapper& brushSideFaceMapper,
    WindingFactory& windingFactory,
    const OccluderMapper::ReallocationData& occReallocationData)
    : m_reader(reader)
    , m_bspFile(reader.getBspFile())
    , m_bsp(reader.getData())
    , m_writer(writer)
    , m_config(config)
    , m_texsrc(texsrc)
    , m_bspprot(bspprot)
    , m_vmfmeta(vmfmeta)
    , m_brushSideFaceMapper(brushSideFaceMapper)
    , m_windingFactory(windingFactory)
    , m_occReallocationData(occReallocationData)
{
    assignBrushes();
}

bool BrushSource::isFuncDetail(const DBrush& dBrush) const noexcept
{
    const bool potentialNonObjectBrushLadderDetail =
        usesNonObjectBrushLadders(m_bspFile.appId()) && dBrush.isLadder();
    return (potentialNonObjectBrushLadderDetail || dBrush.isSolid() || dBrush.isTranslucent())
        && dBrush.isDetail();
}

int BrushSource::getBrushSideIDForIndex(int ibrushside) const
{
    if (const auto it = m_brushSideToID.constFind(ibrushside); it != m_brushSideToID.constEnd()) {
        return it.value();
    }
    return -1;
}

int BrushSource::getBrushIDForIndex(int ibrush) const
{
    if (const auto it = m_brushIndexToID.constFind(ibrush); it != m_brushIndexToID.constEnd()) {
        return it.value();
    }
    return -1;
}

void BrushSource::assignBrushes()
{
    if (m_bsp.nodes.empty() && m_bsp.leaves.empty()) {
        m_worldbrushes = 0;
        return;
    }

    BspTreeStats tl(m_bsp);
    tl.walk(0);

    if (m_config.debug) {
        m_config.log(BspLogLevel::Debug, QStringLiteral("Walked worldspawn tree"));
    }

    m_worldbrushes = tl.getMaxBrushLeaf() + 1;

    for (const DModel& model : m_bsp.models) {
        tl.reset();
        tl.walk(model.headnode);

        DBrushModel bmodel;
        bmodel.fstbrush = tl.getMinBrushLeaf();
        bmodel.numbrush = tl.getMaxBrushLeaf() - tl.getMinBrushLeaf() + 1;
        m_models.push_back(bmodel);
    }

    if (m_config.debug) {
        m_config.log(BspLogLevel::Debug, QStringLiteral("Largest worldbrush: %1").arg(m_worldbrushes));
    }
}

void BrushSource::writeBrushes()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing brushes and planes"));

    const int limit = std::min(m_worldbrushes, static_cast<int>(m_bsp.brushes.size()));
    for (int i = 0; i < limit; ++i) {
        const DBrush& brush = m_bsp.brushes[static_cast<std::size_t>(i)];

        if (m_config.writeDetails && isFuncDetail(brush)) {
            continue;
        }
        if (m_config.writeAreaportals && brush.isAreaportal()) {
            continue;
        }
        if (m_config.writeLadders && brush.isLadder() && !usesNonObjectBrushLadders(m_bspFile.appId())) {
            continue;
        }

        writeBrush(i);
    }
}

bool BrushSource::writeBrush(
    int ibrush,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles)
{
    if (ibrush < 0 || ibrush >= static_cast<int>(m_bsp.brushes.size())) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Invalid brush index %1").arg(ibrush));
        return false;
    }

    const DBrush& brush = m_bsp.brushes[static_cast<std::size_t>(ibrush)];
    const int brushID = m_vmfmeta.getUID();
    m_brushIndexToID.insert(ibrush, brushID);

    std::vector<std::pair<int, Winding>> validBrushSides;
    validBrushSides.reserve(static_cast<std::size_t>(std::max(0, brush.numside)));

    for (int i = 0; i < brush.numside; ++i) {
        const int ibrushside = brush.fstside + i;
        if (ibrushside < 0 || ibrushside >= static_cast<int>(m_bsp.brushSides.size())) {
            continue;
        }

        const DBrushSide& brushSide = m_bsp.brushSides[static_cast<std::size_t>(ibrushside)];
        if (brushSide.bevel) {
            continue;
        }

        Winding wind = m_windingFactory.fromSide(m_bsp, brush, brushSide).removeDegenerated();

        QString skipReason;
        if (wind.isEmpty()) {
            skipReason = QStringLiteral("no vertices");
        } else if (wind.size() < 3) {
            skipReason = QStringLiteral("less than 3 vertices");
        } else if (m_windingFactory.isHuge(wind)) {
            skipReason = QStringLiteral("too big");
        } else {
            const auto plane = wind.buildPlane();
            if (!plane[0].isValid() || !plane[1].isValid() || !plane[2].isValid()) {
                skipReason = QStringLiteral("invalid plane");
            } else if (plane[0] == plane[1] || plane[0] == plane[2] || plane[1] == plane[2]) {
                skipReason = QStringLiteral("duplicate plane point");
            }
        }

        if (!skipReason.isEmpty()) {
            if (m_config.debug) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("Skipped side %1 of brush %2: %3").arg(i).arg(ibrush).arg(skipReason));
            }
            continue;
        }

        if (angles.has_value()) {
            wind = wind.rotate(*angles);
        }
        if (origin.has_value()) {
            wind = wind.translate(*origin);
        }

        validBrushSides.emplace_back(ibrushside, std::move(wind));
    }

    if (validBrushSides.empty()) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Skipped empty brush %1").arg(ibrush));
        return false;
    }

    if (validBrushSides.size() < 3) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Skipped brush %1 with less than 3 sides").arg(ibrush));
        return false;
    }

    m_writer.start(u"solid");
    m_writer.put(u"id", brushID);

    if (m_config.debug) {
        m_writer.start(u"bspsrc_debug");
        m_writer.put(u"brush_index", ibrush);
        m_writer.put(u"brush_contents", formatBrushFlagsDebug(brush.contents));
        m_writer.end(u"bspsrc_debug");
    }

    for (const auto& [ibrushside, wind] : validBrushSides) {
        writeSide(ibrushside, ibrush, wind, origin, angles);
    }

    if (!brush.isDetail() && m_bspprot.isProtectedBrushIndex(ibrush)) {
        m_vmfmeta.writeMetaVisgroup(QStringLiteral("VMEX protector brushes"));
    }

    m_writer.end(u"solid");
    return true;
}

bool BrushSource::writeSide(
    int ibrushside,
    int ibrush,
    const Winding& wind,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles)
{
    const DBrush& brush = m_bsp.brushes[static_cast<std::size_t>(ibrush)];
    const DBrushSide& brushSide = m_bsp.brushSides[static_cast<std::size_t>(ibrushside)];

    const auto plane = wind.buildPlane();
    const Vector3d& e1 = plane[0];
    const Vector3d& e2 = plane[1];
    const Vector3d& e3 = plane[2];

    const Vector3d ev12 = e2.sub(e1);
    const Vector3d ev13 = e3.sub(e1);
    const Vector3d normal = ev12.cross(ev13).normalize();

    const DTexInfo* texinfo = TextureBuilder::lookupTexinfo(brushSide.texinfo, m_bsp.texinfos, m_config);
    const DTexData* texdata = texinfo == nullptr
        ? nullptr
        : TextureBuilder::lookupTexdata(texinfo->texdata, m_bsp.texdatas, m_config);
    std::optional<QString> texname = texdata == nullptr
        ? std::nullopt
        : TextureBuilder::lookupTexname(texdata->texname, m_bsp.texnames, m_config);

    std::optional<Texture> textureOpt;
    if (texname.has_value()) {
        const QString& usedTexname = m_config.fixCubemapTextures
            ? m_texsrc.getFixedTextureNames()[static_cast<std::size_t>(texdata->texname)]
            : *texname;
        textureOpt = TextureBuilder::buildFromTexinfo(*texinfo, *texdata, usedTexname, origin, angles, normal);
    }

    Texture texture = textureOpt.has_value()
        ? std::move(*textureOpt)
        : TextureBuilder::buildFromNormal(normal, ToolTexture::kSkip);

    const bool noFaceForSide = !m_brushSideFaceMapper.getOrigFaceIndex(ibrushside).has_value();
    std::optional<QString> fixedToolTexture;
    if (m_config.fixToolTextures && noFaceForSide) {
        const bool isOccluderBrush = m_occReallocationData.isOccluderBrush(ibrush);
        const bool isOccluderBrushSide = m_occReallocationData.isOccluderBrushSide(ibrush, ibrushside - brush.fstside);

        fixedToolTexture = TextureBuilder::fixToolTexture(
            texture.texture,
            isOccluderBrush,
            isOccluderBrushSide,
            brush.contents,
            texinfo == nullptr ? nullptr : &texinfo->flags,
            m_texsrc.getToolTextureMatcher());
        if (fixedToolTexture.has_value()) {
            texture.texture = *fixedToolTexture;
        }
    }

    if (m_config.nodrawInvisibleSides && noFaceForSide && !fixedToolTexture.has_value()) {
        texture = TextureBuilder::buildFromNormal(normal, ToolTexture::kNodraw);
    }

    if (!m_config.faceTexture.isEmpty()) {
        texture.texture = m_config.faceTexture;
    }

    const int sideID = m_vmfmeta.getUID();

    if (texdata != nullptr) {
        m_texsrc.addBrushSideID(texdata->texname, sideID);
    }

    m_brushSideToID.insert(ibrushside, sideID);

    int smoothingGroups = 0;
    if (const auto origFaceIdx = m_brushSideFaceMapper.getOrigFaceIndex(ibrushside); origFaceIdx.has_value()) {
        if (*origFaceIdx >= 0 && *origFaceIdx < static_cast<int>(m_bsp.origFaces.size())) {
            smoothingGroups = m_bsp.origFaces[static_cast<std::size_t>(*origFaceIdx)].smoothingGroups;
        }
    }

    m_writer.start(u"side");
    m_writer.put(u"id", sideID);

    if (m_config.debug) {
        m_writer.start(u"bspsrc_debug");
        m_writer.put(u"brushside_index", ibrushside);
        m_writer.put(u"normal", normal);
        m_writer.put(u"winding", wind.toString());

        if (!texname.has_value() || texture.texture != *texname) {
            m_writer.put(u"original_material", texture.texture);
        }

        if (brushSide.texinfo != -1 && brushSide.texinfo >= 0
            && brushSide.texinfo < static_cast<int>(m_bsp.texinfos.size())) {
            const DTexInfo& ti = m_bsp.texinfos[static_cast<std::size_t>(brushSide.texinfo)];
            m_writer.put(u"texinfo_index", brushSide.texinfo);
            m_writer.put(u"texinfo_flags", formatSurfaceFlagsDebug(ti.flags));
            m_writer.put(u"texturevecs_u", formatFloatArray4(ti.textureVecsTexels[0]));
            m_writer.put(u"texturevecs_v", formatFloatArray4(ti.textureVecsTexels[1]));

            const Vector3d uaxis{ti.textureVecsTexels[0][0], ti.textureVecsTexels[0][1], ti.textureVecsTexels[0][2]};
            const Vector3d vaxis{ti.textureVecsTexels[1][0], ti.textureVecsTexels[1][1], ti.textureVecsTexels[1][2]};
            Vector3d texNorm = uaxis.cross(vaxis);
            double angle = radiansToDegrees(std::acos(normal.dot(texNorm) / texNorm.length()));
            m_writer.put(u"input_uv_normal", texNorm);
            m_writer.put(u"input_uv_angle", std::isnan(angle) ? 0.0 : angle);

            texNorm = texture.u.axis.cross(texture.v.axis);
            angle = radiansToDegrees(std::acos(normal.dot(texNorm)));
            m_writer.put(u"output_uv_normal", texNorm);
            m_writer.put(u"output_uv_angle", std::isnan(angle) ? 0.0 : angle);
        }
        m_writer.end(u"bspsrc_debug");
    }

    m_writer.put(u"plane", e1, e2, e3);
    m_writer.put(u"smoothing_groups", smoothingGroups);
    m_writer.put(texture);

    m_writer.end(u"side");
    return true;
}

bool BrushSource::writeModel(
    int imodel,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles)
{
    if (imodel < 0 || imodel >= static_cast<int>(m_models.size())) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Invalid model index %1").arg(imodel));
        return false;
    }

    const DBrushModel& bmodel = m_models[static_cast<std::size_t>(imodel)];
    for (int i = 0; i < bmodel.numbrush; ++i) {
        writeBrush(bmodel.fstbrush + i, origin, angles);
    }
    return true;
}

// ============================================================================
// FaceSource
// ============================================================================

FaceSource::FaceSource(
    BspFileReader& reader,
    VmfWriter& writer,
    const BspDecompileConfig& config,
    TextureSource& texsrc,
    VmfMeta& vmfmeta,
    WindingFactory& windingFactory,
    const OccluderMapper::ReallocationData& occReallocationData)
    : m_reader(reader)
    , m_bspFile(reader.getBspFile())
    , m_bsp(reader.getData())
    , m_writer(writer)
    , m_config(config)
    , m_texsrc(texsrc)
    , m_vmfmeta(vmfmeta)
    , m_windingFactory(windingFactory)
    , m_occReallocationData(occReallocationData)
{
    if (m_bsp.origFaces.empty()) {
        for (DFace& face : m_bsp.faces) {
            face.origFace = -1;
        }
    } else {
        for (const DFace& face : m_bsp.faces) {
            if (face.origFace >= 0 && face.origFace < static_cast<int>(m_bsp.origFaces.size())) {
                m_bsp.origFaces[static_cast<std::size_t>(face.origFace)].texinfo = face.texinfo;
            }
        }
    }
}

void FaceSource::writeFaces()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing split faces"));
    if (m_bsp.models.empty()) {
        return;
    }

    const DModel& model = m_bsp.models[0];
    for (int i = 0; i < model.numface; ++i) {
        writeFace(model.fstface + i, false);
    }
}

void FaceSource::writeOrigFaces()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing original faces"));
    if (m_bsp.models.empty()) {
        return;
    }

    QSet<int> writtenFaces;
    const DModel& model = m_bsp.models[0];

    for (int i = 0; i < model.numface; ++i) {
        const int iface = model.fstface + i;
        if (iface < 0 || iface >= static_cast<int>(m_bsp.faces.size())) {
            continue;
        }
        const DFace& face = m_bsp.faces[static_cast<std::size_t>(iface)];
        if (face.origFace < 0) {
            continue;
        }

        const int iorigface = face.origFace;
        if (writtenFaces.contains(iorigface)) {
            continue;
        }

        writeFace(iorigface, true);
        writtenFaces.insert(iorigface);
    }
}

void FaceSource::writeOrigFacesPlus()
{
    createFaceMapping();

    QSet<int> writtenFaces;
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing original faces where possible"));

    if (m_bsp.models.empty()) {
        return;
    }

    const DModel& model = m_bsp.models[0];
    int sfaces = 0;

    for (int i = 0; i < model.numface; ++i) {
        const int iface = model.fstface + i;
        if (iface < 0 || iface >= static_cast<int>(m_bsp.faces.size())) {
            continue;
        }
        const DFace& face = m_bsp.faces[static_cast<std::size_t>(iface)];

        if (face.origFace >= 0) {
            const int iorigface = face.origFace;
            if (writtenFaces.contains(iorigface)) {
                continue;
            }

            if (m_undersizedFaces.contains(iorigface)) {
                ++sfaces;
                const auto it = m_origFaceToSplitFace.constFind(iorigface);
                if (it != m_origFaceToSplitFace.constEnd()) {
                    std::vector<int> splitIndices(it->cbegin(), it->cend());
                    std::sort(splitIndices.begin(), splitIndices.end());
                    for (int iface2 : splitIndices) {
                        writeFace(iface2, false);
                    }
                }
            } else {
                writeFace(iorigface, true);
            }

            writtenFaces.insert(iorigface);
        } else {
            writeFace(iface, false);
        }
    }

    m_config.log(
        BspLogLevel::Info,
        QStringLiteral("%1 original faces were written as split faces").arg(sfaces));
}

void FaceSource::writeDispFaces()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing displacements"));

    if (m_bsp.dispinfos.empty()) {
        return;
    }

    for (int i = 0; i < static_cast<int>(m_bsp.faces.size()); ++i) {
        if (m_bsp.faces[static_cast<std::size_t>(i)].dispInfo != -1) {
            writeFace(i, false);
        }
    }
}

void FaceSource::writeModel(
    int imodel,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles)
{
    if (imodel < 0 || imodel >= static_cast<int>(m_bsp.models.size())) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Invalid model index %1").arg(imodel));
        return;
    }

    const DModel& model = m_bsp.models[static_cast<std::size_t>(imodel)];
    for (int i = 0; i < model.numface; ++i) {
        writeFace(model.fstface + i, false, origin, angles);
    }
}

void FaceSource::writeFace(
    int iface,
    bool orig,
    const std::optional<Vector3d>& origin,
    const std::optional<Vector3d>& angles)
{
    const auto& faceList = orig ? m_bsp.origFaces : m_bsp.faces;
    if (iface < 0 || iface >= static_cast<int>(faceList.size())) {
        return;
    }

    const DFace& face = faceList[static_cast<std::size_t>(iface)];
    if (face.numedge < 2) {
        return;
    }

    Winding wind = m_windingFactory.fromFace(m_bsp, face);

    if (angles.has_value()) {
        wind = wind.rotate(*angles);
    }
    if (origin.has_value()) {
        wind = wind.translate(*origin);
    }

    const auto plane = wind.buildPlane();
    const Vector3d& e1 = plane[0];
    const Vector3d& e2 = plane[1];
    const Vector3d& e3 = plane[2];

    if (!e1.isValid() || !e2.isValid() || !e3.isValid()) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Face with wind %1 is invalid").arg(wind.toString()));
        return;
    }

    const Vector3d ev12 = e2.sub(e1);
    const Vector3d ev13 = e3.sub(e1);
    const Vector3d normal = ev12.cross(ev13).normalize();

    if (normal.isNaN() || normal.isInfinite()) {
        if (m_config.debug) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Bad normal for face %1").arg(iface));
        }
        return;
    }

    m_writer.start(u"solid");
    m_writer.put(u"id", m_vmfmeta.getUID());

    if (m_config.debug) {
        m_writer.start(u"bspsrc_debug");
        m_writer.put(u"face_index", iface);
        m_writer.put(u"normal", normal);
        m_writer.put(u"winding", wind.toString());

        if (face.texinfo != -1 && face.texinfo >= 0
            && face.texinfo < static_cast<int>(m_bsp.texinfos.size())) {
            m_writer.put(u"texinfo_index", face.texinfo);
            m_writer.put(
                u"texinfo_flags",
                formatSurfaceFlagsDebug(m_bsp.texinfos[static_cast<std::size_t>(face.texinfo)].flags));
        }
        m_writer.end(u"bspsrc_debug");
    }

    const int sideID = m_vmfmeta.getUID();
    if (orig) {
        m_vmfmeta.setOrigFaceUID(iface, sideID);
    } else {
        m_vmfmeta.setFaceUID(iface, sideID);
    }

    const DTexInfo* texinfo = TextureBuilder::lookupTexinfo(face.texinfo, m_bsp.texinfos, m_config);
    const DTexData* texdata = texinfo == nullptr
        ? nullptr
        : TextureBuilder::lookupTexdata(texinfo->texdata, m_bsp.texdatas, m_config);
    std::optional<QString> texname = texdata == nullptr
        ? std::nullopt
        : TextureBuilder::lookupTexname(
            texdata->texname,
            m_config.fixCubemapTextures ? m_texsrc.getFixedTextureNames() : m_bsp.texnames,
            m_config);

    std::optional<Texture> textureOpt;
    if (texname.has_value()) {
        textureOpt = TextureBuilder::buildFromTexinfo(*texinfo, *texdata, *texname, origin, angles, normal);
    }

    Texture texture = textureOpt.has_value()
        ? std::move(*textureOpt)
        : TextureBuilder::buildFromNormal(normal, ToolTexture::kSkip);

    if (!m_config.faceTexture.isEmpty()) {
        texture.texture = m_config.faceTexture;
    }

    if (texdata != nullptr) {
        m_texsrc.addBrushSideID(texdata->texname, sideID);
    }

    m_writer.start(u"side");
    m_writer.put(u"id", sideID);
    m_writer.put(u"plane", e1, e2, e3);
    m_writer.put(u"smoothing_groups", face.smoothingGroups);
    m_writer.put(texture);

    const bool disp = face.dispInfo != -1;
    if (disp && m_config.writeDisp) {
        m_vmfmeta.setDispInfoUID(face.dispInfo, sideID);
        writeDisplacement(face.dispInfo);
    }

    m_writer.end(u"side");

    if (!m_config.backfaceTexture.isEmpty()) {
        texture.texture = m_config.backfaceTexture;
    }

    if (disp) {
        writePrismBack(wind, texture);
    } else {
        writePyramBack(wind, texture);
    }

    m_writer.end(u"solid");
}

void FaceSource::writePrismBack(const Winding& wind, Texture texture, double depth)
{
    const auto plane = wind.buildPlane();
    Vector3d e1 = plane[0];
    Vector3d e2 = plane[1];
    Vector3d e3 = plane[2];

    const Vector3d ev12 = e2.sub(e1);
    const Vector3d ev13 = e3.sub(e1);
    const Vector3d normal = ev12.cross(ev13).normalize();

    const Vector3d bedge = normal.scalar(depth);
    e1 = e1.add(bedge);
    e2 = e2.add(bedge);
    e3 = e3.add(bedge);

    texture.u = TextureAxis{texture.u.axis.scalar(-1.0)};
    writeBackSide(texture, e1, e2, e3);

    const Vector3d tv2 = bedge.normalize();
    const std::size_t size = wind.size();

    for (std::size_t i = 0; i < size; ++i) {
        e1 = wind.get(i);
        e2 = wind.get((i + 1) % size);
        e3 = e1.add(bedge);

        Vector3d tv1 = e1.sub(e2).normalize();
        if (!tv1.isValid()) {
            tv1 = Vector3d::kNull;
        }

        texture.u = TextureAxis{tv1};
        texture.v = TextureAxis{tv2};

        writeBackSide(texture, e1, e2, e3);
    }
}

void FaceSource::writePrismBack(const Winding& wind, Texture texture)
{
    writePrismBack(wind, std::move(texture), static_cast<double>(m_config.backfaceDepth));
}

void FaceSource::writePyramBack(const Winding& wind, const Texture& texture, double depth)
{
    const auto plane = wind.buildPlane();
    Vector3d e1 = plane[0];
    Vector3d e2 = plane[1];
    Vector3d e3 = plane[2];

    const Vector3d ev12 = e2.sub(e1);
    const Vector3d ev13 = e3.sub(e1);
    const Vector3d normal = ev12.cross(ev13).normalize();

    e3 = wind.getCenter().add(normal.scalar(depth));

    const std::size_t size = wind.size();
    for (std::size_t i = 0; i < size; ++i) {
        e1 = wind.get(i);
        e2 = wind.get((i + 1) % size);
        writeBackSide(texture, e1, e2, e3);
    }
}

void FaceSource::writePyramBack(const Winding& wind, const Texture& texture)
{
    writePyramBack(wind, texture, static_cast<double>(m_config.backfaceDepth));
}

void FaceSource::writeAreaportal(int portalKey)
{
    for (const DAreaportal& ap : m_bsp.areaportals) {
        if (ap.portalKey == portalKey) {
            writeAreaportal(ap);
            return;
        }
    }
    m_config.log(
        BspLogLevel::Warning,
        QStringLiteral("Tried to write non existing areaportal with portalkey %1").arg(portalKey));
}

void FaceSource::writeAreaportal(const DAreaportal& ap)
{
    const Winding wind = m_windingFactory.fromAreaportal(m_bsp, ap);
    writePolygon(wind, ToolTexture::kAreaportal, true);
}

void FaceSource::writeOccluder(int occluderKey)
{
    if (occluderKey < 0 || occluderKey >= static_cast<int>(m_bsp.occluderDatas.size())) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Invalid occluder key %1").arg(occluderKey));
        return;
    }
    writeOccluder(m_bsp.occluderDatas[static_cast<std::size_t>(occluderKey)]);
}

void FaceSource::writeOccluder(const DOccluderData& od)
{
    for (int i = 0; i < od.polycount; ++i) {
        const int polyIndex = od.firstpoly + i;
        if (polyIndex < 0 || polyIndex >= static_cast<int>(m_bsp.occluderPolyDatas.size())) {
            continue;
        }
        const DOccluderPolyData& opd = m_bsp.occluderPolyDatas[static_cast<std::size_t>(polyIndex)];
        const Winding wind = m_windingFactory.fromOccluder(m_bsp, opd);
        writePolygon(wind, ToolTexture::kOccluder, ToolTexture::kNodraw, true, 1.0);
    }
}

void FaceSource::writePolygon(
    const Winding& wind,
    const QString& frontMaterial,
    const QString& backMaterial,
    bool prism,
    double depth)
{
    if (wind.isEmpty() || wind.size() < 3) {
        return;
    }

    const auto plane = wind.buildPlane();
    const Vector3d& e1 = plane[0];
    const Vector3d& e2 = plane[1];
    const Vector3d& e3 = plane[2];

    if (!e1.isValid() || !e2.isValid() || !e3.isValid()) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Areaportal with wind %1 is invalid").arg(wind.toString()));
        return;
    }

    const Vector3d ev12 = e2.sub(e1);
    const Vector3d ev13 = e3.sub(e1);
    const Vector3d normal = ev12.cross(ev13).normalize();

    if (normal.isNaN() || normal.isInfinite()) {
        if (m_config.debug) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Bad normal in writePolygon"));
        }
        return;
    }

    m_writer.start(u"solid");
    m_writer.put(u"id", m_vmfmeta.getUID());

    const int sideID = m_vmfmeta.getUID();
    Texture texture = TextureBuilder::buildFromNormal(normal, frontMaterial);

    m_writer.start(u"side");
    m_writer.put(u"id", sideID);
    m_writer.put(u"plane", e1, e2, e3);
    m_writer.put(texture);
    m_writer.end(u"side");

    texture.texture = backMaterial;

    if (prism) {
        writePrismBack(wind, texture, depth);
    } else {
        writePyramBack(wind, texture, depth);
    }

    m_writer.end(u"solid");
}

void FaceSource::writePolygon(
    const Winding& wind,
    const QString& frontMaterial,
    const QString& backMaterial,
    bool prism)
{
    writePolygon(wind, frontMaterial, backMaterial, prism, static_cast<double>(m_config.backfaceDepth));
}

void FaceSource::writePolygon(
    const Winding& wind,
    const QString& material,
    bool prism,
    double depth)
{
    writePolygon(wind, material, material, prism, depth);
}

void FaceSource::writePolygon(
    const Winding& wind,
    const QString& material,
    bool prism)
{
    writePolygon(wind, material, material, prism);
}

void FaceSource::writeBackSide(
    const Texture& texture,
    const Vector3d& e1,
    const Vector3d& e2,
    const Vector3d& e3)
{
    m_writer.start(u"side");
    m_writer.put(u"id", m_vmfmeta.getUID());
    m_writer.put(u"plane", e1, e3, e2);
    m_writer.put(u"smoothing_groups", 0);
    m_writer.put(texture);
    m_writer.end(u"side");
}

void FaceSource::writeDisplacement(int idispinfo)
{
    if (idispinfo < 0 || idispinfo >= static_cast<int>(m_bsp.dispinfos.size())) {
        return;
    }

    const DDispInfo& di = m_bsp.dispinfos[static_cast<std::size_t>(idispinfo)];

    std::vector<std::pair<QString, QString>> normalMap;
    std::vector<std::pair<QString, QString>> distanceMap;
    std::vector<std::pair<QString, QString>> alphaMap;
    std::vector<std::pair<QString, QString>> triangleTagMap;
    std::vector<std::pair<QString, QString>> multiBlendMap;
    std::vector<std::pair<QString, QString>> alphaBlendMap;
    std::array<std::vector<std::pair<QString, QString>>, DDispMultiBlend::kMaxMultiBlendChannels> multiBlendColorMaps;

    QString normalSb;
    QString distanceSb;
    QString alphaSb;
    QString multiblendSb;
    QString alphablendSb;
    std::array<QString, DDispMultiBlend::kMaxMultiBlendChannels> multiblendColorSbs;
    QString triangleTagSb;
    QString allowedVertSb;

    const int vertcount = di.getVertexCount();
    const int psize = di.getPowerSize();
    const bool hasMultiBlend = !m_bsp.dispmultiblend.empty() && di.hasMultiBlend();

    for (int i = 0; i < vertcount; ++i) {
        const int vertIdx = di.dispVertStart + i;
        if (vertIdx < 0 || vertIdx >= static_cast<int>(m_bsp.dispverts.size())) {
            break;
        }
        const DDispVert& dv = m_bsp.dispverts[static_cast<std::size_t>(vertIdx)];

        const DDispMultiBlend* dmb = nullptr;
        if (hasMultiBlend) {
            const int mbIdx = m_multiblendOffset + i;
            if (mbIdx >= 0 && mbIdx < static_cast<int>(m_bsp.dispmultiblend.size())) {
                dmb = &m_bsp.dispmultiblend[static_cast<std::size_t>(mbIdx)];
            }
        }

        appendJavaFloat(normalSb, dv.vector.x);
        normalSb += QLatin1Char(' ');
        appendJavaFloat(normalSb, dv.vector.y);
        normalSb += QLatin1Char(' ');
        appendJavaFloat(normalSb, dv.vector.z);

        appendJavaFloat(distanceSb, dv.dist);
        appendJavaFloat(alphaSb, dv.alpha);

        if (dmb != nullptr) {
            appendJavaFloat(multiblendSb, dmb->multiblend.x);
            multiblendSb += QLatin1Char(' ');
            appendJavaFloat(multiblendSb, dmb->multiblend.y);
            multiblendSb += QLatin1Char(' ');
            appendJavaFloat(multiblendSb, dmb->multiblend.z);
            multiblendSb += QLatin1Char(' ');
            appendJavaFloat(multiblendSb, dmb->multiblend.w);

            appendJavaFloat(alphablendSb, dmb->alphablend.x);
            alphablendSb += QLatin1Char(' ');
            appendJavaFloat(alphablendSb, dmb->alphablend.y);
            alphablendSb += QLatin1Char(' ');
            appendJavaFloat(alphablendSb, dmb->alphablend.z);
            alphablendSb += QLatin1Char(' ');
            appendJavaFloat(alphablendSb, dmb->alphablend.w);

            for (int j = 0; j < DDispMultiBlend::kMaxMultiBlendChannels; ++j) {
                QString& mbcsb = multiblendColorSbs[static_cast<std::size_t>(j)];
                appendJavaFloat(mbcsb, dmb->multiblendcolors[static_cast<std::size_t>(j)].x);
                mbcsb += QLatin1Char(' ');
                appendJavaFloat(mbcsb, dmb->multiblendcolors[static_cast<std::size_t>(j)].y);
                mbcsb += QLatin1Char(' ');
                appendJavaFloat(mbcsb, dmb->multiblendcolors[static_cast<std::size_t>(j)].z);
            }
        }

        if (i % (psize + 1) == psize) {
            normalMap.emplace_back(QStringLiteral("row%1").arg(normalMap.size()), normalSb);
            distanceMap.emplace_back(QStringLiteral("row%1").arg(distanceMap.size()), distanceSb);
            alphaMap.emplace_back(QStringLiteral("row%1").arg(alphaMap.size()), alphaSb);

            normalSb.clear();
            distanceSb.clear();
            alphaSb.clear();

            if (dmb != nullptr) {
                multiBlendMap.emplace_back(QStringLiteral("row%1").arg(multiBlendMap.size()), multiblendSb);
                alphaBlendMap.emplace_back(QStringLiteral("row%1").arg(alphaBlendMap.size()), alphablendSb);
                for (int j = 0; j < DDispMultiBlend::kMaxMultiBlendChannels; ++j) {
                    auto& colorMap = multiBlendColorMaps[static_cast<std::size_t>(j)];
                    colorMap.emplace_back(
                        QStringLiteral("row%1").arg(colorMap.size()),
                        multiblendColorSbs[static_cast<std::size_t>(j)]);
                    multiblendColorSbs[static_cast<std::size_t>(j)].clear();
                }
                multiblendSb.clear();
                alphablendSb.clear();
            }
        } else {
            normalSb += QLatin1Char(' ');
            distanceSb += QLatin1Char(' ');
            alphaSb += QLatin1Char(' ');

            if (dmb != nullptr) {
                multiblendSb += QLatin1Char(' ');
                alphablendSb += QLatin1Char(' ');
                for (int j = 0; j < DDispMultiBlend::kMaxMultiBlendChannels; ++j) {
                    multiblendColorSbs[static_cast<std::size_t>(j)] += QLatin1Char(' ');
                }
            }
        }
    }

    if (hasMultiBlend) {
        m_multiblendOffset += vertcount;
    }

    const int tcount = di.getTriangleTagCount();
    for (int i = 0; i < tcount; ++i) {
        const int triIdx = di.dispTriStart + i;
        int dt = (triIdx >= 0 && triIdx < static_cast<int>(m_bsp.disptris.size()))
            ? m_bsp.disptris[static_cast<std::size_t>(triIdx)].tags
            : 0;
        if (dt < 0 || dt > 6) {
            dt = 0;
        }

        triangleTagSb += QString::number(kTriConv[static_cast<std::size_t>(dt)]);

        if (i % (2 * psize) == 2 * psize - 1) {
            triangleTagMap.emplace_back(QStringLiteral("row%1").arg(triangleTagMap.size()), triangleTagSb);
            triangleTagSb.clear();
        } else {
            triangleTagSb += QLatin1Char(' ');
        }
    }

    for (std::size_t i = 0; i < di.allowedVerts.size(); ++i) {
        allowedVertSb += QString::number(di.allowedVerts[i]);
        if (i + 1 < di.allowedVerts.size()) {
            allowedVertSb += QLatin1Char(' ');
        }
    }

    m_writer.start(u"dispinfo");

    if (m_config.debug) {
        m_writer.put(u"bspsrc_dispinfo_index", idispinfo);
    }

    m_writer.put(u"power", di.power);
    m_writer.put(u"startposition", di.startPos.toDouble(), 2);
    m_writer.put(u"flags", di.getSurfaceFlags());
    m_writer.put(u"elevation", 0);
    m_writer.put(u"subdiv", 0);

    m_writer.start(u"normals");
    m_writer.put(normalMap);
    m_writer.end(u"normals");

    m_writer.start(u"distances");
    m_writer.put(distanceMap);
    m_writer.end(u"distances");

    const int rowVerts = psize + 1;
    QString offsetRowStr;
    offsetRowStr.reserve(rowVerts * 6);
    QString offsetNormalRowStr;
    offsetNormalRowStr.reserve(rowVerts * 6);
    for (int col = 0; col < rowVerts; ++col) {
        if (col > 0) {
            offsetRowStr += QLatin1Char(' ');
            offsetNormalRowStr += QLatin1Char(' ');
        }
        offsetRowStr += QStringLiteral("0 0 0");
        offsetNormalRowStr += QStringLiteral("0 0 1");
    }

    m_writer.start(u"offsets");
    for (int row = 0; row < rowVerts; ++row) {
        m_writer.put(QStringLiteral("row%1").arg(row), offsetRowStr);
    }
    m_writer.end(u"offsets");

    m_writer.start(u"offset_normals");
    for (int row = 0; row < rowVerts; ++row) {
        m_writer.put(QStringLiteral("row%1").arg(row), offsetNormalRowStr);
    }
    m_writer.end(u"offset_normals");

    m_writer.start(u"alphas");
    m_writer.put(alphaMap);
    m_writer.end(u"alphas");

    m_writer.start(u"triangle_tags");
    m_writer.put(triangleTagMap);
    m_writer.end(u"triangle_tags");

    m_writer.start(u"allowed_verts");
    m_writer.put(u"10", allowedVertSb);
    m_writer.end(u"allowed_verts");

    if (hasMultiBlend) {
        m_writer.start(u"multiblend");
        m_writer.put(multiBlendMap);
        m_writer.end(u"multiblend");

        m_writer.start(u"alphablend");
        m_writer.put(alphaBlendMap);
        m_writer.end(u"alphablend");

        for (int j = 0; j < DDispMultiBlend::kMaxMultiBlendChannels; ++j) {
            const QString sectionName = QStringLiteral("multiblend_color_%1").arg(j);
            m_writer.start(sectionName);
            m_writer.put(multiBlendColorMaps[static_cast<std::size_t>(j)]);
            m_writer.end(sectionName);
        }
    }

    m_writer.end(u"dispinfo");
}

void FaceSource::createFaceMapping()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Building split face to original face maps"));

    for (int i = 0; i < static_cast<int>(m_bsp.faces.size()); ++i) {
        const int o = m_bsp.faces[static_cast<std::size_t>(i)].origFace;
        if (o < 0) {
            continue;
        }
        m_origFaceToSplitFace[o].insert(i);
    }

    m_config.log(BspLogLevel::Info, QStringLiteral("Building original face areas"));

    for (int i = 0; i < static_cast<int>(m_bsp.origFaces.size()); ++i) {
        DFace& origFace = m_bsp.origFaces[static_cast<std::size_t>(i)];

        if (origFace.area == 0.0f) {
            const Winding wind = m_windingFactory.fromFace(m_bsp, origFace);
            origFace.area = static_cast<float>(wind.getArea());
        }

        float carea = 0.0f;
        const auto it = m_origFaceToSplitFace.constFind(i);
        if (it != m_origFaceToSplitFace.constEnd()) {
            for (int faceIdx : *it) {
                if (faceIdx >= 0 && faceIdx < static_cast<int>(m_bsp.faces.size())) {
                    carea += m_bsp.faces[static_cast<std::size_t>(faceIdx)].area;
                }
            }
        }

        if (static_cast<double>(carea) > static_cast<double>(origFace.area) + kAreaEps) {
            m_undersizedFaces.insert(i);
        }
    }
}

// ============================================================================
// EntitySource
// ============================================================================

EntitySource::EntitySource(
    BspFileReader& reader,
    VmfWriter& writer,
    const BspDecompileConfig& config,
    BrushSource& brushsrc,
    FaceSource& facesrc,
    TextureSource& texsrc,
    const BspProtection& bspprot,
    VmfMeta& vmfmeta,
    const BrushSideFaceMapper& brushSideFaceMapper,
    WindingFactory& windingFactory,
    BrushBounds& brushBounds,
    const AreaportalMapper::ReallocationData& areaportalReallocationData,
    const OccluderMapper::ReallocationData& occluderReallocationData)
    : m_reader(reader)
    , m_bspFile(reader.getBspFile())
    , m_bsp(reader.getData())
    , m_writer(writer)
    , m_config(config)
    , m_brushsrc(brushsrc)
    , m_facesrc(facesrc)
    , m_texsrc(texsrc)
    , m_bspprot(bspprot)
    , m_vmfmeta(vmfmeta)
    , m_brushSideFaceMapper(brushSideFaceMapper)
    , m_windingFactory(windingFactory)
    , m_brushBounds(brushBounds)
    , m_areaportalReallocationData(areaportalReallocationData)
    , m_occluderReallocationData(occluderReallocationData)
{
    processEntities();
}

void EntitySource::writeEntities()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing entities"));

    const bool fixEntityRot = m_config.fixEntityRot;

    VmfMeta::Visgroup* reallocatedVg = m_vmfmeta.visgroups()
        ->getVisgroup(QStringLiteral("Reallocated"))
        ->setColor(QColor(0, 255, 0));

    VmfMeta::Visgroup* rebuildVg = m_vmfmeta.visgroups()
        ->getVisgroup(QStringLiteral("Rebuild"))
        ->setColor(QColor(255, 0, 0));

    VmfMeta::Visgroup* reallocatedAreaportalVg = reallocatedVg
        ->getVisgroup(QStringLiteral("Areaportal"))
        ->setColor(QColor(0, 255, 255));
    VmfMeta::Visgroup* rebuildAreaportalVg = rebuildVg
        ->getVisgroup(QStringLiteral("Areaportal"))
        ->setColor(QColor(0, 178, 178));

    VmfMeta::Visgroup* reallocatedOccluderVg = reallocatedVg
        ->getVisgroup(QStringLiteral("Occluder"))
        ->setColor(QColor(255, 0, 255));
    VmfMeta::Visgroup* rebuildOccluderVg = rebuildVg
        ->getVisgroup(QStringLiteral("Occluder"))
        ->setColor(QColor(178, 0, 178));

    static const QRegularExpression instancePrefixRegex(QStringLiteral("^([^-]+)-"));
    std::vector<VmfMeta::Visgroup*> visgroups;

    for (BspEntity& ent : m_bsp.entities) {
        visgroups.clear();

        const QString className = ent.className();

        if (className == QStringLiteral("worldspawn")) {
            continue;
        }

        if (className == QStringLiteral("env_sprite")) {
            if (const auto model = ent.value(u"model");
                model.has_value() && model->startsWith(QStringLiteral("model"))) {
                ent.removeValue(u"scale");
            }
        }

        const bool isAreaportal = className.startsWith(QStringLiteral("func_areaportal"));
        const bool isOccluder = className == QStringLiteral("func_occluder");
        const bool hasBrush = ent.modelNum() > 0 || isAreaportal || isOccluder;

        if (!m_config.writePointEntities && !hasBrush) {
            continue;
        }
        if (!m_config.writeBrushEntities && hasBrush) {
            continue;
        }
        if (!m_config.writeAreaportals && isAreaportal) {
            continue;
        }
        if (!m_config.writeOccluders && isOccluder) {
            continue;
        }
        if (className == QStringLiteral("info_ladder")) {
            continue;
        }

        if (className == QStringLiteral("info_overlay_accessor")) {
            if (const auto oidStr = ent.value(u"OverlayID");
                oidStr.has_value() && !oidStr->isEmpty()) {
                bool ok = false;
                const int oid = oidStr->toInt(&ok);
                if (ok) {
                    if (const auto target = ent.targetName(); target.has_value()) {
                        m_overlayNames.insert(oid, *target);
                    }
                    continue;
                }
            }
        }

        int portalNum = -1;
        if (isAreaportal) {
            const auto portalNumStr = ent.value(u"portalnumber");
            if (!portalNumStr.has_value()) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("%1 is missing 'portalnumber' attribute, skipping...").arg(className));
                continue;
            }

            bool ok = false;
            portalNum = portalNumStr->toInt(&ok);
            if (!ok) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("Can't parse %1 'portalnumber' attribute: '%2', skipping...")
                        .arg(className, *portalNumStr));
                continue;
            }

            bool anyPortalFound = false;
            bool anyInvalidGeom = false;
            for (const DAreaportal& ap : m_bsp.areaportals) {
                if (ap.portalKey == portalNum) {
                    anyPortalFound = true;
                    if (ap.clipPortalVerts < 3) {
                        anyInvalidGeom = true;
                    }
                }
            }

            if (!anyPortalFound) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("func_areaportal entity links to a non existing areaportal, skipping..."));
                continue;
            }
            if (anyInvalidGeom) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("func_areaportal links areaportal with invalid geometry, skipping..."));
                continue;
            }

            if (!m_config.debug) {
                ent.removeValue(u"portalnumber");
            }
        }

        int occluderNum = -1;
        if (isOccluder) {
            const auto occluderNumStr = ent.value(u"occludernumber");
            if (!occluderNumStr.has_value()) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("%1 is missing 'occludernumber' attribute, skipping...").arg(className));
                continue;
            }

            bool ok = false;
            occluderNum = occluderNumStr->toInt(&ok);
            if (!ok) {
                m_config.log(
                    BspLogLevel::Warning,
                    QStringLiteral("Can't parse %1 'occludernumber' attribute: '%2', skipping...")
                        .arg(className, *occluderNumStr));
                continue;
            }

            if (!m_config.debug) {
                ent.removeValue(u"occludernumber");
            }
        }

        int entID = getHammerID(ent);

        if (m_nmo.has_value()) {
            if (const auto targetName = ent.targetName(); targetName.has_value()) {
                for (const NmoExtraction& extraction : m_nmo->extractions) {
                    if (extraction.name == *targetName) {
                        entID = extraction.id;
                        break;
                    }
                }
            }
        }

        if (entID == -1) {
            entID = m_vmfmeta.getUID();
        }

        m_writer.start(u"entity");
        m_writer.put(u"id", entID);

        const int modelNum = ent.modelNum();

        for (const EntityKeyValue& kv : ent.keyValues()) {
            const QString& key = kv.key;
            const QString& value = kv.value;

            if (key == QStringLiteral("angles") && modelNum >= 0 && fixEntityRot) {
                continue;
            }
            if (key == QStringLiteral("origin") && modelNum == 0) {
                continue;
            }
            if (key == QStringLiteral("model") && modelNum != -2) {
                continue;
            }
            if (key == QStringLiteral("hammerid")) {
                continue;
            }
            if ((isAreaportal || isOccluder)
                && (key == QStringLiteral("angles") || key == QStringLiteral("origin"))) {
                continue;
            }

            m_writer.put(key, value);
        }

        m_writer.put(u"classname", className);

        const auto& io = ent.io();
        if (!io.empty()) {
            m_writer.start(u"connections");
            for (const EntityKeyValue& kv : io) {
                m_writer.put(kv);
            }
            m_writer.end(u"connections");
        }

        std::optional<Vector3d> origin;
        if (const auto entOrigin = ent.origin(); entOrigin.has_value()) {
            origin = entOrigin->toDouble();
        }
        std::optional<Vector3d> angles;
        if (fixEntityRot) {
            if (const auto entAngles = ent.angles(); entAngles.has_value()) {
                angles = entAngles->toDouble();
            }
        }

        if (modelNum > 0) {
            if (m_config.brushMode == BrushMode::BrushPlanes) {
                m_brushsrc.writeModel(modelNum, origin, angles);
            } else {
                m_facesrc.writeModel(modelNum, origin, angles);
            }
        } else {
            if (isAreaportal && portalNum >= 0) {
                const auto apIt = m_areaportalReallocationData.mapping.constFind(portalNum);
                if (m_config.brushMode == BrushMode::BrushPlanes && apIt != m_areaportalReallocationData.mapping.constEnd()) {
                    m_brushsrc.writeBrush(apIt.value());
                    visgroups.push_back(reallocatedAreaportalVg);
                } else {
                    m_facesrc.writeAreaportal(portalNum);
                    visgroups.push_back(rebuildAreaportalVg);
                }

                if (m_config.debug) {
                    visgroups.push_back(
                        m_vmfmeta.visgroups()
                            ->getVisgroup(QStringLiteral("AreaportalID"))
                            ->getVisgroup(QString::number(portalNum)));
                }
            }

            if (isOccluder && occluderNum >= 0) {
                const auto occIt = m_occluderReallocationData.occluderToBrushes.constFind(occluderNum);
                if (m_config.brushMode == BrushMode::BrushPlanes
                    && occIt != m_occluderReallocationData.occluderToBrushes.constEnd()) {
                    std::vector<int> brushIds(occIt->cbegin(), occIt->cend());
                    std::sort(brushIds.begin(), brushIds.end());
                    for (int brushId : brushIds) {
                        m_brushsrc.writeBrush(brushId);
                    }
                    visgroups.push_back(reallocatedOccluderVg);
                } else {
                    m_facesrc.writeOccluder(occluderNum);
                    visgroups.push_back(rebuildOccluderVg);
                }
            }
        }

        if (const auto targetName = ent.targetName(); targetName.has_value()) {
            const auto match = instancePrefixRegex.match(*targetName);
            if (match.hasMatch()) {
                visgroups.push_back(
                    m_vmfmeta.visgroups()
                        ->getVisgroup(QStringLiteral("Instances"))
                        ->getVisgroup(match.captured(1)));
            }
        }

        if (m_bspprot.isProtectedEntity(ent)) {
            visgroups.push_back(
                m_vmfmeta.visgroups()->getVisgroup(QStringLiteral("VMEX flagged entities")));
        }

        if (m_nmo.has_value()) {
            if (const auto targetName = ent.targetName(); targetName.has_value()) {
                for (const NmoObjective& objective : m_nmo->nodes) {
                    const bool matched = objective.entityName == *targetName
                        || std::find(objective.entities.begin(), objective.entities.end(), *targetName)
                            != objective.entities.end();
                    if (matched) {
                        visgroups.push_back(
                            m_vmfmeta.visgroups()
                                ->getVisgroup(QStringLiteral("Objectives"))
                                ->getVisgroup(objective.name));
                    }
                }

                for (const NmoAntiObjective& anti : m_nmo->antiNodes) {
                    const bool matched =
                        std::find(anti.entities.begin(), anti.entities.end(), *targetName) != anti.entities.end();
                    if (matched) {
                        visgroups.push_back(
                            m_vmfmeta.visgroups()
                                ->getVisgroup(QStringLiteral("Objectives"))
                                ->getVisgroup(QStringLiteral("anti"))
                                ->getVisgroup(anti.name));
                    }
                }
            }
        }

        if (!visgroups.empty()) {
            m_vmfmeta.writeMetaVisgroups(visgroups);
        }

        m_writer.end(u"entity");
    }

    if (m_config.debug) {
        writeDebugPortals(m_bsp, m_facesrc, m_windingFactory, m_writer, m_vmfmeta);
        writeDebugOccluders(m_bsp, m_facesrc, m_windingFactory, m_writer, m_vmfmeta);
    }
}

void EntitySource::writeDetails()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing func_details"));

    std::vector<int> funcDetailBrushIndices;
    for (int i = 0; i < static_cast<int>(m_bsp.brushes.size()); ++i) {
        if (m_brushsrc.isFuncDetail(m_bsp.brushes[static_cast<std::size_t>(i)])
            && !m_bspprot.isProtectedBrushIndex(i)) {
            funcDetailBrushIndices.push_back(i);
        }
    }

    std::vector<std::vector<int>> funcDetailBrushGroups;

    if (m_config.detailMerge) {
        int newGroupId = 0;
        std::vector<std::pair<int, int>> brushGroups; // (brushIndex, groupId)
        brushGroups.reserve(funcDetailBrushIndices.size());

        for (int brushIdx : funcDetailBrushIndices) {
            const AABB bounds = m_brushBounds.getBounds(m_bsp, m_bsp.brushes[static_cast<std::size_t>(brushIdx)]);
            const AABB extendedBounds = bounds.expand(static_cast<double>(m_config.detailMergeThresh));

            QSet<int> intersectingGroupIds;
            for (const auto& [otherBrushIdx, groupId] : brushGroups) {
                if (!intersectingGroupIds.contains(groupId)) {
                    const AABB otherBounds = m_brushBounds.getBounds(
                        m_bsp,
                        m_bsp.brushes[static_cast<std::size_t>(otherBrushIdx)]);
                    if (extendedBounds.intersectsWith(otherBounds)) {
                        intersectingGroupIds.insert(groupId);
                    }
                }
            }

            if (!intersectingGroupIds.isEmpty()) {
                for (auto& [otherBrushIdx, groupId] : brushGroups) {
                    if (intersectingGroupIds.contains(groupId)) {
                        groupId = newGroupId;
                    }
                }
            }
            brushGroups.emplace_back(brushIdx, newGroupId++);
        }

        std::map<int, std::vector<int>> inverseBrushGroups;
        for (const auto& [brushIdx, groupId] : brushGroups) {
            inverseBrushGroups[groupId].push_back(brushIdx);
        }
        funcDetailBrushGroups.reserve(inverseBrushGroups.size());
        for (auto& [groupId, group] : inverseBrushGroups) {
            funcDetailBrushGroups.push_back(std::move(group));
        }
    } else {
        funcDetailBrushGroups.reserve(funcDetailBrushIndices.size());
        for (int brushIdx : funcDetailBrushIndices) {
            funcDetailBrushGroups.push_back({brushIdx});
        }
    }

    for (const auto& group : funcDetailBrushGroups) {
        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"func_detail");

        for (int brushIdx : group) {
            m_brushsrc.writeBrush(brushIdx);
        }

        m_writer.end(u"entity");
    }

    const auto& protBrushes = m_bspprot.getProtectedBrushIndices();
    if (!protBrushes.empty()) {
        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"func_detail");
        m_vmfmeta.writeMetaVisgroup(QStringLiteral("VMEX protector brushes"));

        for (int protBrushIdx : protBrushes) {
            m_brushsrc.writeBrush(protBrushIdx);
        }

        m_writer.end(u"entity");
    }
}

void EntitySource::writeVisClusters()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing func_viscluster"));

    std::map<int, std::vector<const DLeaf*>> leafsByCluster;
    for (const DLeaf& dLeaf : m_bsp.leaves) {
        if (dLeaf.cluster < 0) {
            continue;
        }
        const Vector3f size = dLeaf.maxs.sub(dLeaf.mins);
        if (size.x > 0.0f && size.y > 0.0f && size.z > 0.0f) {
            leafsByCluster[dLeaf.cluster].push_back(&dLeaf);
        }
    }

    for (const auto& [cluster, leaves] : leafsByCluster) {
        if (leaves.size() < 2) {
            continue;
        }

        AABB bounds = AABB::kZero;
        for (const DLeaf* dLeaf : leaves) {
            bounds = bounds.include(AABB{dLeaf->mins.toDouble(), dLeaf->maxs.toDouble()});
        }

        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"func_viscluster");

        m_facesrc.writePolygon(
            Winding({
                bounds.min,
                bounds.min.withX(bounds.max.x),
                bounds.min.withX(bounds.max.x).withY(bounds.max.y),
                bounds.min.withY(bounds.max.y),
            }),
            ToolTexture::kTrigger,
            true,
            bounds.max.z - bounds.min.z);

        m_vmfmeta.writeMetaVisgroups({
            m_vmfmeta.visgroups()
                ->getVisgroup(QStringLiteral("Rebuild"))
                ->getVisgroup(QStringLiteral("Vis Cluster"))
                ->getVisgroup(QString::number(cluster)),
        });

        m_writer.end(u"entity");

        if (m_config.debug) {
            for (const DLeaf* leaf : leaves) {
                m_writer.start(u"entity");
                m_writer.put(u"id", m_vmfmeta.getUID());
                m_writer.put(u"classname", u"func_detail");

                m_facesrc.writePolygon(
                    Winding({
                        leaf->mins.toDouble(),
                        leaf->mins.withX(leaf->maxs.x).toDouble(),
                        leaf->mins.withX(leaf->maxs.x).withY(leaf->maxs.y).toDouble(),
                        leaf->mins.withY(leaf->maxs.y).toDouble(),
                    }),
                    ToolTexture::kSkip,
                    true,
                    static_cast<double>(leaf->maxs.z - leaf->mins.z));

                m_vmfmeta.writeMetaVisgroups({
                    m_vmfmeta.visgroups()
                        ->getVisgroup(QStringLiteral("debug"))
                        ->getVisgroup(QStringLiteral("leafs-in-cluster"))
                        ->getVisgroup(QString::number(cluster)),
                });

                m_writer.end(u"entity");
            }
        }
    }
}

void EntitySource::writeOverlays()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing info_overlays"));

    for (int overlayI = 0; overlayI < static_cast<int>(m_bsp.overlays.size()); ++overlayI) {
        const DOverlay& o = m_bsp.overlays[static_cast<std::size_t>(overlayI)];

        if (m_windingFactory.isHuge(o.origin)) {
            m_config.log(
                BspLogLevel::Warning,
                QStringLiteral("Overlay %1 has origin outside map extends, skipping...").arg(overlayI));
            continue;
        }

        bool uvHuge = false;
        for (const Vector3f& pt : o.uvpoints) {
            if (m_windingFactory.isHuge(pt)) {
                uvHuge = true;
                break;
            }
        }
        if (uvHuge) {
            m_config.log(
                BspLogLevel::Warning,
                QStringLiteral("Overlay %1 has uvpoint outside map extends, skipping...").arg(overlayI));
            continue;
        }

        const Vector3f ubasis{o.uvpoints[0].z, o.uvpoints[1].z, o.uvpoints[2].z};
        const bool vflip = o.uvpoints[3].z == 1.0f;
        Vector3f vbasis = o.basisNormal.cross(ubasis).normalize();
        if (vflip) {
            vbasis = vbasis.scalar(-1.0f);
        }

        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"info_overlay");

        const auto material = TextureSource::getTextureName(
            o.texinfo,
            m_bsp.texinfos,
            m_bsp.texdatas,
            m_config.fixCubemapTextures ? m_texsrc.getFixedTextureNames() : m_bsp.texnames);
        m_writer.put(u"material", material.value_or(ToolTexture::kHint));
        m_writer.put(u"StartU", static_cast<double>(o.u[0]));
        m_writer.put(u"EndU", static_cast<double>(o.u[1]));
        m_writer.put(u"StartV", static_cast<double>(o.v[0]));
        m_writer.put(u"EndV", static_cast<double>(o.v[1]));
        m_writer.put(u"BasisOrigin", o.origin);
        m_writer.put(u"BasisU", ubasis);
        m_writer.put(u"BasisV", vbasis);
        m_writer.put(u"BasisNormal", o.basisNormal);
        m_writer.put(u"origin", o.origin);

        if (!m_bsp.overlayFades.empty()
            && overlayI < static_cast<int>(m_bsp.overlayFades.size())) {
            const DOverlayFade& of = m_bsp.overlayFades[static_cast<std::size_t>(overlayI)];
            m_writer.put(u"fademindist", static_cast<double>(of.fadeDistMinSq));
            m_writer.put(u"fademaxdist", static_cast<double>(of.fadeDistMaxSq));
        }

        if (!m_bsp.overlaySysLevels.empty()
            && overlayI < static_cast<int>(m_bsp.overlaySysLevels.size())) {
            const DOverlaySystemLevel& osl = m_bsp.overlaySysLevels[static_cast<std::size_t>(overlayI)];
            m_writer.put(u"mincpulevel", osl.minCPULevel);
            m_writer.put(u"maxcpulevel", osl.maxCPULevel);
            m_writer.put(u"mingpulevel", osl.minGPULevel);
            m_writer.put(u"maxgpulevel", osl.maxGPULevel);
        }

        for (int j = 0; j < 4; ++j) {
            m_writer.put(QStringLiteral("uv%1").arg(j), o.uvpoints[static_cast<std::size_t>(j)].withZ(0.0f));
        }

        m_writer.put(u"RenderOrder", o.getRenderOrder());

        QSet<int> sideSet;
        const int faceCount = std::min(o.getFaceCount(), static_cast<int>(o.ofaces.size()));

        if (m_config.brushMode == BrushMode::BrushPlanes) {
            for (int j = 0; j < faceCount; ++j) {
                const int faceIdx = o.ofaces[static_cast<std::size_t>(j)];
                if (faceIdx < 0 || faceIdx >= static_cast<int>(m_bsp.faces.size())) {
                    continue;
                }
                const DFace& dFace = m_bsp.faces[static_cast<std::size_t>(faceIdx)];

                if (dFace.dispInfo >= 0) {
                    sideSet.insert(m_vmfmeta.getDispInfoUID(dFace.dispInfo));
                } else {
                    const int origFaceI = dFace.origFace;
                    if (origFaceI < 0) {
                        continue;
                    }

                    for (int brushSideI : m_brushSideFaceMapper.getBrushSideIndices(origFaceI)) {
                        const int brushSideId = m_brushsrc.getBrushSideIDForIndex(brushSideI);
                        if (brushSideId >= 0) {
                            sideSet.insert(brushSideId);
                        } else {
                            m_config.log(
                                BspLogLevel::Warning,
                                QStringLiteral("Face %1 used by overlay %2 is mapped to brushside %3, which was never written.")
                                    .arg(overlayI)
                                    .arg(origFaceI)
                                    .arg(brushSideI));
                        }
                    }
                }
            }
        } else {
            for (int j = 0; j < faceCount; ++j) {
                const int iface = o.ofaces[static_cast<std::size_t>(j)];
                const int faceId = m_vmfmeta.getFaceUID(iface);
                if (faceId != -1) {
                    sideSet.insert(faceId);
                }
            }
        }

        std::vector<int> sortedSides(sideSet.cbegin(), sideSet.cend());
        std::sort(sortedSides.begin(), sortedSides.end());
        m_writer.put(u"sides", joinSortedIntSet(sortedSides));

        if (const auto nameIt = m_overlayNames.constFind(o.id); nameIt != m_overlayNames.constEnd()) {
            m_writer.put(u"targetname", nameIt.value());
        }

        m_writer.end(u"entity");
    }
}

void EntitySource::writeStaticProps()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing prop_statics"));

    std::vector<std::pair<Vector3f, QString>> lightingOrigins;

    auto findOrAddLightingOrigin = [&lightingOrigins](const Vector3f& origin) -> QString {
        for (const auto& [existingOrigin, name] : lightingOrigins) {
            if (existingOrigin == origin) {
                return name;
            }
        }
        const QString name = QStringLiteral("sprp_lighting_%1").arg(lightingOrigins.size());
        lightingOrigins.emplace_back(origin, name);
        return name;
    };

    for (const DStaticProp& pst : m_bsp.staticProps) {
        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"prop_static");
        m_writer.put(u"origin", pst.origin);
        m_writer.put(u"angles", pst.angles);
        m_writer.put(u"skin", pst.skin);
        m_writer.put(u"fademindist", static_cast<double>(pst.fademin == 0.0f ? -1.0f : pst.fademin));
        m_writer.put(u"fademaxdist", static_cast<double>(pst.fademax));
        m_writer.put(u"solid", pst.solid);

        if (pst.propType >= 0 && pst.propType < static_cast<int>(m_bsp.staticPropName.size())) {
            m_writer.put(u"model", m_bsp.staticPropName[static_cast<std::size_t>(pst.propType)]);
        }

        const bool screenSpaceFade = pst.hasV10Csgo ? false : pst.hasScreenSpaceFadeInPixels();
        m_writer.put(u"screenspacefade", screenSpaceFade);
        m_writer.put(u"ignorenormals", pst.hasIgnoreNormals());
        m_writer.put(u"disableselfshadowing", pst.hasNoSelfShadowing());
        m_writer.put(u"disablevertexlighting", pst.hasNoPerVertexLighting());

        if (pst.usesLightingOrigin()) {
            m_writer.put(u"lightingorigin", findOrAddLightingOrigin(pst.lightingOrigin));
        }

        m_writer.put(u"disableshadows", pst.hasNoShadowing());

        if (pst.hasV5) {
            m_writer.put(u"fadescale", static_cast<double>(pst.forcedFadeScale));
        }

        if (pst.hasV6) {
            m_writer.put(u"maxdxlevel", pst.maxDXLevel);
            m_writer.put(u"mindxlevel", pst.minDXLevel);
        }

        if (pst.hasVinScaling) {
            m_writer.put(u"scale", pst.vinScaling);
        }

        if (pst.hasV8) {
            m_writer.put(u"maxcpulevel", pst.maxCPULevel);
            m_writer.put(u"mincpulevel", pst.minCPULevel);
            m_writer.put(u"maxgpulevel", pst.maxGPULevel);
            m_writer.put(u"mingpulevel", pst.minGPULevel);
        }

        if (pst.hasDiffuseModulation && !pst.isV11Lite) {
            m_writer.put(
                u"rendercolor",
                pst.diffuseModulation.r,
                pst.diffuseModulation.g,
                pst.diffuseModulation.b);
            m_writer.put(u"renderamt", pst.diffuseModulation.a);
        }

        if (pst.hasV9) {
            m_writer.put(u"disableX360", pst.disableX360);
        }

        if (pst.hasTargetName) {
            m_writer.put(u"targetname", pst.targetname);
        }

        if (pst.hasV10) {
            const bool genLightmaps = !pst.hasNoPerTexelLighting();
            m_writer.put(u"generatelightmaps", genLightmaps);
            if (genLightmaps) {
                m_writer.put(u"lightmapresolutionx", pst.lightmapResolutionX);
                m_writer.put(u"lightmapresolutiony", pst.lightmapResolutionY);
            }
        }

        if (pst.hasDiffuseModulation && pst.isV11Lite) {
            if (pst.diffuseModulation.rgba != -1) {
                m_writer.put(
                    u"rendercolor",
                    pst.diffuseModulation.r,
                    pst.diffuseModulation.g,
                    pst.diffuseModulation.b);
                m_writer.put(u"renderamt", pst.diffuseModulation.a);
            }
        }

        if (pst.hasV10Csgo) {
            m_writer.put(u"drawinfastreflection", pst.hasRenderInFastReflection());
            m_writer.put(u"disableshadowdepth", pst.hasDisableShadowDepth());
            m_writer.put(u"enablelightbounce", pst.hasEnableLightBounce());
        }

        if (pst.hasV11Csgo) {
            m_writer.put(u"uniformscale", static_cast<double>(pst.uniformScale));
        }

        if (pst.hasV13) {
            m_writer.put(u"scale", pst.scale);
        }

        m_writer.end(u"entity");
    }

    for (const auto& [origin, targetName] : lightingOrigins) {
        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"info_lighting");
        m_writer.put(u"targetname", targetName);
        m_writer.put(u"origin", origin);
        m_writer.end(u"entity");
    }
}

void EntitySource::writeCubemaps()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing env_cubemaps"));

    for (int i = 0; i < static_cast<int>(m_bsp.cubemaps.size()); ++i) {
        const DCubemapSample& cm = m_bsp.cubemaps[static_cast<std::size_t>(i)];

        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"env_cubemap");
        m_writer.put(u"origin", cm.origin[0], cm.origin[1], cm.origin[2]);
        m_writer.put(u"cubemapsize", cm.size);

        std::vector<int> sideList = m_texsrc.getBrushSidesForCubemap(i);
        const int cmSides = static_cast<int>(sideList.size());
        if (cmSides > 0 && cmSides < m_config.maxCubemapSides) {
            std::sort(sideList.begin(), sideList.end());
            m_writer.put(u"sides", joinSortedIntSet(sideList));
        }

        m_writer.end(u"entity");
    }
}

void EntitySource::writeLadders()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing func_ladders"));

    const int limit = std::min(m_brushsrc.getWorldbrushes(), static_cast<int>(m_bsp.brushes.size()));
    for (int i = 0; i < limit; ++i) {
        const DBrush& brush = m_bsp.brushes[static_cast<std::size_t>(i)];
        if (!brush.isLadder()) {
            continue;
        }

        m_writer.start(u"entity");
        m_writer.put(u"id", m_vmfmeta.getUID());
        m_writer.put(u"classname", u"func_ladder");

        m_brushsrc.writeBrush(i);

        m_writer.end(u"entity");
    }
}

void EntitySource::processEntities()
{
    for (BspEntity& ent : m_bsp.entities) {
        const QString className = ent.className();

        if (className == QStringLiteral("worldspawn")) {
            ent.removeValue(u"world_mins");
            ent.removeValue(u"world_maxs");
            ent.removeValue(u"hammerid");

            if (!ent.hasKey(u"mapversion")) {
                ent.setValue(QStringLiteral("mapversion"), m_bspFile.revision());
            }
        }

        if (m_config.sourceFormat != SourceFormat::Auto) {
            const QChar srcSep = (m_config.sourceFormat == SourceFormat::New)
                ? QLatin1Char(',')
                : QChar(0x1b);
            const QChar dstSep = (m_config.sourceFormat == SourceFormat::New)
                ? QChar(0x1b)
                : QLatin1Char(',');

            for (EntityKeyValue& kv : ent.mutableIo()) {
                kv.value.replace(srcSep, dstSep);
            }
        }

        if (m_bspFile.appId() == SourceAppId::kVampireBloodlines) {
            for (EntityKeyValue& kv : ent.mutableKeyValues()) {
                kv.value.replace(QStringLiteral("\\\""), QString{});
            }
            for (EntityKeyValue& kv : ent.mutableIo()) {
                kv.value.replace(QStringLiteral("\\\""), QString{});
            }
        }

        if (className == QStringLiteral("func_simpleladder")) {
            const int modelNum = ent.modelNum();
            const auto hammerId = ent.value(u"hammerid");

            ent.clear();
            ent.setClassName(QStringLiteral("func_ladder"));
            ent.setModelNum(modelNum);

            if (hammerId.has_value()) {
                ent.setValue(QStringLiteral("hammerid"), *hammerId);
            }
        }

        const QString updatedClassName = ent.className();
        if (updatedClassName.startsWith(QStringLiteral("light"))
            && updatedClassName != QStringLiteral("light_dynamic")) {
            fixLightEntity(ent);
        }

        if (updatedClassName.startsWith(QStringLiteral("info_player_"))) {
            createCamera(ent);
        }

        const int hammerid = getHammerID(ent);
        if (hammerid != -1) {
            m_vmfmeta.getUIDBlackList().insert(hammerid);
        }
    }
}

int EntitySource::getHammerID(const BspEntity& ent) const
{
    const auto hammeridStr = ent.value(u"hammerid");
    if (!hammeridStr.has_value()) {
        return -1;
    }

    bool ok = false;
    const int hammerid = hammeridStr->toInt(&ok);
    if (!ok) {
        m_config.log(
            BspLogLevel::Warning,
            QStringLiteral("Invalid hammerid format %1").arg(*hammeridStr));
        return -1;
    }

    return hammerid;
}

void EntitySource::fixLightEntity(BspEntity& ent) const
{
    const auto style = ent.value(u"style");
    const auto defaultStyle = ent.value(u"defaultstyle");

    if (!style.has_value()) {
        return;
    }

    bool ok = false;
    const int styleVal = style->toInt(&ok);
    if (ok) {
        if (styleVal < 32) {
            return;
        }
    } else {
        m_config.log(
            BspLogLevel::Warning,
            QStringLiteral("Invalid light style number format: %1").arg(*style));
    }

    if (defaultStyle.has_value()) {
        ent.setValue(QStringLiteral("style"), *defaultStyle);
        ent.removeValue(u"defaultstyle");
    } else {
        ent.removeValue(u"style");
    }
}

void EntitySource::createCamera(const BspEntity& ent)
{
    const auto originOpt = ent.origin();
    if (!originOpt.has_value()) {
        return;
    }
    const Vector3d origin = originOpt->toDouble();

    const auto anglesOpt = ent.angles();
    const Vector3d angles = anglesOpt.has_value() ? anglesOpt->toDouble() : Vector3d::kNull;

    Vector3d pos = origin.add(Vector3d{0.0, 0.0, 64.0});
    const Vector3d look = Vector3d{192.0, 0.0, 0.0}.rotate(angles).add(origin);
    pos = look.sub(pos).normalize().scalar(-64.0).add(pos);

    m_vmfmeta.getCameras().push_back(Camera{pos, look});
}

void EntitySource::setNmo(const NmoData& nmoData)
{
    m_nmo = nmoData;

    for (const NmoObjective& nmoObjective : nmoData.nodes) {
        m_vmfmeta.reserveVisgroupId(
            nmoObjective.id,
            {QStringLiteral("Objectives"), nmoObjective.name});
    }

    for (const NmoAntiObjective& nmoAntiObjective : nmoData.antiNodes) {
        m_vmfmeta.reserveVisgroupId(
            nmoAntiObjective.id,
            {QStringLiteral("Objectives"), QStringLiteral("anti"), nmoAntiObjective.name});
    }

    for (const NmoExtraction& extraction : nmoData.extractions) {
        m_vmfmeta.getUIDBlackList().insert(extraction.id);
    }
}

void EntitySource::writeDebugPortals(
    const BspData& bsp,
    FaceSource& faceSource,
    WindingFactory& windingFactory,
    VmfWriter& writer,
    VmfMeta& vmfMeta)
{
    for (int iAreaportal = 0; iAreaportal < static_cast<int>(bsp.areaportals.size()); ++iAreaportal) {
        const DAreaportal& areaportal = bsp.areaportals[static_cast<std::size_t>(iAreaportal)];

        writer.start(u"entity");
        writer.put(u"id", vmfMeta.getUID());
        writer.put(u"classname", u"func_detail");
        writer.put(u"iAreaportal", iAreaportal);
        writer.put(u"portalKey", areaportal.portalKey);
        writer.put(u"otherPortal", areaportal.otherportal);

        faceSource.writePolygon(windingFactory.fromAreaportal(bsp, areaportal), ToolTexture::kSkip, false);
        vmfMeta.writeMetaVisgroups({
            vmfMeta.visgroups()
                ->getVisgroup(QStringLiteral("debug"))
                ->getVisgroup(QStringLiteral("areaportal-visualization"))
                ->getVisgroup(QString::number(areaportal.portalKey)),
        });

        writer.end(u"entity");
    }
}

void EntitySource::writeDebugOccluders(
    const BspData& bsp,
    FaceSource& faceSource,
    WindingFactory& windingFactory,
    VmfWriter& writer,
    VmfMeta& vmfMeta)
{
    for (int iOccluder = 0; iOccluder < static_cast<int>(bsp.occluderDatas.size()); ++iOccluder) {
        const DOccluderData& occluder = bsp.occluderDatas[static_cast<std::size_t>(iOccluder)];
        for (int sideOfOccluder = 0; sideOfOccluder < occluder.polycount; ++sideOfOccluder) {
            const int polyIdx = occluder.firstpoly + sideOfOccluder;
            if (polyIdx < 0 || polyIdx >= static_cast<int>(bsp.occluderPolyDatas.size())) {
                continue;
            }
            const DOccluderPolyData& occluderPoly = bsp.occluderPolyDatas[static_cast<std::size_t>(polyIdx)];

            writer.start(u"entity");
            writer.put(u"id", vmfMeta.getUID());
            writer.put(u"classname", u"func_detail");
            writer.put(u"iOccluder", iOccluder);
            writer.put(u"side", sideOfOccluder);
            writer.put(u"flags", occluder.flags);
            writer.put(u"iOccluderPoly", polyIdx);
            writer.put(u"planenum", occluderPoly.planenum);

            faceSource.writePolygon(windingFactory.fromOccluder(bsp, occluderPoly), ToolTexture::kSkip, false);
            vmfMeta.writeMetaVisgroups({
                vmfMeta.visgroups()
                    ->getVisgroup(QStringLiteral("debug"))
                    ->getVisgroup(QStringLiteral("occluder-visualization"))
                    ->getVisgroup(QString::number(iOccluder)),
            });

            writer.end(u"entity");
        }
    }
}

} // namespace Domain::Bsp::Internal
