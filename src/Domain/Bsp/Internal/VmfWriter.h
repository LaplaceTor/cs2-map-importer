#pragma once

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QLatin1StringView>
#include <QSet>
#include <QString>
#include <QStringView>

#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

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
 */
class VmfWriter {
public:
    VmfWriter(
        int doubleScale,
        int doubleScaleTextureAxes,
        int doubleScaleTextureScale,
        bool nullOutput = false,
        const BspDecompileConfig* config = nullptr);

    void start(QStringView name);
    void end(QStringView name);

    void put(QStringView key, QStringView value);
    void put(QStringView key, QLatin1StringView value);
    void put(QStringView key, const char16_t* value);
    void put(QStringView key, const char* value);
    void put(QStringView key, int value);
    void put(QStringView key, int v0, int v1, int v2);
    void put(QStringView key, double value);
    template <std::same_as<bool> B>
    void put(QStringView key, B value)
    {
        putBool(key, value);
    }
    void put(QStringView key, const Vector3f& v, int p = 0);
    void put(QStringView key, const Vector3d& v, int p = 0);
    void put(QStringView key, const Vector3d& v1, const Vector3d& v2, const Vector3d& v3);
    void put(QStringView key, const TextureAxis& axis);
    void put(const std::vector<std::pair<QString, QString>>& orderedMap);
    void put(const BspEntity& entity);
    void put(const Texture& tex);
    void put(const EntityKeyValue& kv);

    QByteArray finish();

private:
    void putBool(QStringView key, bool value);
    void appendIndent();
    void appendUtf8(QStringView text);
    void logWarning(const QString& message) const;

    QString formatVector3d(const Vector3d& v, int p) const;
    QString formatTextureAxis(const TextureAxis& tx) const;
    QString formatFloat(double f, int decimalPlaces) const;

    int m_doubleScale = 8;
    int m_doubleScaleTextureAxes = 4;
    int m_doubleScaleTextureScale = 4;
    bool m_nullOutput = false;
    const BspDecompileConfig* m_config = nullptr;

    QByteArray m_buffer;
    std::vector<QString> m_section;
};

/**
 * @brief VMF metadata control class (UID allocation, visgroup tree, worldspawn, cameras).
 */
class VmfMeta {
public:
    class Visgroup {
    public:
        Visgroup* getVisgroup(const QString& visgroupName);
        Visgroup* setColor(const QColor& color);
        const QColor& color() const noexcept { return m_color; }
        const QString& name() const noexcept { return m_name; }
        int id() const noexcept { return m_id; }
        bool isUsed() const noexcept { return m_used; }
        void setUsed(bool used) noexcept { m_used = used; }

    private:
        friend class VmfMeta;

        Visgroup(VmfMeta& meta, QString name, Visgroup* parent);

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
        std::map<QString, std::unique_ptr<Visgroup>, AlphanumComparator> m_children;
        bool m_used = false;
    };

    VmfMeta(BspFileReader& reader, VmfWriter& writer, const BspDecompileConfig& config);

    QSet<int>& getUIDBlackList() noexcept { return m_uidBlacklist; }
    const QSet<int>& getUIDBlackList() const noexcept { return m_uidBlacklist; }

    int getUID();

    int getFaceUID(int iface) const;
    std::optional<int> setFaceUID(int iface, int id);
    std::optional<int> setOrigFaceUID(int iface, int id);

    int getDispInfoUID(int idispinfo) const;
    std::optional<int> setDispInfoUID(int idispinfo, int id);

    void appendComment(const QString& comment);
    std::optional<QString> getComment() const;

    void writeWorldHeader();
    void writeWorldFooter();

    void writeVisgroups();
    void writeMetaVisgroup(const QString& visgroupName);
    void writeMetaVisgroups(const std::vector<Visgroup*>& visgroups);

    Visgroup* visgroups() noexcept { return m_rootVisgroup.get(); }

    bool reserveVisgroupId(int visgroupId, const std::vector<QString>& visgroupPath);

    void writeCameras();
    std::vector<Camera>& getCameras() noexcept { return m_cameras; }
    const std::vector<Camera>& getCameras() const noexcept { return m_cameras; }

private:
    int getNewVisgroupId(const Visgroup& vg);
    void writeVisgroup(const Visgroup& visgroup);
    BspEntity& worldspawnEntity() noexcept;
    const BspEntity& worldspawnEntity() const noexcept;

    BspFileReader& m_reader;
    VmfWriter& m_writer;
    const BspDecompileConfig& m_config;

    std::mt19937 m_rng;

    QHash<int, int> m_faceUIDs;
    QHash<int, int> m_origFaceUIDs;
    QHash<int, int> m_dispinfoUIDs;
    QSet<int> m_uidBlacklist;
    int m_uid = 0;

    std::map<std::vector<QString>, int> m_reservedVisgroups;
    QSet<int> m_reservedVisgroupIds;
    int m_visgroupIndex = 0;
    std::unique_ptr<Visgroup> m_rootVisgroup;

    std::vector<Camera> m_cameras;

    BspEntity* m_worldspawn = nullptr;
    std::optional<BspEntity> m_fallbackWorldspawn;
};

} // namespace Domain::Bsp::Internal
