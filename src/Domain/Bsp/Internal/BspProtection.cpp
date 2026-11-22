#include "Domain/Bsp/Internal/BspProtection.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>

#include "Domain/Package/Internal/BspPakZipReader.h"

namespace Domain::Bsp::Internal {

namespace {

constexpr float kEpsSize = 0.01f;
constexpr float kAlignedAlpha = 0.99f;
constexpr float kNodrawRatioLimit = 0.9f;

const Vector3d kPb1{1.0, 4.0, 9.0};
const Vector3d kPb2{4.0, 9.0, 1.0};
const Vector3d kPb3{9.0, 1.0, 4.0};

} // namespace

BspProtection::BspProtection(
    BspFileReader& reader,
    BrushBounds& brushBounds,
    TextureSource& texsrc,
    bool isFixTextureNames,
    const BspDecompileConfig& config)
    : m_reader(reader)
    , m_brushBounds(brushBounds)
    , m_texsrc(texsrc)
    , m_isFixTextureNames(isFixTextureNames)
    , m_config(config) {
    m_reader.loadEntities();
    m_reader.loadPlanes();
    m_reader.loadBrushes();
    m_reader.loadBrushSides();
}

bool BspProtection::check() {
    m_flaggedEnt = false;
    m_flaggedTex = false;
    m_flaggedBrush = false;
    m_encryptedEnt = false;
    m_obfuscatedEnt = false;
    m_modifiedTexinfo = false;
    m_protBrushIndices.clear();
    m_protEntities.clear();

    checkBrushes();
    checkBrushSides();
    checkEntities();
    checkTextures();
    checkPakfile();

    const bool prot = isProtected();
    if (!prot) {
        m_config.log(BspLogLevel::Debug, QStringLiteral("Nothing found"));
    }
    return prot;
}

bool BspProtection::isProtected() const noexcept {
    return m_flaggedEnt
        || m_flaggedTex
        || m_flaggedBrush
        || m_encryptedEnt
        || m_obfuscatedEnt
        || m_modifiedTexinfo;
}

QStringList BspProtection::getProtectionMethods() const {
    QStringList methods;

    if (hasEntityFlag()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "VMEX entity flag (no_decomp)"));
    }
    if (hasTextureFlag()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "VMEX texture flag (tools/locked)"));
    }
    if (hasBrushFlag()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "VMEX protector brush flag"));
    }
    if (hasEncryptedEntities()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "BSPProtect entity encryption"));
    }
    if (hasObfuscatedEntities()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "IID entity obfuscation"));
    }
    if (hasModifiedTexinfo()) {
        methods.append(QCoreApplication::translate(
            "Domain::Bsp::Internal::BspProtection",
            "IID nodraw texture hack"));
    }

    return methods;
}

bool BspProtection::isProtectedBrushIndex(int brushIndex) const noexcept {
    return std::find(m_protBrushIndices.begin(), m_protBrushIndices.end(), brushIndex)
        != m_protBrushIndices.end();
}

bool BspProtection::isProtectedEntity(const BspEntity& entity) const noexcept {
    return std::find(m_protEntities.begin(), m_protEntities.end(), &entity)
        != m_protEntities.end();
}

void BspProtection::checkBrushes() {
    m_config.log(BspLogLevel::Debug, QStringLiteral("Checking for protector prefab"));

    const BspData& bsp = m_reader.getData();
    int b1 = -1;
    int b2 = -1;
    int b3 = -1;

    for (std::size_t i = 0; i < bsp.brushes.size(); ++i) {
        const DBrush& b = bsp.brushes[i];
        if (!isAlignedBrush(b) || !isSameTexBrush(b)) {
            continue;
        }

        const Vector3d bsize = m_brushBounds.getBounds(bsp, b).getSize();

        if (kPb1.sub(bsize).length() < kEpsSize) {
            b1 = static_cast<int>(i);
        }
        if (kPb2.sub(bsize).length() < kEpsSize) {
            b2 = static_cast<int>(i);
        }
        if (kPb3.sub(bsize).length() < kEpsSize) {
            b3 = static_cast<int>(i);
        }

        if (b1 >= 0 && b2 >= 0 && b3 >= 0) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Found protector prefab!"));
            m_flaggedBrush = true;

            m_protBrushIndices.push_back(b1);
            m_protBrushIndices.push_back(b2);
            m_protBrushIndices.push_back(b3);

            b1 = -1;
            b2 = -1;
            b3 = -1;
        }
    }
}

void BspProtection::checkBrushSides() {
    m_config.log(
        BspLogLevel::Debug,
        QStringLiteral("Checking for nodraw brush sides (ratio limit: %1)").arg(kNodrawRatioLimit));

    const BspData& bsp = m_reader.getData();
    if (bsp.brushSides.empty()) {
        m_modifiedTexinfo = false;
        return;
    }

    double nodrawSides = 0.0;
    for (const DBrushSide& bs : bsp.brushSides) {
        if (bs.texinfo == 0) {
            nodrawSides += 1.0;
        }
    }

    const double nodrawRatio = nodrawSides / static_cast<double>(bsp.brushSides.size());
    m_modifiedTexinfo = nodrawRatio > kNodrawRatioLimit;
    if (m_modifiedTexinfo) {
        m_config.log(BspLogLevel::Debug, QStringLiteral("Found nodraw hack!"));
    }
}

