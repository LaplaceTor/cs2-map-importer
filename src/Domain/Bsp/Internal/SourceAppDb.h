#pragma once

#include <optional>

#include <QSet>
#include <QString>

namespace Domain::Bsp::Internal {

namespace SourceAppId {
    constexpr int kUnknown = 0;
    constexpr int kDefault = -1;
    constexpr int kHalfLifeSource = 280;
    constexpr int kHalfLife2 = 220;
    constexpr int kHalfLife2Ep1 = 380;
    constexpr int kHalfLife2Ep2 = 420;
    constexpr int kHalfLife2Dm = 320;
    constexpr int kCounterStrikeSource = 240;
    constexpr int kCounterStrikeGo = 730;
    constexpr int kDayOfDefeatSource = 300;
    constexpr int kTeamFortress2 = 440;
    constexpr int kPortal = 400;
    constexpr int kPortal2 = 620;
    constexpr int kLeft4Dead = 500;
    constexpr int kLeft4Dead2 = 550;
    constexpr int kAlienSwarm = 630;
    constexpr int kDota2Beta = 570;
    constexpr int kVampireBloodlines = 2600;
    constexpr int kDarkMessiah = 2100;
    constexpr int kSiNEpisodes = 1300;
    constexpr int kTheShip = 2400;
    constexpr int kBloodyGoodTime = 2450;
    constexpr int kGarrysMod = 4000;
    constexpr int kPostal3 = 10220;
    constexpr int kZenoClash = 22200;
    constexpr int kVindictus = 212160;
    constexpr int kDearEsther = 203810;
    constexpr int kTacticalIntervention = 51100;
    constexpr int kSynergy = 17520;
    constexpr int kZombiePanicSource = 17500;
    constexpr int kNoMoreRoomInHell = 224260;
    constexpr int kBladeSymphony = 225600;
    constexpr int kContagion = 238430;
    constexpr int kCyberDiver = 100001;
    constexpr int kBlackMesa = 362890;
    constexpr int kInsurgency = 222880;
    constexpr int kTitanfall = 1454890;
    constexpr int kStrataSource = -100;
} // namespace SourceAppId

class SourceAppDb {
public:
    static const SourceAppDb& instance();

    /// Auto-detects the SourceAppId from map name, BSP version, and entity class names.
    int find(const QString& bspName, int bspVersion, const QSet<QString>& entityClasses) const;

    /// Returns human-readable game name for appId, or std::nullopt if unknown.
    std::optional<QString> getName(int appId) const;

private:
    SourceAppDb() = default;
};

} // namespace Domain::Bsp::Internal
