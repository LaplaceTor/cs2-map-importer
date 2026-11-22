#pragma once

#include <vector>

#include <QSet>
#include <QString>
#include <QStringList>

#include "Domain/Bsp/BspDecompileConfig.h"
#include "Domain/Bsp/Internal/BspFileReader.h"
#include "Domain/Bsp/Internal/BspStructs.h"
#include "Domain/Bsp/Internal/TextureSource.h"
#include "Domain/Bsp/Internal/Winding.h"

namespace Domain::Bsp::Internal {

/**
 * @brief Detects VMEX, BSPProtect, and IID anti-decompile protection methods in BSP maps.
 *
 * 1:1 port of info.ata4.bspsrc.decompiler.modules.BspProtection.
 */
class BspProtection {
public:
    static constexpr QLatin1StringView kBspProtectFile{"entities.dat"};
    static constexpr QLatin1StringView kVmexLockedTex{"tools/locked"};
    static constexpr QLatin1StringView kVmexLockedEnt{"no_decomp"};

    BspProtection(
        BspFileReader& reader,
        BrushBounds& brushBounds,
        TextureSource& texsrc,
        bool isFixTextureNames,
        const BspDecompileConfig& config);

    bool check();
    bool isProtected() const noexcept;

    bool hasEntityFlag() const noexcept { return m_flaggedEnt; }
    bool hasTextureFlag() const noexcept { return m_flaggedTex; }
    bool hasBrushFlag() const noexcept { return m_flaggedBrush; }
    bool hasEncryptedEntities() const noexcept { return m_encryptedEnt; }
    bool hasObfuscatedEntities() const noexcept { return m_obfuscatedEnt; }
    bool hasModifiedTexinfo() const noexcept { return m_modifiedTexinfo; }

    QStringList protectionMethods() const;
    const std::vector<int>& protectedBrushIndices() const noexcept { return m_protBrushIndices; }
    bool isProtectedBrushIndex(int brushIndex) const noexcept;
    bool isProtectedEntityIndex(int entityIndex) const noexcept;

private:
    void checkBrushes();
    void checkBrushSides();
    void checkEntities();
    void checkTextures();
    void checkPakfile();

    bool isAlignedBrush(const DBrush& brush) const;
    bool isSameTexBrush(const DBrush& brush) const;

    BspFileReader& m_reader;
    BrushBounds& m_brushBounds;
    TextureSource& m_texsrc;
    bool m_isFixTextureNames = true;
    const BspDecompileConfig& m_config;

    bool m_flaggedEnt = false;
    bool m_flaggedTex = false;
    bool m_flaggedBrush = false;
    bool m_encryptedEnt = false;
    bool m_obfuscatedEnt = false;
    bool m_modifiedTexinfo = false;

    std::vector<int> m_protBrushIndices;
    QSet<int> m_protEntityIndices;
};

} // namespace Domain::Bsp::Internal
