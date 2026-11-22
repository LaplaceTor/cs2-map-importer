#include "Domain/Bsp/Internal/BspDecompilerModules.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QRegularExpression>
#include <QStringView>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
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

QString formatFloatArray4(const float (&arr)[4])
{
    return QStringLiteral("[%1, %2, %3, %4]")
        .arg(QString::number(arr[0], 'g', 9),
             QString::number(arr[1], 'g', 9),
             QString::number(arr[2], 'g', 9),
             QString::number(arr[3], 'g', 9));
}

void appendIntUtf8(QByteArray& out, int value)
{
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        out.append(buf, static_cast<qsizetype>(ptr - buf));
    }
}

void appendJavaFloatUtf8(QByteArray& out, float value)
{
    if (std::isnan(value)) {
        out.append("NaN", 3);
        return;
    }
    if (std::isinf(value)) {
        if (value < 0.0f) {
            out.append("-Infinity", 9);
        } else {
            out.append("Infinity", 8);
        }
        return;
    }

    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf) - 2, value);
    if (ec == std::errc{}) {
        const std::string_view sv(buf, static_cast<std::size_t>(ptr - buf));
        out.append(sv.data(), static_cast<qsizetype>(sv.size()));
        if (sv.find('.') == std::string_view::npos
            && sv.find('e') == std::string_view::npos
            && sv.find('E') == std::string_view::npos) {
            out.append(".0", 2);
        }
        return;
    }

    out.append(QByteArray::number(static_cast<double>(value), 'g', 9));
}

QByteArrayView formatRowKey(char (&buf)[16], int row) noexcept
{
    buf[0] = 'r';
    buf[1] = 'o';
    buf[2] = 'w';
    auto [ptr, ec] = std::to_chars(buf + 3, buf + sizeof(buf), row);
    if (ec != std::errc{}) {
        return QByteArrayView("row0");
    }
    return QByteArrayView(buf, static_cast<qsizetype>(ptr - buf));
}

QByteArray joinSortedIntSet(const std::vector<int>& values)
{
    QByteArray out;
    out.reserve(static_cast<qsizetype>(values.size() * 6));
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out.append(' ');
        }
        appendIntUtf8(out, values[i]);
    }
    return out;
}