void BspProtection::checkEntities() {
    m_config.log(
        BspLogLevel::Debug,
        QStringLiteral("Checking for entity lock key \"%1\" and obfuscated targetnames").arg(kVmexLockedEnt));

    const BspData& bsp = m_reader.getData();
    int targetnames = 0;
    int targetnamesObfs = 0;

    for (const BspEntity& ent : bsp.entities) {
        if (const std::optional<QString> targetName = ent.targetName(); targetName.has_value()) {
            ++targetnames;
            if (!targetName->isEmpty()
                && std::all_of(targetName->cbegin(), targetName->cend(), [](QChar c) {
                       return c >= QLatin1Char('0') && c <= QLatin1Char('9');
                   })) {
                ++targetnamesObfs;
            }
        }

        for (const EntityKeyValue& kv : ent.keyValues()) {
            if (kv.key == kVmexLockedEnt) {
                m_config.log(BspLogLevel::Debug, QStringLiteral("Found lock key!"));
                m_protEntities.push_back(&ent);
                m_flaggedEnt = true;
            }
        }
    }

    m_obfuscatedEnt = targetnames > 0 && targetnames == targetnamesObfs;
    if (m_obfuscatedEnt) {
        m_config.log(BspLogLevel::Debug, QStringLiteral("Found obfuscation!"));
    }
}

void BspProtection::checkTextures() {
    m_config.log(
        BspLogLevel::Debug,
        QStringLiteral("Checking for lock texture \"%1\"").arg(kVmexLockedTex));

    const BspData& bsp = m_reader.getData();
    for (const QString& texname : bsp.texnames) {
        if (texname.compare(kVmexLockedTex, Qt::CaseInsensitive) == 0) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Found lock texture!"));
            m_flaggedTex = true;
            return;
        }
    }
}

void BspProtection::checkPakfile() {
    m_config.log(
        BspLogLevel::Debug,
        QStringLiteral("Checking for encrypted entities inside pakfile (file: \"%1\")").arg(kBspProtectFile));

    // BSPProtect currently works with Source 2007/2009 aka Orange Box only
    if (m_reader.getBspFile().version() != 20) {
        m_encryptedEnt = false;
        return;
    }

    auto zipResult = Domain::Package::Internal::BspPakZipReader::open(
        m_reader.getBspFile().file(),
        m_config.token);
    if (zipResult.isSuccess()) {
        if (zipResult.value().hasEntry(QStringLiteral("entities.dat"))) {
            m_config.log(BspLogLevel::Debug, QStringLiteral("Found encrypted entities!"));
            m_encryptedEnt = true;
        } else {
            m_encryptedEnt = false;
        }
    } else {
        if (zipResult.isFailure()) {
            m_config.log(
                BspLogLevel::Warning,
                QCoreApplication::translate(
                    "Domain::Bsp::Internal::BspProtection",
                    "Couldn't read pakfile: %1")
                    .arg(zipResult.error().message()));
        }
        m_encryptedEnt = false;
    }
}

bool BspProtection::isAlignedBrush(const DBrush& brush) const {
    if (brush.numside != 6 || brush.fstside < 0) {
        return false;
    }

    const BspData& bsp = m_reader.getData();
    if (static_cast<std::size_t>(brush.fstside) + 6 > bsp.brushSides.size()) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        const DBrushSide& bs = bsp.brushSides[static_cast<std::size_t>(brush.fstside + i)];
        if (bs.pnum < 0 || static_cast<std::size_t>(bs.pnum) >= bsp.planes.size()) {
            continue;
        }
        const DPlane& bpl = bsp.planes[static_cast<std::size_t>(bs.pnum)];
        if (std::abs(bpl.normal.x) > kAlignedAlpha
            || std::abs(bpl.normal.y) > kAlignedAlpha
            || std::abs(bpl.normal.z) > kAlignedAlpha) {
            return true;
        }
    }

    return false;
}

bool BspProtection::isSameTexBrush(const DBrush& brush) const {
    if (brush.numside <= 0 || brush.fstside < 0) {
        return false;
    }

    const BspData& bsp = m_reader.getData();
    if (static_cast<std::size_t>(brush.fstside) + static_cast<std::size_t>(brush.numside) > bsp.brushSides.size()) {
        return false;
    }

    const std::vector<QString>& texnames =
        m_isFixTextureNames ? m_texsrc.getFixedTextureNames() : bsp.texnames;

    const DBrushSide& firstSide = bsp.brushSides[static_cast<std::size_t>(brush.fstside)];
    const std::optional<QString> texname =
        TextureSource::getTextureName(firstSide.texinfo, bsp.texinfos, bsp.texdatas, texnames);

    if (!texname.has_value()) {
        return false;
    }

    for (int i = 1; i < brush.numside; ++i) {
        const DBrushSide& bs = bsp.brushSides[static_cast<std::size_t>(brush.fstside + i)];
        const std::optional<QString> nextTexname =
            TextureSource::getTextureName(bs.texinfo, bsp.texinfos, bsp.texdatas, texnames);

        if (!nextTexname.has_value() || texname->compare(*nextTexname, Qt::CaseInsensitive) != 0) {
            return false;
        }
    }

    return true;
}

} // namespace Domain::Bsp::Internal
