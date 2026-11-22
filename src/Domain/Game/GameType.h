#pragma once

namespace Domain::Game {

enum class EngineType : int {
    Source1,
    Source2,
};

enum class GameType : int {
    Unknown,
    CS2,
    CSGO,
    CSS,
    HL2,
    L4D,
    L4D2,
    Portal,
    Portal2,
    TF2,
    GMod,
    BlackMesa,
    Custom,
};

} // namespace Domain::Game