void setIndexedId(std::vector<int>& vec, int index, int id)
{
    if (index < 0) {
        return;
    }
    const auto idx = static_cast<std::size_t>(index);
    if (idx >= vec.size()) {
        vec.resize(idx + 1, -1);
    }
    vec[idx] = id;
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
    , m_bspFile(reader.bspFile())
    , m_bsp(reader.data())
    , m_writer(writer)
    , m_config(config)
    , m_texsrc(texsrc)
    , m_bspprot(bspprot)
    , m_vmfmeta(vmfmeta)
    , m_brushSideFaceMapper(brushSideFaceMapper)
    , m_windingFactory(windingFactory)
    , m_occReallocationData(occReallocationData)
    , m_brushSideToId(m_bsp.brushSides.size(), -1)
    , m_brushIndexToId(m_bsp.brushes.size(), -1)
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

int BrushSource::brushSideIdForIndex(int ibrushside) const
{
    if (ibrushside >= 0 && static_cast<std::size_t>(ibrushside) < m_brushSideToId.size()) {
        return m_brushSideToId[static_cast<std::size_t>(ibrushside)];
    }
    return -1;
}

int BrushSource::brushIdForIndex(int ibrush) const
{
    if (ibrush >= 0 && static_cast<std::size_t>(ibrush) < m_brushIndexToId.size()) {
        return m_brushIndexToId[static_cast<std::size_t>(ibrush)];
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

    m_worldbrushes = tl.maxBrushLeaf() + 1;

    for (const DModel& model : m_bsp.models) {
        tl.reset();
        tl.walk(model.headnode);

        DBrushModel bmodel;
        bmodel.fstbrush = tl.minBrushLeaf();
        bmodel.numbrush = tl.maxBrushLeaf() - tl.minBrushLeaf() + 1;
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
    const int brushID = m_vmfmeta.allocateUid();
    setIndexedId(m_brushIndexToId, ibrush, brushID);

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

        Winding wind = m_windingFactory.fromSide(m_bsp, ibrush, ibrushside);
        wind.removeDegeneratedInPlace();

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
            wind.rotateInPlace(*angles);
        }
        if (origin.has_value()) {
            wind.translateInPlace(*origin);
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

    m_writer.start("solid");
    m_writer.put("id", brushID);

    if (m_config.debug) {
        m_writer.start("bspsrc_debug");
        m_writer.put("brush_index", ibrush);
        m_writer.put("brush_contents", QStringView(formatBrushFlagsBracketed(brush.contents)));
        m_writer.end("bspsrc_debug");
    }

    for (const auto& [ibrushside, wind] : validBrushSides) {
        writeSide(ibrushside, ibrush, wind, origin, angles);
    }

    if (!brush.isDetail() && m_bspprot.isProtectedBrushIndex(ibrush)) {
        m_vmfmeta.writeMetaVisgroup(QStringLiteral("VMEX protector brushes"));
    }

    m_writer.end("solid");
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

    const Vector3d ev12 = e2 - e1;
    const Vector3d ev13 = e3 - e1;
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
            ? m_texsrc.fixedTextureNames()[static_cast<std::size_t>(texdata->texname)]
            : *texname;
        textureOpt = TextureBuilder::buildFromTexinfo(*texinfo, *texdata, usedTexname, origin, angles, normal);
    }

    Texture texture = textureOpt.has_value()
        ? std::move(*textureOpt)
        : TextureBuilder::buildFromNormal(normal, ToolTexture::kSkip);

    const bool noFaceForSide = !m_brushSideFaceMapper.origFaceIndex(ibrushside).has_value();
    std::optional<QString> fixedToolTexture;
    if (m_config.fixToolTextures && noFaceForSide) {
        const bool isOccluderBrush = m_occReallocationData.isOccluderBrush(ibrush);
        const bool isOccluderBrushSide = m_occReallocationData.isOccluderBrushSide(ibrush, ibrushside - brush.fstside);

        fixedToolTexture = TextureBuilder::fixToolTexture(
            texture.texture,
            isOccluderBrush,
            isOccluderBrushSide,
            brush.contents,
            texinfo == nullptr ? std::nullopt : std::optional<SurfaceFlags>(texinfo->flags),
            m_texsrc.toolTextureMatcher());
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

    const int sideID = m_vmfmeta.allocateUid();

    if (texdata != nullptr) {
        m_texsrc.addBrushSideID(texdata->texname, sideID);
    }

    setIndexedId(m_brushSideToId, ibrushside, sideID);

    int smoothingGroups = 0;
    if (const auto origFaceIdx = m_brushSideFaceMapper.origFaceIndex(ibrushside); origFaceIdx.has_value()) {
        if (*origFaceIdx >= 0 && *origFaceIdx < static_cast<int>(m_bsp.origFaces.size())) {
            smoothingGroups = m_bsp.origFaces[static_cast<std::size_t>(*origFaceIdx)].smoothingGroups;
        }
    }

    m_writer.start("side");
    m_writer.put("id", sideID);

    if (m_config.debug) {
        m_writer.start("bspsrc_debug");
        m_writer.put("brushside_index", ibrushside);
        m_writer.put("normal", normal);
        m_writer.put("winding", QStringView(wind.toString()));

        if (!texname.has_value() || texture.texture != *texname) {
            m_writer.put("original_material", QStringView(texture.texture));
        }

        if (brushSide.texinfo != -1 && brushSide.texinfo >= 0
            && brushSide.texinfo < static_cast<int>(m_bsp.texinfos.size())) {
            const DTexInfo& ti = m_bsp.texinfos[static_cast<std::size_t>(brushSide.texinfo)];
            m_writer.put("texinfo_index", brushSide.texinfo);
            m_writer.put("texinfo_flags", QStringView(formatSurfaceFlagsBracketed(ti.flags)));
            m_writer.put("texturevecs_u", QStringView(formatFloatArray4(ti.textureVecsTexels[0])));
            m_writer.put("texturevecs_v", QStringView(formatFloatArray4(ti.textureVecsTexels[1])));

            const Vector3d uaxis{ti.textureVecsTexels[0][0], ti.textureVecsTexels[0][1], ti.textureVecsTexels[0][2]};
            const Vector3d vaxis{ti.textureVecsTexels[1][0], ti.textureVecsTexels[1][1], ti.textureVecsTexels[1][2]};
            Vector3d texNorm = uaxis.cross(vaxis);
            double angle = radiansToDegrees(std::acos(normal.dot(texNorm) / texNorm.length()));
            m_writer.put("input_uv_normal", texNorm);
            m_writer.put("input_uv_angle", std::isnan(angle) ? 0.0 : angle);

            texNorm = texture.u.axis.cross(texture.v.axis);
            angle = radiansToDegrees(std::acos(normal.dot(texNorm)));
            m_writer.put("output_uv_normal", texNorm);
            m_writer.put("output_uv_angle", std::isnan(angle) ? 0.0 : angle);
        }
        m_writer.end("bspsrc_debug");
    }

    m_writer.put("plane", e1, e2, e3);
    m_writer.put("smoothing_groups", smoothingGroups);
    m_writer.put(texture);

    m_writer.end("side");
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
    , m_bspFile(reader.bspFile())
    , m_bsp(reader.data())
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

    Winding wind = m_windingFactory.fromFace(m_bsp, iface, orig);

    if (angles.has_value()) {
        wind.rotateInPlace(*angles);
    }
    if (origin.has_value()) {
        wind.translateInPlace(*origin);
    }

    const auto plane = wind.buildPlane();
    const Vector3d& e1 = plane[0];
    const Vector3d& e2 = plane[1];
    const Vector3d& e3 = plane[2];

    if (!e1.isValid() || !e2.isValid() || !e3.isValid()) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Face with wind %1 is invalid").arg(wind.toString()));
        return;
    }

    const Vector3d ev12 = e2 - e1;
    const Vector3d ev13 = e3 - e1;
    const Vector3d normal = ev12.cross(ev13).normalize();

    if (normal.isNaN() || normal.isInfinite()) {
        if (m_config.debug) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Bad normal for face %1").arg(iface));
        }
        return;
    }

    m_writer.start("solid");
    m_writer.put("id", m_vmfmeta.allocateUid());

    if (m_config.debug) {
        m_writer.start("bspsrc_debug");
        m_writer.put("face_index", iface);
        m_writer.put("normal", normal);
        m_writer.put("winding", QStringView(wind.toString()));

        if (face.texinfo != -1 && face.texinfo >= 0
            && face.texinfo < static_cast<int>(m_bsp.texinfos.size())) {
            m_writer.put("texinfo_index", face.texinfo);
            m_writer.put(
                "texinfo_flags",
                QStringView(formatSurfaceFlagsBracketed(m_bsp.texinfos[static_cast<std::size_t>(face.texinfo)].flags)));
        }
        m_writer.end("bspsrc_debug");
    }

    const int sideID = m_vmfmeta.allocateUid();
    if (orig) {
        m_vmfmeta.setOrigFaceUid(iface, sideID);
    } else {
        m_vmfmeta.setFaceUid(iface, sideID);
    }

    const DTexInfo* texinfo = TextureBuilder::lookupTexinfo(face.texinfo, m_bsp.texinfos, m_config);
    const DTexData* texdata = texinfo == nullptr
        ? nullptr
        : TextureBuilder::lookupTexdata(texinfo->texdata, m_bsp.texdatas, m_config);
    std::optional<QString> texname = texdata == nullptr
        ? std::nullopt
        : TextureBuilder::lookupTexname(
            texdata->texname,
            m_config.fixCubemapTextures ? m_texsrc.fixedTextureNames() : m_bsp.texnames,
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

    m_writer.start("side");
    m_writer.put("id", sideID);
    m_writer.put("plane", e1, e2, e3);
    m_writer.put("smoothing_groups", face.smoothingGroups);
    m_writer.put(texture);

    const bool disp = face.dispInfo != -1;
    if (disp && m_config.writeDisp) {
        m_vmfmeta.setDispInfoUid(face.dispInfo, sideID);
        writeDisplacement(face.dispInfo);
    }

    m_writer.end("side");

    if (!m_config.backfaceTexture.isEmpty()) {
        texture.texture = m_config.backfaceTexture;
    }

    if (disp) {
        writePrismBack(wind, texture);
    } else {
        writePyramBack(wind, texture);
    }

    m_writer.end("solid");
}

void FaceSource::writePrismBack(const Winding& wind, Texture texture, double depth)
{
    const auto plane = wind.buildPlane();
    Vector3d e1 = plane[0];
    Vector3d e2 = plane[1];
    Vector3d e3 = plane[2];

    const Vector3d ev12 = e2 - e1;
    const Vector3d ev13 = e3 - e1;
    const Vector3d normal = ev12.cross(ev13).normalize();

    const Vector3d bedge = normal * depth;
    e1 += bedge;
    e2 += bedge;
    e3 += bedge;

    texture.u = TextureAxis{texture.u.axis * -1.0};
    writeBackSide(texture, e1, e2, e3);

    const Vector3d tv2 = bedge.normalize();
    const std::size_t size = wind.size();

    for (std::size_t i = 0; i < size; ++i) {
        e1 = wind[i];
        e2 = wind[(i + 1) % size];
        e3 = e1 + bedge;

        Vector3d tv1 = (e1 - e2).normalize();
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

    const Vector3d ev12 = e2 - e1;
    const Vector3d ev13 = e3 - e1;
    const Vector3d normal = ev12.cross(ev13).normalize();

    e3 = wind.center() + (normal * depth);

    const std::size_t size = wind.size();
    for (std::size_t i = 0; i < size; ++i) {
        e1 = wind[i];
        e2 = wind[(i + 1) % size];
        writeBackSide(texture, e1, e2, e3);
    }
}

void FaceSource::writePyramBack(const Winding& wind, const Texture& texture)
{
    writePyramBack(wind, texture, static_cast<double>(m_config.backfaceDepth));
}

void FaceSource::writeAreaportal(int portalKey)
{
    for (int i = 0; i < static_cast<int>(m_bsp.areaportals.size()); ++i) {
        if (m_bsp.areaportals[static_cast<std::size_t>(i)].portalKey == portalKey) {
            const Winding& wind = m_windingFactory.fromAreaportal(m_bsp, i);
            writePolygon(wind, ToolTexture::kAreaportal, true);
            return;
        }
    }
    m_config.log(
        BspLogLevel::Warning,
        QStringLiteral("Tried to write non existing areaportal with portalkey %1").arg(portalKey));
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
        const Winding& wind = m_windingFactory.fromOccluder(m_bsp, polyIndex);
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

    const Vector3d ev12 = e2 - e1;
    const Vector3d ev13 = e3 - e1;
    const Vector3d normal = ev12.cross(ev13).normalize();

    if (normal.isNaN() || normal.isInfinite()) {
        if (m_config.debug) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Bad normal in writePolygon"));
        }
        return;
    }

    m_writer.start("solid");
    m_writer.put("id", m_vmfmeta.allocateUid());

    const int sideID = m_vmfmeta.allocateUid();
    Texture texture = TextureBuilder::buildFromNormal(normal, frontMaterial);

    m_writer.start("side");
    m_writer.put("id", sideID);
    m_writer.put("plane", e1, e2, e3);
    m_writer.put(texture);
    m_writer.end("side");

    texture.texture = backMaterial;

    if (prism) {
        writePrismBack(wind, texture, depth);
    } else {
        writePyramBack(wind, texture, depth);
    }

    m_writer.end("solid");
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
    m_writer.start("side");
    m_writer.put("id", m_vmfmeta.allocateUid());
    m_writer.put("plane", e1, e3, e2);
    m_writer.put("smoothing_groups", 0);
    m_writer.put(texture);
    m_writer.end("side");
}

void FaceSource::writeDisplacement(int idispinfo)
{
    if (idispinfo < 0 || idispinfo >= static_cast<int>(m_bsp.dispinfos.size())) {
        return;
    }

    const DDispInfo& di = m_bsp.dispinfos[static_cast<std::size_t>(idispinfo)];
    const int vertcount = di.vertexCount();
    const int psize = di.powerSize();
    const int rowVerts = psize + 1;
    const bool hasMultiBlend = !m_bsp.dispmultiblend.empty() && di.hasMultiBlend();

    const int availVerts = (di.dispVertStart >= 0
                            && di.dispVertStart < static_cast<int>(m_bsp.dispverts.size()))
        ? std::min(vertcount, static_cast<int>(m_bsp.dispverts.size()) - di.dispVertStart)
        : 0;
    const int completeRows = rowVerts > 0 ? (availVerts / rowVerts) : 0;

    QByteArray rowBuf;
    rowBuf.reserve(static_cast<qsizetype>(rowVerts * 48));
    char keyBuf[16];

    m_writer.start("dispinfo");

    if (m_config.debug) {
        m_writer.put("bspsrc_dispinfo_index", idispinfo);
    }

    m_writer.put("power", di.power);
    m_writer.put("startposition", di.startPos.toDouble(), 2);
    m_writer.put("flags", di.surfaceFlags());
    m_writer.put("elevation", 0);
    m_writer.put("subdiv", 0);

    m_writer.start("normals");
    for (int row = 0; row < completeRows; ++row) {
        rowBuf.clear();
        const int rowStart = di.dispVertStart + row * rowVerts;
        for (int col = 0; col < rowVerts; ++col) {
            if (col > 0) {
                rowBuf.append(' ');
            }
            const DDispVert& dv = m_bsp.dispverts[static_cast<std::size_t>(rowStart + col)];
            appendJavaFloatUtf8(rowBuf, dv.vector.x);
            rowBuf.append(' ');
            appendJavaFloatUtf8(rowBuf, dv.vector.y);
            rowBuf.append(' ');
            appendJavaFloatUtf8(rowBuf, dv.vector.z);
        }
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("normals");

    m_writer.start("distances");
    for (int row = 0; row < completeRows; ++row) {
        rowBuf.clear();
        const int rowStart = di.dispVertStart + row * rowVerts;
        for (int col = 0; col < rowVerts; ++col) {
            if (col > 0) {
                rowBuf.append(' ');
            }
            appendJavaFloatUtf8(rowBuf, m_bsp.dispverts[static_cast<std::size_t>(rowStart + col)].dist);
        }
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("distances");

    rowBuf.clear();
    for (int col = 0; col < rowVerts; ++col) {
        if (col > 0) {
            rowBuf.append(' ');
        }
        rowBuf.append("0 0 0", 5);
    }

    m_writer.start("offsets");
    for (int row = 0; row < rowVerts; ++row) {
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("offsets");

    rowBuf.clear();
    for (int col = 0; col < rowVerts; ++col) {
        if (col > 0) {
            rowBuf.append(' ');
        }
        rowBuf.append("0 0 1", 5);
    }

    m_writer.start("offset_normals");
    for (int row = 0; row < rowVerts; ++row) {
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("offset_normals");

    m_writer.start("alphas");
    for (int row = 0; row < completeRows; ++row) {
        rowBuf.clear();
        const int rowStart = di.dispVertStart + row * rowVerts;
        for (int col = 0; col < rowVerts; ++col) {
            if (col > 0) {
                rowBuf.append(' ');
            }
            appendJavaFloatUtf8(rowBuf, m_bsp.dispverts[static_cast<std::size_t>(rowStart + col)].alpha);
        }
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("alphas");

    const int tcount = di.triangleTagCount();
    const int triRowCols = 2 * psize;
    const int triRows = triRowCols > 0 ? (tcount / triRowCols) : 0;

    m_writer.start("triangle_tags");
    for (int row = 0; row < triRows; ++row) {
        rowBuf.clear();
        for (int col = 0; col < triRowCols; ++col) {
            if (col > 0) {
                rowBuf.append(' ');
            }
            const int triIdx = di.dispTriStart + row * triRowCols + col;
            int dt = (triIdx >= 0 && triIdx < static_cast<int>(m_bsp.disptris.size()))
                ? m_bsp.disptris[static_cast<std::size_t>(triIdx)].tags
                : 0;
            if (dt < 0 || dt > 6) {
                dt = 0;
            }
            appendIntUtf8(rowBuf, kTriConv[static_cast<std::size_t>(dt)]);
        }
        m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
    }
    m_writer.end("triangle_tags");

    rowBuf.clear();
    for (std::size_t i = 0; i < di.allowedVerts.size(); ++i) {
        if (i > 0) {
            rowBuf.append(' ');
        }
        appendIntUtf8(rowBuf, di.allowedVerts[i]);
    }

    m_writer.start("allowed_verts");
    m_writer.put("10", QByteArrayView(rowBuf));
    m_writer.end("allowed_verts");

    if (hasMultiBlend) {
        const int availMbVerts = (m_multiblendOffset >= 0
                                  && m_multiblendOffset < static_cast<int>(m_bsp.dispmultiblend.size()))
            ? std::min(availVerts, static_cast<int>(m_bsp.dispmultiblend.size()) - m_multiblendOffset)
            : 0;
        const int mbRows = rowVerts > 0 ? (availMbVerts / rowVerts) : 0;

        m_writer.start("multiblend");
        for (int row = 0; row < mbRows; ++row) {
            rowBuf.clear();
            const int rowStart = m_multiblendOffset + row * rowVerts;
            for (int col = 0; col < rowVerts; ++col) {
                if (col > 0) {
                    rowBuf.append(' ');
                }
                const auto& mb = m_bsp.dispmultiblend[static_cast<std::size_t>(rowStart + col)].multiblend;
                appendJavaFloatUtf8(rowBuf, mb.x);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, mb.y);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, mb.z);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, mb.w);
            }
            m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
        }
        m_writer.end("multiblend");

        m_writer.start("alphablend");
        for (int row = 0; row < mbRows; ++row) {
            rowBuf.clear();
            const int rowStart = m_multiblendOffset + row * rowVerts;
            for (int col = 0; col < rowVerts; ++col) {
                if (col > 0) {
                    rowBuf.append(' ');
                }
                const auto& ab = m_bsp.dispmultiblend[static_cast<std::size_t>(rowStart + col)].alphablend;
                appendJavaFloatUtf8(rowBuf, ab.x);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, ab.y);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, ab.z);
                rowBuf.append(' ');
                appendJavaFloatUtf8(rowBuf, ab.w);
            }
            m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
        }
        m_writer.end("alphablend");

        for (int j = 0; j < DDispMultiBlend::kMaxMultiBlendChannels; ++j) {
            char secBuf[32];
            constexpr QByteArrayView kPrefix("multiblend_color_");
            std::memcpy(secBuf, kPrefix.data(), static_cast<std::size_t>(kPrefix.size()));
            auto [ptr, ec] = std::to_chars(secBuf + kPrefix.size(), secBuf + sizeof(secBuf), j);
            const QByteArrayView sectionName(secBuf, static_cast<qsizetype>(ptr - secBuf));

            m_writer.start(sectionName);
            for (int row = 0; row < mbRows; ++row) {
                rowBuf.clear();
                const int rowStart = m_multiblendOffset + row * rowVerts;
                for (int col = 0; col < rowVerts; ++col) {
                    if (col > 0) {
                        rowBuf.append(' ');
                    }
                    const auto& color = m_bsp.dispmultiblend[static_cast<std::size_t>(rowStart + col)]
                        .multiblendcolors[static_cast<std::size_t>(j)];
                    appendJavaFloatUtf8(rowBuf, color.x);
                    rowBuf.append(' ');
                    appendJavaFloatUtf8(rowBuf, color.y);
                    rowBuf.append(' ');
                    appendJavaFloatUtf8(rowBuf, color.z);
                }
                m_writer.put(formatRowKey(keyBuf, row), QByteArrayView(rowBuf));
            }
            m_writer.end(sectionName);
        }

        m_multiblendOffset += vertcount;
    }

    m_writer.end("dispinfo");
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
            const Winding& wind = m_windingFactory.fromFace(m_bsp, i, true);
            origFace.area = static_cast<float>(wind.area());
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
    , m_bspFile(reader.bspFile())
    , m_bsp(reader.data())
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
        ->childVisgroup(QStringLiteral("Reallocated"))
        ->setColor(QColor(0, 255, 0));

    VmfMeta::Visgroup* rebuildVg = m_vmfmeta.visgroups()
        ->childVisgroup(QStringLiteral("Rebuild"))
        ->setColor(QColor(255, 0, 0));

    VmfMeta::Visgroup* reallocatedAreaportalVg = reallocatedVg
        ->childVisgroup(QStringLiteral("Areaportal"))
        ->setColor(QColor(0, 255, 255));
    VmfMeta::Visgroup* rebuildAreaportalVg = rebuildVg
        ->childVisgroup(QStringLiteral("Areaportal"))
        ->setColor(QColor(0, 178, 178));

    VmfMeta::Visgroup* reallocatedOccluderVg = reallocatedVg
        ->childVisgroup(QStringLiteral("Occluder"))
        ->setColor(QColor(255, 0, 255));
    VmfMeta::Visgroup* rebuildOccluderVg = rebuildVg
        ->childVisgroup(QStringLiteral("Occluder"))
        ->setColor(QColor(178, 0, 178));

    static const QRegularExpression instancePrefixRegex(QStringLiteral("^([^-]+)-"));
    std::vector<VmfMeta::Visgroup*> visgroups;

    for (int entityIdx = 0; entityIdx < static_cast<int>(m_bsp.entities.size()); ++entityIdx) {
        BspEntity& ent = m_bsp.entities[static_cast<std::size_t>(entityIdx)];
        visgroups.clear();

        const QString className = ent.className();

        if (className == QLatin1StringView("worldspawn")) {
            continue;
        }

        if (className == QLatin1StringView("env_sprite")) {
            if (const auto model = ent.value(u"model");
                model.has_value() && model->startsWith(QLatin1StringView("model"))) {
                ent.removeValue(u"scale");
            }
        }

        const bool isAreaportal = className.startsWith(QLatin1StringView("func_areaportal"));
        const bool isOccluder = className == QLatin1StringView("func_occluder");
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
        if (className == QLatin1StringView("info_ladder")) {
            continue;
        }

        if (className == QLatin1StringView("info_overlay_accessor")) {
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

        int entID = hammerId(ent);

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
            entID = m_vmfmeta.allocateUid();
        }

        m_writer.start("entity");
        m_writer.put("id", entID);

        const int modelNum = ent.modelNum();

        for (const EntityKeyValue& kv : ent.keyValues()) {
            const QString& key = kv.key;
            const QString& value = kv.value;

            if (key == QLatin1StringView("angles") && modelNum >= 0 && fixEntityRot) {
                continue;
            }
            if (key == QLatin1StringView("origin") && modelNum == 0) {
                continue;
            }
            if (key == QLatin1StringView("model") && modelNum != -2) {
                continue;
            }
            if (key == QLatin1StringView("hammerid")) {
                continue;
            }
            if ((isAreaportal || isOccluder)
                && (key == QLatin1StringView("angles") || key == QLatin1StringView("origin"))) {
                continue;
            }

            m_writer.put(QStringView(key), QStringView(value));
        }

        m_writer.put("classname", QStringView(className));

        const auto& io = ent.io();
        if (!io.empty()) {
            m_writer.start("connections");
            for (const EntityKeyValue& kv : io) {
                m_writer.put(kv);
            }
            m_writer.end("connections");
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
                            ->childVisgroup(QStringLiteral("AreaportalID"))
                            ->childVisgroup(QString::number(portalNum)));
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
                        ->childVisgroup(QStringLiteral("Instances"))
                        ->childVisgroup(match.captured(1)));
            }
        }

        if (m_bspprot.isProtectedEntityIndex(entityIdx)) {
            visgroups.push_back(
                m_vmfmeta.visgroups()->childVisgroup(QStringLiteral("VMEX flagged entities")));
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
                                ->childVisgroup(QStringLiteral("Objectives"))
                                ->childVisgroup(objective.name));
                    }
                }

                for (const NmoAntiObjective& anti : m_nmo->antiNodes) {
                    const bool matched =
                        std::find(anti.entities.begin(), anti.entities.end(), *targetName) != anti.entities.end();
                    if (matched) {
                        visgroups.push_back(
                            m_vmfmeta.visgroups()
                                ->childVisgroup(QStringLiteral("Objectives"))
                                ->childVisgroup(QStringLiteral("anti"))
                                ->childVisgroup(anti.name));
                    }
                }
            }
        }

        if (!visgroups.empty()) {
            m_vmfmeta.writeMetaVisgroups(visgroups);
        }

        m_writer.end("entity");
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
            const AABB bounds = m_brushBounds.bounds(m_bsp, brushIdx);
            const AABB extendedBounds = bounds.expand(static_cast<double>(m_config.detailMergeThresh));

            QSet<int> intersectingGroupIds;
            for (const auto& [otherBrushIdx, groupId] : brushGroups) {
                if (!intersectingGroupIds.contains(groupId)) {
                    const AABB otherBounds = m_brushBounds.bounds(m_bsp, otherBrushIdx);
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
        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "func_detail");

        for (int brushIdx : group) {
            m_brushsrc.writeBrush(brushIdx);
        }

        m_writer.end("entity");
    }

    const auto& protBrushes = m_bspprot.protectedBrushIndices();
    if (!protBrushes.empty()) {
        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "func_detail");
        m_vmfmeta.writeMetaVisgroup(QStringLiteral("VMEX protector brushes"));

        for (int protBrushIdx : protBrushes) {
            m_brushsrc.writeBrush(protBrushIdx);
        }

        m_writer.end("entity");
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
        const Vector3f size = dLeaf.maxs - dLeaf.mins;
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

        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "func_viscluster");

        m_facesrc.writePolygon(
            Winding(std::array<Vector3d, 4>{
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
                ->childVisgroup(QStringLiteral("Rebuild"))
                ->childVisgroup(QStringLiteral("Vis Cluster"))
                ->childVisgroup(QString::number(cluster)),
        });

        m_writer.end("entity");

        if (m_config.debug) {
            for (const DLeaf* leaf : leaves) {
                m_writer.start("entity");
                m_writer.put("id", m_vmfmeta.allocateUid());
                m_writer.put("classname", "func_detail");

                m_facesrc.writePolygon(
                    Winding(std::array<Vector3d, 4>{
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
                        ->childVisgroup(QStringLiteral("debug"))
                        ->childVisgroup(QStringLiteral("leafs-in-cluster"))
                        ->childVisgroup(QString::number(cluster)),
                });

                m_writer.end("entity");
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
            vbasis = -vbasis;
        }

        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "info_overlay");

        const auto material = TextureSource::textureName(
            o.texinfo,
            m_bsp.texinfos,
            m_bsp.texdatas,
            m_config.fixCubemapTextures ? m_texsrc.fixedTextureNames() : m_bsp.texnames);
        m_writer.put("material", material.has_value() ? QStringView(*material) : QStringView(ToolTexture::kHint));
        m_writer.put("StartU", static_cast<double>(o.u[0]));
        m_writer.put("EndU", static_cast<double>(o.u[1]));
        m_writer.put("StartV", static_cast<double>(o.v[0]));
        m_writer.put("EndV", static_cast<double>(o.v[1]));
        m_writer.put("BasisOrigin", o.origin);
        m_writer.put("BasisU", ubasis);
        m_writer.put("BasisV", vbasis);
        m_writer.put("BasisNormal", o.basisNormal);
        m_writer.put("origin", o.origin);

        if (!m_bsp.overlayFades.empty()
            && overlayI < static_cast<int>(m_bsp.overlayFades.size())) {
            const DOverlayFade& of = m_bsp.overlayFades[static_cast<std::size_t>(overlayI)];
            m_writer.put("fademindist", static_cast<double>(of.fadeDistMinSq));
            m_writer.put("fademaxdist", static_cast<double>(of.fadeDistMaxSq));
        }

        if (!m_bsp.overlaySysLevels.empty()
            && overlayI < static_cast<int>(m_bsp.overlaySysLevels.size())) {
            const DOverlaySystemLevel& osl = m_bsp.overlaySysLevels[static_cast<std::size_t>(overlayI)];
            m_writer.put("mincpulevel", osl.minCPULevel);
            m_writer.put("maxcpulevel", osl.maxCPULevel);
            m_writer.put("mingpulevel", osl.minGPULevel);
            m_writer.put("maxgpulevel", osl.maxGPULevel);
        }

        for (int j = 0; j < 4; ++j) {
            const char uvKey[4] = {'u', 'v', static_cast<char>('0' + j), '\0'};
            m_writer.put(QByteArrayView(uvKey, 3), o.uvpoints[static_cast<std::size_t>(j)].withZ(0.0f));
        }

        m_writer.put("RenderOrder", o.renderOrder());

        QSet<int> sideSet;
        const int faceCount = std::min(o.faceCount(), static_cast<int>(o.ofaces.size()));

        if (m_config.brushMode == BrushMode::BrushPlanes) {
            for (int j = 0; j < faceCount; ++j) {
                const int faceIdx = o.ofaces[static_cast<std::size_t>(j)];
                if (faceIdx < 0 || faceIdx >= static_cast<int>(m_bsp.faces.size())) {
                    continue;
                }
                const DFace& dFace = m_bsp.faces[static_cast<std::size_t>(faceIdx)];

                if (dFace.dispInfo >= 0) {
                    sideSet.insert(m_vmfmeta.dispInfoUid(dFace.dispInfo));
                } else {
                    const int origFaceI = dFace.origFace;
                    if (origFaceI < 0) {
                        continue;
                    }

                    for (int brushSideI : m_brushSideFaceMapper.brushSideIndices(origFaceI)) {
                        const int brushSideId = m_brushsrc.brushSideIdForIndex(brushSideI);
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
                const int faceId = m_vmfmeta.faceUid(iface);
                if (faceId != -1) {
                    sideSet.insert(faceId);
                }
            }
        }

        std::vector<int> sortedSides(sideSet.cbegin(), sideSet.cend());
        std::sort(sortedSides.begin(), sortedSides.end());
        m_writer.put("sides", QByteArrayView(joinSortedIntSet(sortedSides)));

        if (const auto nameIt = m_overlayNames.constFind(o.id); nameIt != m_overlayNames.constEnd()) {
            m_writer.put("targetname", QStringView(nameIt.value()));
        }

        m_writer.end("entity");
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
        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "prop_static");
        m_writer.put("origin", pst.origin);
        m_writer.put("angles", pst.angles);
        m_writer.put("skin", pst.skin);
        m_writer.put("fademindist", static_cast<double>(pst.fademin == 0.0f ? -1.0f : pst.fademin));
        m_writer.put("fademaxdist", static_cast<double>(pst.fademax));
        m_writer.put("solid", pst.solid);

        if (pst.propType >= 0 && pst.propType < static_cast<int>(m_bsp.staticPropName.size())) {
            m_writer.put("model", QStringView(m_bsp.staticPropName[static_cast<std::size_t>(pst.propType)]));
        }

        const bool screenSpaceFade = pst.hasV10Csgo ? false : pst.hasScreenSpaceFadeInPixels();
        m_writer.put("screenspacefade", screenSpaceFade);
        m_writer.put("ignorenormals", pst.hasIgnoreNormals());
        m_writer.put("disableselfshadowing", pst.hasNoSelfShadowing());
        m_writer.put("disablevertexlighting", pst.hasNoPerVertexLighting());

        if (pst.usesLightingOrigin()) {
            m_writer.put("lightingorigin", QStringView(findOrAddLightingOrigin(pst.lightingOrigin)));
        }

        m_writer.put("disableshadows", pst.hasNoShadowing());

        if (pst.hasV5) {
            m_writer.put("fadescale", static_cast<double>(pst.forcedFadeScale));
        }

        if (pst.hasV6) {
            m_writer.put("maxdxlevel", pst.maxDXLevel);
            m_writer.put("mindxlevel", pst.minDXLevel);
        }

        if (pst.hasVinScaling) {
            m_writer.put("scale", pst.vinScaling);
        }

        if (pst.hasV8) {
            m_writer.put("maxcpulevel", pst.maxCPULevel);
            m_writer.put("mincpulevel", pst.minCPULevel);
            m_writer.put("maxgpulevel", pst.maxGPULevel);
            m_writer.put("mingpulevel", pst.minGPULevel);
        }

        if (pst.hasDiffuseModulation && !pst.isV11Lite) {
            m_writer.put(
                "rendercolor",
                pst.diffuseModulation.r,
                pst.diffuseModulation.g,
                pst.diffuseModulation.b);
            m_writer.put("renderamt", pst.diffuseModulation.a);
        }

        if (pst.hasV9) {
            m_writer.put("disableX360", pst.disableX360);
        }

        if (pst.hasTargetName) {
            m_writer.put("targetname", QStringView(pst.targetname));
        }

        if (pst.hasV10) {
            const bool genLightmaps = !pst.hasNoPerTexelLighting();
            m_writer.put("generatelightmaps", genLightmaps);
            if (genLightmaps) {
                m_writer.put("lightmapresolutionx", pst.lightmapResolutionX);
                m_writer.put("lightmapresolutiony", pst.lightmapResolutionY);
            }
        }

        if (pst.hasDiffuseModulation && pst.isV11Lite) {
            if (pst.diffuseModulation.rgba != -1) {
                m_writer.put(
                    "rendercolor",
                    pst.diffuseModulation.r,
                    pst.diffuseModulation.g,
                    pst.diffuseModulation.b);
                m_writer.put("renderamt", pst.diffuseModulation.a);
            }
        }

        if (pst.hasV10Csgo) {
            m_writer.put("drawinfastreflection", pst.hasRenderInFastReflection());
            m_writer.put("disableshadowdepth", pst.hasDisableShadowDepth());
            m_writer.put("enablelightbounce", pst.hasEnableLightBounce());
        }

        if (pst.hasV11Csgo) {
            m_writer.put("uniformscale", static_cast<double>(pst.uniformScale));
        }

        if (pst.hasV13) {
            m_writer.put("scale", pst.scale);
        }

        m_writer.end("entity");
    }

    for (const auto& [origin, targetName] : lightingOrigins) {
        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "info_lighting");
        m_writer.put("targetname", QStringView(targetName));
        m_writer.put("origin", origin);
        m_writer.end("entity");
    }
}

