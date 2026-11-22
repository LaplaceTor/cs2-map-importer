#include "Domain/Bsp/Internal/VmfWriter.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <string_view>
#include <utility>

#include "Domain/Bsp/Internal/BspFileReader.h"

namespace Domain::Bsp::Internal {

namespace {

QString formatVectorDebug(const Vector3d& v)
{
    return QStringLiteral("(%1, %2, %3)")
        .arg(QString::number(v.x, 'g', 15),
             QString::number(v.y, 'g', 15),
             QString::number(v.z, 'g', 15));
}

QString joinVisgroupPath(const std::vector<QString>& path)
{
    QString out;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            out += QLatin1Char('/');
        }
        out += path[i];
    }
    return out;
}

class StackUtf8Buffer {
public:
    void append(char ch)
    {
        append(&ch, 1);
    }

    void append(std::string_view sv)
    {
        append(sv.data(), sv.size());
    }

    void append(const char* data, std::size_t size)
    {
        if (size == 0) {
            return;
        }
        if (m_useHeap) {
            m_heap.append(data, static_cast<qsizetype>(size));
            return;
        }
        if (m_size + size <= kStackCapacity) {
            std::memcpy(m_stack + m_size, data, size);
            m_size += size;
            return;
        }
        m_heap.reserve(static_cast<qsizetype>(m_size + size));
        m_heap.append(m_stack, static_cast<qsizetype>(m_size));
        m_heap.append(data, static_cast<qsizetype>(size));
        m_useHeap = true;
    }

    void appendInt(int value)
    {
        char tmp[32];
        auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), value);
        if (ec == std::errc{}) {
            append(tmp, static_cast<std::size_t>(ptr - tmp));
        }
    }

    void appendFloat(double f, int decimalPlaces)
    {
        if (std::isnan(f)) {
            append("NaN");
            return;
        }
        if (std::isinf(f)) {
            append(f < 0.0 ? "-Infinity" : "Infinity");
            return;
        }

        char tmp[512];
        if (decimalPlaces <= 0) {
            auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), f);
            if (ec == std::errc{}) {
                const std::string_view sv(tmp, static_cast<std::size_t>(ptr - tmp));
                if (sv == "-0") {
                    append("0.0");
                    return;
                }
                append(sv);
                if (sv.find('.') == std::string_view::npos
                    && sv.find('e') == std::string_view::npos
                    && sv.find('E') == std::string_view::npos) {
                    append(".0");
                }
                return;
            }
            const QByteArray fallback = QByteArray::number(f, 'g', 15);
            append(fallback.constData(), static_cast<std::size_t>(fallback.size()));
            return;
        }

        auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), f, std::chars_format::fixed, decimalPlaces);
        std::string_view sv;
        QByteArray fallback;
        if (ec == std::errc{}) {
            sv = std::string_view(tmp, static_cast<std::size_t>(ptr - tmp));
        } else {
            fallback = QByteArray::number(f, 'f', decimalPlaces);
            sv = std::string_view(fallback.constData(), static_cast<std::size_t>(fallback.size()));
        }

        if (sv.find('.') != std::string_view::npos) {
            while (!sv.empty() && sv.back() == '0') {
                sv.remove_suffix(1);
            }
            if (!sv.empty() && sv.back() == '.') {
                sv.remove_suffix(1);
            }
        }

        if (sv == "-0") {
            sv = "0";
        }

        append(sv);
    }

    QByteArrayView view() const noexcept
    {
        if (m_useHeap) {
            return QByteArrayView(m_heap);
        }
        return QByteArrayView(m_stack, static_cast<qsizetype>(m_size));
    }

private:
    static constexpr std::size_t kStackCapacity = 256;
    char m_stack[kStackCapacity]{};
    std::size_t m_size = 0;
    QByteArray m_heap;
    bool m_useHeap = false;
};

template <typename WarnFn>
void appendVector3dFormatted(
    StackUtf8Buffer& out,
    const Vector3d& v,
    int p,
    int doubleScale,
    const WarnFn& onInvalid)
{
    if (p == 1) {
        out.append('(');
    } else if (p == 2) {
        out.append('[');
    }

    if (!v.isValid()) {
        onInvalid(v);
        out.append("0 0 0");
    } else {
        out.appendFloat(v.x, doubleScale);
        out.append(' ');
        out.appendFloat(v.y, doubleScale);
        out.append(' ');
        out.appendFloat(v.z, doubleScale);
    }

    if (p == 1) {
        out.append(')');
    } else if (p == 2) {
        out.append(']');
    }
}

