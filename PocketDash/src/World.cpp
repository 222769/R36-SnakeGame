#include "World.h"

#include <algorithm>

namespace pd {

namespace {

constexpr SDL_Color c(Uint8 r, Uint8 g, Uint8 b) { return SDL_Color{r, g, b, 255}; }

const WorldDef kWorlds[kWorldCount] = {
    {WorldId::SunnyMeadows, "SUNNY MEADOWS", "DASH AND HOP", "MEADOW GUARDIAN", 8, true,
     {c(104, 148, 64), c(100, 143, 61), c(60, 108, 50), c(36, 72, 36), c(236, 196, 84), c(54, 118, 164)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::FrozenPeaks, "FROZEN PEAKS", "SLIPPERY ICE", "FROST YETI", 8, false,
     {c(222, 230, 238), c(212, 222, 232), c(150, 172, 196), c(104, 126, 156), c(130, 196, 236), c(96, 150, 196)},
     {0.25f, 1.0f, 1.0f}},
    {WorldId::DesertRuins, "DESERT RUINS", "MOVING PLATFORMS", "SAND SPHINX", 8, false,
     {c(214, 186, 132), c(206, 178, 124), c(168, 128, 86), c(124, 92, 62), c(232, 160, 72), c(64, 140, 170)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::CandyKingdom, "CANDY KINGDOM", "BOUNCE PADS", "JELLY KING", 8, false,
     {c(240, 206, 220), c(232, 196, 212), c(206, 120, 160), c(160, 84, 124), c(120, 210, 200), c(214, 140, 190)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::SpookyWoods, "SPOOKY WOODS", "MOVING SHADOWS", "PUMPKIN WITCH", 8, false,
     {c(66, 74, 70), c(60, 68, 64), c(38, 48, 44), c(24, 30, 28), c(228, 140, 52), c(52, 70, 96)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::VolcanoValley, "VOLCANO VALLEY", "FALLING ROCKS", "MAGMA DRAGON", 8, false,
     {c(104, 82, 72), c(96, 76, 66), c(64, 48, 44), c(42, 30, 28), c(240, 120, 50), c(226, 92, 40)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::CosmicZone, "COSMIC ZONE", "LOW GRAVITY", "STAR EMPEROR", 8, false,
     {c(58, 54, 88), c(52, 48, 80), c(100, 88, 160), c(70, 60, 120), c(240, 226, 140), c(92, 84, 170)},
     {1.0f, 0.45f, 1.0f}},
};

} // namespace

const WorldDef& worldDef(WorldId id) { return worldDef(static_cast<int>(id)); }

const WorldDef& worldDef(int index) { return kWorlds[std::clamp(index, 0, kWorldCount - 1)]; }

} // namespace pd
