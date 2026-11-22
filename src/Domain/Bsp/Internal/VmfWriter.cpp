#include "Domain/Bsp/Internal/VmfWriter.h"

#include <QByteArrayView>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>

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
    , m_nullOutput(nullOutput)
    , m_config(config)
{
}

void VmfWriter::appendIndent()
{
    if (m_nullOutput) {
        return;
    }
    for (std::size_t i = 0; i < m_section.size(); ++i) {
        m_buffer.append('\t');
    }
}

void VmfWriter::appendUtf8(QStringView text)
{
    if (m_nullOutput || text.isEmpty()) {
        return;
    }
    m_buffer.append(text.toUtf8());
}

void VmfWriter::logWarning(const QString& message) const
{
    if (m_config != nullptr) {
        m_config->log(BspLogLevel::Warning, message);
    }
}

void VmfWriter::start(QStringView name)
{
    if (!m_nullOutput) {
        appendIndent();
        appendUtf8(name);
        m_buffer.append("\r\n");
        appendIndent();
        m_buffer.append("{\r\n");
    }
    m_section.push_back(name.toString());
}

void VmfWriter::end(QStringView name)
{
    if (m_section.empty()) {
        logWarning(QStringLiteral("VmfWriter::end('%1'): no open sections left").arg(name.toString()));
        return;
    }

    const QString& top = m_section.back();
    if (top != name) {
        logWarning(QStringLiteral("VMF section end name mismatch. Expected: '%1', got '%2'")
                       .arg(name.toString(), top));
        return;
    }

    m_section.pop_back();
    if (!m_nullOutput) {
        appendIndent();
        m_buffer.append("}\r\n");
    }
}

void VmfWriter::put(QStringView key, QStringView value)
{
    if (m_nullOutput) {
        return;
    }
    appendIndent();
    m_buffer.append('"');
    appendUtf8(key);
    m_buffer.append("\" \"");
    appendUtf8(value);
    m_buffer.append("\"\r\n");
}

void VmfWriter::put(QStringView key, QLatin1StringView value)
{
    put(key, QString(value));
}

void VmfWriter::put(QStringView key, const char16_t* value)
{
    put(key, value != nullptr ? QStringView(value) : QStringView{});
}

void VmfWriter::put(QStringView key, const char* value)
{
    put(key, value != nullptr ? QString::fromUtf8(value) : QString{});
}

void VmfWriter::put(QStringView key, int value)
{
    put(key, QString::number(value));
}

void VmfWriter::put(QStringView key, int v0, int v1, int v2)
{
    put(key, QStringLiteral("%1 %2 %3").arg(v0).arg(v1).arg(v2));
}

void VmfWriter::put(QStringView key, double value)
{
    put(key, formatFloat(value, m_doubleScale));
}

void VmfWriter::putBool(QStringView key, bool value)
{
    put(key, value ? QStringView{u"1"} : QStringView{u"0"});
}

void VmfWriter::put(QStringView key, const Vector3f& v, int p)
{
    put(key, formatVector3d(v.toDouble(), p));
}

void VmfWriter::put(QStringView key, const Vector3d& v, int p)
{
    put(key, formatVector3d(v, p));
}

void VmfWriter::put(QStringView key, const Vector3d& v1, const Vector3d& v2, const Vector3d& v3)
{
    put(key,
        formatVector3d(v1, 1) + QLatin1Char(' ')
            + formatVector3d(v2, 1) + QLatin1Char(' ')
            + formatVector3d(v3, 1));
}

void VmfWriter::put(QStringView key, const TextureAxis& axis)
{
    put(key, formatTextureAxis(axis));
}

void VmfWriter::put(const std::vector<std::pair<QString, QString>>& orderedMap)
{
    for (const auto& [k, v] : orderedMap) {
        put(k, v);
    }
}

void VmfWriter::put(const BspEntity& entity)
{
    for (const EntityKeyValue& kv : entity.keyValues()) {
        put(kv.key, kv.value);
    }
}

void VmfWriter::put(const Texture& tex)
{
    put(u"material", tex.texture);
    put(u"uaxis", tex.u);
    put(u"vaxis", tex.v);
    put(u"lightmapscale", tex.lmscale);
}

void VmfWriter::put(const EntityKeyValue& kv)
{
    put(kv.key, kv.value);
}

QByteArray VmfWriter::finish()
{
    if (!m_section.empty()) {
        QString stackState;
        for (std::size_t i = 0; i < m_section.size(); ++i) {
            if (i > 0) {
                stackState += QStringLiteral(" -> ");
            }
            stackState += m_section[i];
        }
        logWarning(QStringLiteral("Unclosed VMF chunk: %1").arg(stackState));
    }

    QByteArray out = std::move(m_buffer);
    m_buffer.clear();
    return out;
}

