#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QColor>
#include <QIODevice>
#include <QSet>
#include <QString>
#include <QStringView>

#include <concepts>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "Core/KeyValues/KeyValuesWriter.h"
#include "Core/Result/Result.h"
#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspMath.h"
#include "Domain/Bsp/Internal/BspStructs.h"
#include "Domain/Bsp/Internal/TextureSource.h"

namespace Domain::Bsp::Internal {

class BspFileReader;

struct Camera {
    Vector3d pos;
    Vector3d look;
};

/**
 * @brief Formatted VMF text writer matching Java BSPSource's VmfWriter output.
 *
 * Supports both in-memory buffering and streaming writes to a QIODevice.
 */
class VmfWriter {
public:
    VmfWriter(
        int doubleScale,
        int doubleScaleTextureAxes,
        int doubleScaleTextureScale,
        bool nullOutput = false,
        const BspDecompileConfig* config = nullptr);

    VmfWriter(
        QIODevice* device,
        int doubleScale,
        int doubleScaleTextureAxes,
        int doubleScaleTextureScale,
        bool nullOutput = false,
        const BspDecompileConfig* config = nullptr);

    void start(QByteArrayView name);
    void end(QByteArrayView name);

    void put(QByteArrayView key, QByteArrayView value);
    void put(QByteArrayView key, QStringView value);
    void put(QStringView key, QStringView value);
    void put(QByteArrayView key, int value);
    void put(QByteArrayView key, int v0, int v1, int v2);
    void put(QByteArrayView key, double value);
    template <std::same_as<bool> B>
    void put(QByteArrayView key, B value)
    {
        putBool(key, value);
    }
    void put(QByteArrayView key, const Vector3f& v, int p = 0);
    void put(QByteArrayView key, const Vector3d& v, int p = 0);
    void put(QByteArrayView key, const Vector3d& v1, const Vector3d& v2, const Vector3d& v3);
    void put(QByteArrayView key, const TextureAxis& axis);
    void put(const BspEntity& entity);
    void put(const Texture& tex);
    void put(const EntityKeyValue& kv);

    Core::Result<void> flush();
    QByteArray finish();

private:
    void putBool(QByteArrayView key, bool value);
    void logWarning(const QString& message) const;
    void warnUnclosedSections() const;

    int m_doubleScale = 8;
    int m_doubleScaleTextureAxes = 4;
    int m_doubleScaleTextureScale = 4;
    const BspDecompileConfig* m_config = nullptr;

    Core::KeyValues::KeyValuesWriter m_kvWriter;
};

/**
 * @brief VMF metadata control class (UID allocation, visgroup tree, worldspawn, cameras).
 */
class VmfMeta {
public:
    class Visgroup {
    private:
        struct PassKey {};

    public:
        Visgroup(PassKey, VmfMeta& meta, QString name, Visgroup* parent);
        Visgroup(const Visgroup&) = delete;
        Visgroup& operator=(const Visgroup&) = delete;
        Visgroup(Visgroup&&) = delete;
        Visgroup& operator=(Visgroup&&) = delete;

        Visgroup* childVisgroup(const QString& visgroupName);
        Visgroup* setColor(const QColor& color);
        const QColor& color() const noexcept { return m_color; }
        const QString& name() const noexcept { return m_name; }
        int id() const noexcept { return m_id; }
        bool isUsed() const noexcept { return m_used; }
        void setUsed(bool used) noexcept { m_used = used; }

    private:
        friend class VmfMeta;

        Visgroup* rootVisgroup() noexcept;
        std::vector<QString> visgroupPath() const;
        bool isTreeUsed() const noexcept;
        bool containsVisgroupId(int id) const noexcept;
        QColor generateNewVisgroupColor();

        VmfMeta& m_meta;
        QString m_name;
        int m_id = 0;
        QColor m_color;
        Visgroup* m_parent = nullptr;
        std::map<QString, Visgroup, AlphanumComparator> m_children;
        bool m_used = false;
    };

    VmfMeta(BspFileReader& reader, VmfWriter& writer, const BspDecompileConfig& config);

    QSet<int>& uidBlacklist() noexcept { return m_uidBlacklist; }
    const QSet<int>& uidBlacklist() const noexcept { return m_uidBlacklist; }

    int allocateUid();

    int faceUid(int iface) const;
    std::optional<int> setFaceUid(int iface, int id);
    std::optional<int> setOrigFaceUid(int iface, int id);

    int dispInfoUid(int idispinfo) const;
    std::optional<int> setDispInfoUid(int idispinfo, int id);

    void appendComment(const QString& comment);
    std::optional<QString> comment() const;

    void writeWorldHeader();
    void writeWorldFooter();

    QString mapVersion() const;

    void writeVersionInfo(VmfWriter& targetWriter) const;
    void writeVisgroups();
    void writeVisgroups(VmfWriter& targetWriter) const;
    void writeViewSettings(VmfWriter& targetWriter) const;
    void writeMetaVisgroup(const QString& visgroupName);
    void writeMetaVisgroups(const std::vector<Visgroup*>& visgroups);

    Visgroup* visgroups() noexcept { return m_rootVisgroup ? &(*m_rootVisgroup) : nullptr; }

    bool reserveVisgroupId(int visgroupId, const std::vector<QString>& visgroupPath);

    void writeCameras();
    void writeCordon();
    std::vector<Camera>& cameras() noexcept { return m_cameras; }
    const std::vector<Camera>& cameras() const noexcept { return m_cameras; }

private:
    int newVisgroupId(const Visgroup& vg);
    void writeVisgroup(VmfWriter& targetWriter, const Visgroup& visgroup) const;
    BspEntity& worldspawnEntity() noexcept;
    const BspEntity& worldspawnEntity() const noexcept;

    BspFileReader& m_reader;
    VmfWriter& m_writer;
    const BspDecompileConfig& m_config;

    std::mt19937 m_rng;

    std::vector<int> m_faceUids;
    std::vector<int> m_origFaceUids;
    std::vector<int> m_dispinfoUids;
    QSet<int> m_uidBlacklist;
    int m_uid = 0;

    std::map<std::vector<QString>, int> m_reservedVisgroups;
    QSet<int> m_reservedVisgroupIds;
    int m_visgroupIndex = 0;
    std::optional<Visgroup> m_rootVisgroup;

    std::vector<Camera> m_cameras;

    BspEntity* m_worldspawn = nullptr;
    std::optional<BspEntity> m_fallbackWorldspawn;
};

} // namespace Domain::Bsp::Internal