void EntitySource::writeCubemaps()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing env_cubemaps"));

    for (int i = 0; i < static_cast<int>(m_bsp.cubemaps.size()); ++i) {
        const DCubemapSample& cm = m_bsp.cubemaps[static_cast<std::size_t>(i)];

        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "env_cubemap");
        m_writer.put("origin", cm.origin[0], cm.origin[1], cm.origin[2]);
        m_writer.put("cubemapsize", cm.size);

        std::vector<int> sideList = m_texsrc.brushSidesForCubemap(i);
        const int cmSides = static_cast<int>(sideList.size());
        if (cmSides > 0 && cmSides < m_config.maxCubemapSides) {
            std::sort(sideList.begin(), sideList.end());
            m_writer.put("sides", QByteArrayView(joinSortedIntSet(sideList)));
        }

        m_writer.end("entity");
    }
}

void EntitySource::writeLadders()
{
    m_config.log(BspLogLevel::Info, QStringLiteral("Writing func_ladders"));

    const int limit = std::min(m_brushsrc.worldBrushCount(), static_cast<int>(m_bsp.brushes.size()));
    for (int i = 0; i < limit; ++i) {
        const DBrush& brush = m_bsp.brushes[static_cast<std::size_t>(i)];
        if (!brush.isLadder()) {
            continue;
        }

        m_writer.start("entity");
        m_writer.put("id", m_vmfmeta.allocateUid());
        m_writer.put("classname", "func_ladder");

        m_brushsrc.writeBrush(i);

        m_writer.end("entity");
    }
}