std::optional<int> setIndexedUid(std::vector<int>& vec, int index, int id)
{
    if (index < 0) {
        return std::nullopt;
    }
    const auto idx = static_cast<std::size_t>(index);
    if (idx >= vec.size()) {
        vec.resize(idx + 1, -1);
    }
    const int prevVal = vec[idx];
    vec[idx] = id;
    return prevVal != -1 ? std::optional<int>(prevVal) : std::nullopt;
}

} // namespace

VmfWriter::VmfWriter(
    int doubleScale,
    int doubleScaleTextureAxes,
    int doubleScaleTextureScale,
    bool nullOutput,
    const BspDecompileConfig* config)
    : m_doubleScale(doubleScale)
    , m_doubleScaleTextureAxes(doubleScaleTextureAxes)
    , m_doubleScaleTextureScale(doubleScaleTextureScale)
    , m_config(config)
    , m_kvWriter(Core::KeyValues::KeyValuesWriter::Options::vmf(nullOutput))
{
}

VmfWriter::VmfWriter(
    QIODevice* device,
    int doubleScale,
    int doubleScaleTextureAxes,
    int doubleScaleTextureScale,
    bool nullOutput,
    const BspDecompileConfig* config)
    : m_doubleScale(doubleScale)
    , m_doubleScaleTextureAxes(doubleScaleTextureAxes)
    , m_doubleScaleTextureScale(doubleScaleTextureScale)
    , m_config(config)
    , m_kvWriter(device, Core::KeyValues::KeyValuesWriter::Options::vmf(nullOutput))
{
}

void VmfWriter::logWarning(const QString& message) const
{
    if (m_config != nullptr) {
        m_config->log(BspLogLevel::Warning, message);
    }
}

void VmfWriter::warnUnclosedSections() const
{
    const auto& open = m_kvWriter.openSections();
    if (!open.empty()) {
        QString stackState;
        for (std::size_t i = 0; i < open.size(); ++i) {
            if (i > 0) {
                stackState += QStringLiteral(" -> ");
            }
            stackState += open[i];
        }
        logWarning(QStringLiteral("Unclosed VMF chunk: %1").arg(stackState));
    }
}

void VmfWriter::start(QByteArrayView name)
{
    m_kvWriter.startSectionUtf8(name);
}

void VmfWriter::end(QByteArrayView name)
{
    const auto& open = m_kvWriter.openSections();
    if (open.empty()) {
        logWarning(QStringLiteral("VmfWriter::end('%1'): no open sections left")
                       .arg(QString::fromUtf8(name)));
        return;
    }

    if ((name.isEmpty() && !open.back().isEmpty()) || !m_kvWriter.endSectionUtf8(name)) {
        logWarning(QStringLiteral("VMF section end name mismatch. Expected: '%1', got '%2'")
                       .arg(QString::fromUtf8(name), open.back()));
    }
}

void VmfWriter::put(QByteArrayView key, QByteArrayView value)
{
    m_kvWriter.writePropertyUtf8(key, value);
}

void VmfWriter::put(QByteArrayView key, QStringView value)
{
    m_kvWriter.writePropertyUtf8(key, value);
}

void VmfWriter::put(QStringView key, QStringView value)
{
    m_kvWriter.writeProperty(key, value);
}

void VmfWriter::put(QByteArrayView key, int value)
{
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc{}) {
        m_kvWriter.writePropertyUtf8(key, QByteArrayView(buf, static_cast<qsizetype>(ptr - buf)));
    }
}

void VmfWriter::put(QByteArrayView key, int v0, int v1, int v2)
{
    StackUtf8Buffer buf;
    buf.appendInt(v0);
    buf.append(' ');
    buf.appendInt(v1);
    buf.append(' ');
    buf.appendInt(v2);
    m_kvWriter.writePropertyUtf8(key, buf.view());
}