QString VmfWriter::formatVector3d(const Vector3d& v, int p) const
{
    QString out;
    if (p == 1) {
        out += QLatin1Char('(');
    } else if (p == 2) {
        out += QLatin1Char('[');
    }

    if (!v.isValid()) {
        logWarning(QStringLiteral("Invalid vector: %1").arg(formatVectorDebug(v)));
        out += QStringLiteral("0 0 0");
    } else {
        out += formatFloat(v.x, m_doubleScale);
        out += QLatin1Char(' ');
        out += formatFloat(v.y, m_doubleScale);
        out += QLatin1Char(' ');
        out += formatFloat(v.z, m_doubleScale);
    }

    if (p == 1) {
        out += QLatin1Char(')');
    } else if (p == 2) {
        out += QLatin1Char(']');
    }

    return out;
}

QString VmfWriter::formatTextureAxis(const TextureAxis& tx) const
{
    QString out;
    out += QLatin1Char('[');

    if (!tx.axis.isValid()) {
        logWarning(QStringLiteral("Invalid vector: %1").arg(formatVectorDebug(tx.axis)));
        out += QStringLiteral("0 0 0 ");
    } else {
        out += formatFloat(tx.axis.x, m_doubleScaleTextureAxes);
        out += QLatin1Char(' ');
        out += formatFloat(tx.axis.y, m_doubleScaleTextureAxes);
        out += QLatin1Char(' ');
        out += formatFloat(tx.axis.z, m_doubleScaleTextureAxes);
        out += QLatin1Char(' ');
    }

    out += QString::number(tx.shift);
    out += QStringLiteral("] ");
    out += formatFloat(tx.tw, m_doubleScaleTextureScale);
    return out;
}

QString VmfWriter::formatFloat(double f, int decimalPlaces) const
{
    if (std::isnan(f)) {
        return QStringLiteral("NaN");
    }
    if (std::isinf(f)) {
        return f < 0.0 ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
    }

    char buf[128];

    if (decimalPlaces <= 0) {
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf) - 3, f);
        if (ec == std::errc{}) {
            std::string_view sv(buf, static_cast<std::size_t>(ptr - buf));
            if (sv == "-0") {
                return QStringLiteral("0.0");
            }
            if (sv.find('.') == std::string_view::npos
                && sv.find('e') == std::string_view::npos
                && sv.find('E') == std::string_view::npos) {
                return QString::fromLatin1(sv.data(), static_cast<qsizetype>(sv.size()))
                    + QStringLiteral(".0");
            }
            return QString::fromLatin1(sv.data(), static_cast<qsizetype>(sv.size()));
        }
        return QString::number(f, 'g', 15);
    }

    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), f, std::chars_format::fixed, decimalPlaces);
    std::string_view sv;
    QByteArray fallbackStorage;
    if (ec == std::errc{}) {
        sv = std::string_view(buf, static_cast<std::size_t>(ptr - buf));
    } else {
        fallbackStorage = QString::number(f, 'f', decimalPlaces).toLatin1();
        sv = std::string_view(fallbackStorage.constData(), static_cast<std::size_t>(fallbackStorage.size()));
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

    return QString::fromLatin1(sv.data(), static_cast<qsizetype>(sv.size()));
}

// ============================================================================
// VmfMeta::Visgroup
// ============================================================================

VmfMeta::Visgroup::Visgroup(VmfMeta& meta, QString name, Visgroup* parent)
    : m_meta(meta)
    , m_name(std::move(name))
    , m_color(generateNewVisgroupColor())
    , m_parent(parent)
{
    m_id = m_meta.getNewVisgroupId(*this);
}