void EntitySource::processEntities()
{
    for (BspEntity& ent : m_bsp.entities) {
        const QString className = ent.className();

        if (className == QLatin1StringView("worldspawn")) {
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

        if (className == QLatin1StringView("func_simpleladder")) {
            const int modelNum = ent.modelNum();
            const auto hammerIdVal = ent.value(u"hammerid");

            ent.clear();
            ent.setClassName(QStringLiteral("func_ladder"));
            ent.setModelNum(modelNum);

            if (hammerIdVal.has_value()) {
                ent.setValue(QStringLiteral("hammerid"), *hammerIdVal);
            }
        }

        const QString updatedClassName = ent.className();
        if (updatedClassName.startsWith(QLatin1StringView("light"))
            && updatedClassName != QLatin1StringView("light_dynamic")) {
            fixLightEntity(ent);
        }

        if (updatedClassName.startsWith(QLatin1StringView("info_player_"))) {
            createCamera(ent);
        }

        const int hammerid = hammerId(ent);
        if (hammerid != -1) {
            m_vmfmeta.uidBlacklist().insert(hammerid);
        }
    }
}

int EntitySource::hammerId(const BspEntity& ent) const
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

    Vector3d pos = origin + Vector3d{0.0, 0.0, 64.0};
    const Vector3d look = Vector3d{192.0, 0.0, 0.0}.rotate(angles) + origin;
    pos = (look - pos).normalize() * -64.0 + pos;

    m_vmfmeta.cameras().push_back(Camera{pos, look});
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
        m_vmfmeta.uidBlacklist().insert(extraction.id);
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

        writer.start("entity");
        writer.put("id", vmfMeta.allocateUid());
        writer.put("classname", "func_detail");
        writer.put("iAreaportal", iAreaportal);
        writer.put("portalKey", areaportal.portalKey);
        writer.put("otherPortal", areaportal.otherportal);

        faceSource.writePolygon(windingFactory.fromAreaportal(bsp, iAreaportal), ToolTexture::kSkip, false);
        vmfMeta.writeMetaVisgroups({
            vmfMeta.visgroups()
                ->childVisgroup(QStringLiteral("debug"))
                ->childVisgroup(QStringLiteral("areaportal-visualization"))
                ->childVisgroup(QString::number(areaportal.portalKey)),
        });

        writer.end("entity");
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

            writer.start("entity");
            writer.put("id", vmfMeta.allocateUid());
            writer.put("classname", "func_detail");
            writer.put("iOccluder", iOccluder);
            writer.put("side", sideOfOccluder);
            writer.put("flags", occluder.flags);
            writer.put("iOccluderPoly", polyIdx);
            writer.put("planenum", occluderPoly.planenum);

            faceSource.writePolygon(windingFactory.fromOccluder(bsp, polyIdx), ToolTexture::kSkip, false);
            vmfMeta.writeMetaVisgroups({
                vmfMeta.visgroups()
                    ->childVisgroup(QStringLiteral("debug"))
                    ->childVisgroup(QStringLiteral("occluder-visualization"))
                    ->childVisgroup(QString::number(iOccluder)),
            });

            writer.end("entity");
        }
    }
}

} // namespace Domain::Bsp::Internal
