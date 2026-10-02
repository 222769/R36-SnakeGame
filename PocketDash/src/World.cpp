#include "World.h"

#include <algorithm>

namespace pd {

namespace {

constexpr SDL_Color c(Uint8 r, Uint8 g, Uint8 b) { return SDL_Color{r, g, b, 255}; }

const WorldDef kWorlds[kWorldCount] = {
    {WorldId::SunnyMeadows, "SUNNY MEADOWS", "DASH AND HOP", "MEADOW GUARDIAN", 8, true,
     {c(112, 200, 88), c(100, 186, 78), c(64, 140, 64), c(44, 104, 52), c(255, 214, 64), c(80, 160, 240)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::FrozenPeaks, "FROZEN PEAKS", "SLIPPERY ICE", "FROST YETI", 8, false,
     {c(220, 236, 250), c(204, 224, 244), c(150, 180, 220), c(110, 140, 190), c(120, 220, 255), c(140, 200, 250)},
     {0.25f, 1.0f, 1.0f}},
    {WorldId::DesertRuins, "DESERT RUINS", "MOVING PLATFORMS", "SAND SPHINX", 8, false,
     {c(236, 204, 132), c(224, 190, 118), c(180, 130, 80), c(140, 96, 60), c(255, 160, 60), c(70, 170, 200)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::CandyKingdom, "CANDY KINGDOM", "BOUNCE PADS", "JELLY KING", 8, false,
     {c(255, 200, 224), c(250, 184, 214), c(230, 110, 170), c(190, 80, 140), c(130, 230, 220), c(255, 140, 200)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::SpookyWoods, "SPOOKY WOODS", "MOVING SHADOWS", "PUMPKIN WITCH", 8, false,
     {c(70, 70, 110), c(62, 62, 100), c(40, 36, 70), c(28, 24, 52), c(255, 150, 40), c(90, 80, 160)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::VolcanoValley, "VOLCANO VALLEY", "FALLING ROCKS", "MAGMA DRAGON", 8, false,
     {c(120, 80, 70), c(108, 72, 64), c(70, 44, 44), c(50, 30, 30), c(255, 120, 40), c(255, 90, 30)},
     {1.0f, 1.0f, 1.0f}},
    {WorldId::CosmicZone, "COSMIC ZONE", "LOW GRAVITY", "STAR EMPEROR", 8, false,
     {c(50, 40, 90), c(44, 34, 80), c(110, 90, 190), c(80, 60, 150), c(255, 240, 120), c(120, 100, 220)},
     {1.0f, 0.45f, 1.0f}},
};

} // namespace

const WorldDef& worldDef(WorldId id) { return worldDef(static_cast<int>(id)); }

const WorldDef& worldDef(int index) { return kWorlds[std::clamp(index, 0, kWorldCount - 1)]; }

} // namespace pd