void VmfWriter::put(QByteArrayView key, double value)
{
    StackUtf8Buffer buf;
    buf.appendFloat(value, m_doubleScale);
    m_kvWriter.writePropertyUtf8(key, buf.view());
}

void VmfWriter::putBool(QByteArrayView key, bool value)
{
    m_kvWriter.writePropertyUtf8(key, value ? QByteArrayView("1") : QByteArrayView("0"));
}

void VmfWriter::put(QByteArrayView key, const Vector3f& v, int p)
{
    put(key, v.toDouble(), p);
}

void VmfWriter::put(QByteArrayView key, const Vector3d& v, int p)
{
    StackUtf8Buffer buf;
    appendVector3dFormatted(buf, v, p, m_doubleScale, [this](const Vector3d& bad) {
        logWarning(QStringLiteral("Invalid vector: %1").arg(formatVectorDebug(bad)));
    });
    m_kvWriter.writePropertyUtf8(key, buf.view());
}

void VmfWriter::put(QByteArrayView key, const Vector3d& v1, const Vector3d& v2, const Vector3d& v3)
{
    StackUtf8Buffer buf;
    auto onInvalid = [this](const Vector3d& bad) {
        logWarning(QStringLiteral("Invalid vector: %1").arg(formatVectorDebug(bad)));
    };
    appendVector3dFormatted(buf, v1, 1, m_doubleScale, onInvalid);
    buf.append(' ');
    appendVector3dFormatted(buf, v2, 1, m_doubleScale, onInvalid);
    buf.append(' ');
    appendVector3dFormatted(buf, v3, 1, m_doubleScale, onInvalid);
    m_kvWriter.writePropertyUtf8(key, buf.view());
}

void VmfWriter::put(QByteArrayView key, const TextureAxis& axis)
{
    StackUtf8Buffer buf;
    buf.append('[');

    if (!axis.axis.isValid()) {
        logWarning(QStringLiteral("Invalid vector: %1").arg(formatVectorDebug(axis.axis)));
        buf.append("0 0 0 ");
    } else {
        buf.appendFloat(axis.axis.x, m_doubleScaleTextureAxes);
        buf.append(' ');
        buf.appendFloat(axis.axis.y, m_doubleScaleTextureAxes);
        buf.append(' ');
        buf.appendFloat(axis.axis.z, m_doubleScaleTextureAxes);
        buf.append(' ');
    }

    buf.appendInt(axis.shift);
    buf.append("] ");
    buf.appendFloat(axis.tw, m_doubleScaleTextureScale);
    m_kvWriter.writePropertyUtf8(key, buf.view());
}

void VmfWriter::put(const BspEntity& entity)
{
    for (const EntityKeyValue& kv : entity.keyValues()) {
        put(QStringView(kv.key), QStringView(kv.value));
    }
}

void VmfWriter::put(const Texture& tex)
{
    put("material", QStringView(tex.texture));
    put("uaxis", tex.u);
    put("vaxis", tex.v);
    put("lightmapscale", tex.lmscale);
}

void VmfWriter::put(const EntityKeyValue& kv)
{
    put(QStringView(kv.key), QStringView(kv.value));
}

Core::Result<void> VmfWriter::flush()
{
    warnUnclosedSections();
    return m_kvWriter.flush();
}

QByteArray VmfWriter::finish()
{
    warnUnclosedSections();
    return m_kvWriter.finish();
}

// ============================================================================
// VmfMeta::Visgroup
// ============================================================================

VmfMeta::Visgroup::Visgroup(PassKey, VmfMeta& meta, QString name, Visgroup* parent)
    : m_meta(meta)
    , m_name(std::move(name))
    , m_color(generateNewVisgroupColor())
    , m_parent(parent)
{
    m_id = m_meta.newVisgroupId(*this);
}

VmfMeta::Visgroup* VmfMeta::Visgroup::childVisgroup(const QString& visgroupName)
{
    if (visgroupName.isEmpty()) {
        m_meta.m_config.log(BspLogLevel::Warning, QStringLiteral("Ignoring empty visgroup name"));
        return this;
    }

    auto it = m_children.find(visgroupName);
    if (it != m_children.end()) {
        return &it->second;
    }

    auto [insertedIt, inserted] = m_children.try_emplace(visgroupName, PassKey{}, m_meta, visgroupName, this);
    return &insertedIt->second;
}