VmfMeta::Visgroup* VmfMeta::Visgroup::getVisgroup(const QString& visgroupName)
{
    if (visgroupName.isEmpty()) {
        m_meta.m_config.log(BspLogLevel::Warning, QStringLiteral("Ignoring empty visgroup name"));
        return this;
    }

    auto it = m_children.find(visgroupName);
    if (it != m_children.end()) {
        return it->second.get();
    }

    auto child = std::unique_ptr<Visgroup>(new Visgroup(m_meta, visgroupName, this));
    Visgroup* raw = child.get();
    m_children.emplace(visgroupName, std::move(child));
    return raw;
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
    for (const auto& [childName, childPtr] : m_children) {
        if (childPtr->isTreeUsed()) {
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
    for (const auto& [childName, childPtr] : m_children) {
        if (childPtr->containsVisgroupId(id)) {
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
    m_rootVisgroup = std::unique_ptr<Visgroup>(new Visgroup(*this, QStringLiteral("root"), nullptr));

    auto& entities = m_reader.getData().entities;
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

    if (const auto comment = worldspawnEntity().value(u"comment"); comment.has_value()) {
        m_config.log(BspLogLevel::Info, QStringLiteral("Map comment: %1").arg(*comment));
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

int VmfMeta::getUID()
{
    if (m_uidBlacklist.isEmpty()) {
        return m_uid++;
    }

    do {
        ++m_uid;
    } while (m_uidBlacklist.contains(m_uid));

    return m_uid;
}

int VmfMeta::getFaceUID(int iface) const
{
    if (const auto it = m_faceUIDs.constFind(iface); it != m_faceUIDs.constEnd()) {
        return it.value();
    }

    const auto& faces = m_reader.getData().faces;
    if (iface >= 0 && iface < static_cast<int>(faces.size())) {
        const int ioface = faces[static_cast<std::size_t>(iface)].origFace;
        if (const auto itOrig = m_origFaceUIDs.constFind(ioface); itOrig != m_origFaceUIDs.constEnd()) {
            return itOrig.value();
        }
    }

    return -1;
}

std::optional<int> VmfMeta::setFaceUID(int iface, int id)
{
    std::optional<int> prev;
    if (const auto it = m_faceUIDs.constFind(iface); it != m_faceUIDs.constEnd()) {
        prev = it.value();
    }
    m_faceUIDs.insert(iface, id);
    return prev;
}

std::optional<int> VmfMeta::setOrigFaceUID(int iface, int id)
{
    std::optional<int> prev;
    if (const auto it = m_origFaceUIDs.constFind(iface); it != m_origFaceUIDs.constEnd()) {
        prev = it.value();
    }
    m_origFaceUIDs.insert(iface, id);
    return prev;
}

int VmfMeta::getDispInfoUID(int idispinfo) const
{
    if (const auto it = m_dispinfoUIDs.constFind(idispinfo); it != m_dispinfoUIDs.constEnd()) {
        return it.value();
    }
    return -1;
}

std::optional<int> VmfMeta::setDispInfoUID(int idispinfo, int id)
{
    std::optional<int> prev;
    if (const auto it = m_dispinfoUIDs.constFind(idispinfo); it != m_dispinfoUIDs.constEnd()) {
        prev = it.value();
    }
    m_dispinfoUIDs.insert(idispinfo, id);
    return prev;
}

void VmfMeta::appendComment(const QString& comment)
{
    const auto prev = getComment();
    const QString combined = prev.has_value()
        ? (*prev + QStringLiteral(" | ") + comment)
        : comment;
    worldspawnEntity().setValue(QStringLiteral("comment"), combined);
}

std::optional<QString> VmfMeta::getComment() const
{
    return worldspawnEntity().value(u"comment");
}

void VmfMeta::writeWorldHeader()
{
    m_writer.start(u"world");
    m_writer.put(u"id", getUID());
    m_writer.put(worldspawnEntity());
    m_writer.put(u"classname", u"worldspawn");
}

void VmfMeta::writeWorldFooter()
{
    m_writer.end(u"world");
}

void VmfMeta::writeVisgroups()
{
    if (m_rootVisgroup->m_children.empty()) {
        return;
    }

    m_writer.start(u"visgroups");
    for (const auto& [childName, childPtr] : m_rootVisgroup->m_children) {
        writeVisgroup(*childPtr);
    }
    m_writer.end(u"visgroups");
}

void VmfMeta::writeVisgroup(const Visgroup& visgroup)
{
    if (!visgroup.isTreeUsed()) {
        return;
    }

    m_writer.start(u"visgroup");
    m_writer.put(u"name", visgroup.name());
    m_writer.put(u"visgroupid", visgroup.id());
    m_writer.put(
        u"color",
        QStringLiteral("%1 %2 %3")
            .arg(visgroup.color().red())
            .arg(visgroup.color().blue())
            .arg(visgroup.color().green()));

    for (const auto& [childName, childPtr] : visgroup.m_children) {
        writeVisgroup(*childPtr);
    }

    m_writer.end(u"visgroup");
}

void VmfMeta::writeMetaVisgroup(const QString& visgroupName)
{
    Visgroup* vg = m_rootVisgroup->getVisgroup(visgroupName);
    if (vg != nullptr && vg != m_rootVisgroup.get()) {
        writeMetaVisgroups({vg});
    }
}

void VmfMeta::writeMetaVisgroups(const std::vector<Visgroup*>& visgroups)
{
    m_writer.start(u"editor");
    for (Visgroup* vg : visgroups) {
        if (vg == nullptr) {
            continue;
        }
        if (vg == m_rootVisgroup.get()) {
            m_config.log(BspLogLevel::Warning, QStringLiteral("Root visgroup cannot be written"));
            continue;
        }
        if (vg->rootVisgroup() != m_rootVisgroup.get()) {
            m_config.log(
                BspLogLevel::Warning,
                QStringLiteral("Visgroup '%1' is not part of this VmfMeta's root visgroup")
                    .arg(joinVisgroupPath(vg->visgroupPath())));
            continue;
        }

        m_writer.put(u"visgroupid", vg->id());
        vg->setUsed(true);
    }
    m_writer.end(u"editor");
}

int VmfMeta::getNewVisgroupId(const Visgroup& vg)
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
    m_writer.start(u"cameras");

    if (m_cameras.empty()) {
        m_writer.put(u"activecamera", -1);
    } else {
        m_writer.put(u"activecamera", 0);

        for (const Camera& camera : m_cameras) {
            m_writer.start(u"camera");
            m_writer.put(u"position", camera.pos, 2);
            m_writer.put(u"look", camera.look, 2);
            m_writer.end(u"camera");
        }
    }

    m_writer.end(u"cameras");
}

} // namespace Domain::Bsp::Internal