VmfMeta::Visgroup* VmfMeta::Visgroup::setColor(const QColor& color)
{
    if (color.isValid()) {
        m_color = color;
    }
    return this;
}

VmfMeta::Visgroup* VmfMeta::Visgroup::rootVisgroup() noexcept
{
    Visgroup* root = this;
    while (root->m_parent != nullptr) {
        root = root->m_parent;
    }
    return root;
}

std::vector<QString> VmfMeta::Visgroup::visgroupPath() const
{
    std::vector<QString> path;
    for (const Visgroup* vg = this; vg != nullptr; vg = vg->m_parent) {
        path.push_back(vg->m_name);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool VmfMeta::Visgroup::isTreeUsed() const noexcept
{
    if (m_used) {
        return true;
    }
    for (const auto& [childName, child] : m_children) {
        if (child.isTreeUsed()) {
            return true;
        }
    }
    return false;
}

bool VmfMeta::Visgroup::containsVisgroupId(int id) const noexcept
{
    if (m_id == id) {
        return true;
    }
    for (const auto& [childName, child] : m_children) {
        if (child.containsVisgroupId(id)) {
            return true;
        }
    }
    return false;
}

QColor VmfMeta::Visgroup::generateNewVisgroupColor()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float hue = dist(m_meta.m_rng);
    const float saturation = dist(m_meta.m_rng) * 0.8f + 0.1f;
    const float luminance = dist(m_meta.m_rng) * 0.4f + 0.5f;
    return QColor::fromHsvF(hue, saturation, luminance);
}

// ============================================================================
// VmfMeta
// ============================================================================

VmfMeta::VmfMeta(BspFileReader& reader, VmfWriter& writer, const BspDecompileConfig& config)
    : m_reader(reader)
    , m_writer(writer)
    , m_config(config)
    , m_rng(std::random_device{}())
{
    m_rootVisgroup.emplace(Visgroup::PassKey{}, *this, QStringLiteral("root"), nullptr);

    auto& entities = m_reader.data().entities;
    for (BspEntity& ent : entities) {
        if (ent.className().compare(QStringLiteral("worldspawn"), Qt::CaseInsensitive) == 0) {
            m_worldspawn = &ent;
            break;
        }
    }

    if (m_worldspawn == nullptr) {
        m_config.log(
            BspLogLevel::Warning,
            QStringLiteral("Couldn't find worldspawn entity. This may be due to the map having stripped "
                           "entities. Some information like skybox and detail sprites will be missing, "
                           "because these are saved in the worldspawn entity"));
        m_fallbackWorldspawn.emplace();
        m_fallbackWorldspawn->setClassName(QStringLiteral("worldspawn"));
    }

    if (const auto commentOpt = worldspawnEntity().value(u"comment"); commentOpt.has_value()) {
        m_config.log(BspLogLevel::Info, QStringLiteral("Map comment: %1").arg(*commentOpt));
    }

    if (!worldspawnEntity().hasKey(u"mapversion")) {
        worldspawnEntity().setValue(QStringLiteral("mapversion"), m_reader.bspFile().revision());
    }
}

BspEntity& VmfMeta::worldspawnEntity() noexcept
{
    return m_worldspawn != nullptr ? *m_worldspawn : *m_fallbackWorldspawn;
}

const BspEntity& VmfMeta::worldspawnEntity() const noexcept
{
    return m_worldspawn != nullptr ? *m_worldspawn : *m_fallbackWorldspawn;
}

int VmfMeta::allocateUid()
{
    if (m_uidBlacklist.isEmpty()) {
        return m_uid++;
    }

    do {
        ++m_uid;
    } while (m_uidBlacklist.contains(m_uid));

    return m_uid;
}

int VmfMeta::faceUid(int iface) const
{
    if (iface >= 0 && static_cast<std::size_t>(iface) < m_faceUids.size()) {
        const int uid = m_faceUids[static_cast<std::size_t>(iface)];
        if (uid != -1) {
            return uid;
        }
    }

    const auto& faces = m_reader.data().faces;
    if (iface >= 0 && iface < static_cast<int>(faces.size())) {
        const int ioface = faces[static_cast<std::size_t>(iface)].origFace;
        if (ioface >= 0 && static_cast<std::size_t>(ioface) < m_origFaceUids.size()) {
            return m_origFaceUids[static_cast<std::size_t>(ioface)];
        }
    }

    return -1;
}

std::optional<int> VmfMeta::setFaceUid(int iface, int id)
{
    return setIndexedUid(m_faceUids, iface, id);
}

std::optional<int> VmfMeta::setOrigFaceUid(int iface, int id)
{
    return setIndexedUid(m_origFaceUids, iface, id);
}

int VmfMeta::dispInfoUid(int idispinfo) const
{
    if (idispinfo >= 0 && static_cast<std::size_t>(idispinfo) < m_dispinfoUids.size()) {
        return m_dispinfoUids[static_cast<std::size_t>(idispinfo)];
    }
    return -1;
}

std::optional<int> VmfMeta::setDispInfoUid(int idispinfo, int id)
{
    return setIndexedUid(m_dispinfoUids, idispinfo, id);
}

void VmfMeta::appendComment(const QString& commentText)
{
    const auto prev = comment();
    const QString combined = prev.has_value()
        ? (*prev + QStringLiteral(" | ") + commentText)
        : commentText;
    worldspawnEntity().setValue(QStringLiteral("comment"), combined);
}

std::optional<QString> VmfMeta::comment() const
{
    return worldspawnEntity().value(u"comment");
}

void VmfMeta::writeWorldHeader()
{
    m_writer.start("world");
    m_writer.put("id", allocateUid());
    m_writer.put(worldspawnEntity());
    m_writer.put("classname", "worldspawn");
}

void VmfMeta::writeWorldFooter()
{
    m_writer.end("world");
}

QString VmfMeta::mapVersion() const
{
    const auto version = worldspawnEntity().value(u"mapversion");
    if (!version.has_value() || version->isEmpty()) {
        return QStringLiteral("2");
    }
    return *version;
}

void VmfMeta::writeVersionInfo(VmfWriter& targetWriter) const
{
    targetWriter.start("versioninfo");
    targetWriter.put("editorversion", 400);
    targetWriter.put("editorbuild", 9999);
    targetWriter.put("mapversion", QStringView(mapVersion()));
    targetWriter.put("formatversion", 100);
    targetWriter.put("prefab", 0);
    targetWriter.end("versioninfo");
}

void VmfMeta::writeVisgroups()
{
    writeVisgroups(m_writer);
}

void VmfMeta::writeVisgroups(VmfWriter& targetWriter) const
{
    targetWriter.start("visgroups");
    if (m_rootVisgroup.has_value()) {
        for (const auto& [childName, child] : m_rootVisgroup->m_children) {
            writeVisgroup(targetWriter, child);
        }
    }
    targetWriter.end("visgroups");
}

void VmfMeta::writeVisgroup(VmfWriter& targetWriter, const Visgroup& visgroup) const
{
    if (!visgroup.isTreeUsed()) {
        return;
    }

    targetWriter.start("visgroup");
    targetWriter.put("name", QStringView(visgroup.name()));
    targetWriter.put("visgroupid", visgroup.id());
    targetWriter.put("color", visgroup.color().red(), visgroup.color().green(), visgroup.color().blue());

    for (const auto& [childName, child] : visgroup.m_children) {
        writeVisgroup(targetWriter, child);
    }

    targetWriter.end("visgroup");
}

void VmfMeta::writeViewSettings(VmfWriter& targetWriter) const
{
    targetWriter.start("viewsettings");
    targetWriter.put("bSnapToGrid", 1);
    targetWriter.put("bShowGrid", 1);
    targetWriter.put("bShowLogicalGrid", 0);
    targetWriter.put("nGridSpacing", 64);
    targetWriter.put("bShow3DGrid", 0);
    targetWriter.end("viewsettings");
}

void VmfMeta::writeMetaVisgroup(const QString& visgroupName)
{
    Visgroup* root = visgroups();
    if (root == nullptr) {
        return;
    }
    Visgroup* vg = root->childVisgroup(visgroupName);
    if (vg != nullptr && vg != root) {
        writeMetaVisgroups({vg});
    }
}

void VmfMeta::writeMetaVisgroups(const std::vector<Visgroup*>& visgroupsList)
{
    Visgroup* root = visgroups();
    m_writer.start("editor");
    for (Visgroup* vg : visgroupsList) {
        if (vg == nullptr) {
            continue;
        }
        if (vg == root) {
            m_config.log(BspLogLevel::Warning, QStringLiteral("Root visgroup cannot be written"));
            continue;
        }
        if (vg->rootVisgroup() != root) {
            m_config.log(
                BspLogLevel::Warning,
                QStringLiteral("Visgroup '%1' is not part of this VmfMeta's root visgroup")
                    .arg(joinVisgroupPath(vg->visgroupPath())));
            continue;
        }

        m_writer.put("visgroupid", vg->id());
        vg->setUsed(true);
    }
    m_writer.end("editor");
}

int VmfMeta::newVisgroupId(const Visgroup& vg)
{
    const auto path = vg.visgroupPath();
    if (const auto it = m_reservedVisgroups.find(path); it != m_reservedVisgroups.end()) {
        return it->second;
    }

    while (true) {
        const int candidate = m_visgroupIndex++;
        if (!m_reservedVisgroupIds.contains(candidate)) {
            return candidate;
        }
    }
}

bool VmfMeta::reserveVisgroupId(int visgroupId, const std::vector<QString>& visgroupPath)
{
    if (visgroupPath.empty() || std::any_of(visgroupPath.begin(), visgroupPath.end(), [](const QString& s) {
            return s.isEmpty();
        })) {
        m_config.log(BspLogLevel::Warning, QStringLiteral("Invalid visgroup path for reservation"));
        return false;
    }

    std::vector<QString> visgroupPathFixed;
    visgroupPathFixed.reserve(visgroupPath.size() + 1);
    visgroupPathFixed.push_back(m_rootVisgroup->name());
    visgroupPathFixed.insert(visgroupPathFixed.end(), visgroupPath.begin(), visgroupPath.end());

    if (m_rootVisgroup->containsVisgroupId(visgroupId)) {
        m_config.log(
            BspLogLevel::Error,
            QStringLiteral("Tried to reserve already taken visgroup id %1 to '%2'")
                .arg(visgroupId)
                .arg(joinVisgroupPath(visgroupPathFixed)));
        return false;
    }

    for (const auto& [existingPath, existingId] : m_reservedVisgroups) {
        if (existingId == visgroupId && existingPath != visgroupPathFixed) {
            m_config.log(
                BspLogLevel::Error,
                QStringLiteral("Tried to reserve visgroup %1 with id %2, which is already reserved by %3")
                    .arg(joinVisgroupPath(visgroupPathFixed))
                    .arg(visgroupId)
                    .arg(joinVisgroupPath(existingPath)));
            return false;
        }
    }

    if (const auto it = m_reservedVisgroups.find(visgroupPathFixed); it != m_reservedVisgroups.end()) {
        m_config.log(
            BspLogLevel::Warning,
            QStringLiteral("Visgroup '%1' is already reserved with id %2, overwriting with %3")
                .arg(joinVisgroupPath(visgroupPathFixed))
                .arg(it->second)
                .arg(visgroupId));
        m_reservedVisgroupIds.remove(it->second);
    }

    m_reservedVisgroups[visgroupPathFixed] = visgroupId;
    m_reservedVisgroupIds.insert(visgroupId);
    return true;
}

void VmfMeta::writeCameras()
{
    m_writer.start("cameras");

    if (m_cameras.empty()) {
        m_writer.put("activecamera", -1);
    } else {
        m_writer.put("activecamera", 0);

        for (const Camera& camera : m_cameras) {
            m_writer.start("camera");
            m_writer.put("position", camera.pos, 2);
            m_writer.put("look", camera.look, 2);
            m_writer.end("camera");
        }
    }

    m_writer.end("cameras");
}

void VmfMeta::writeCordon()
{
    m_writer.start("cordon");
    m_writer.put("mins", "(-1024 -1024 -1024)");
    m_writer.put("maxs", "(1024 1024 1024)");
    m_writer.put("active", 0);
    m_writer.end("cordon");
}

} // namespace Domain::Bsp::Internal
