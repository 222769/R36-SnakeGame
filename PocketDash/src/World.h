#pragma once

#include <SDL.h>

namespace pd {

enum class WorldId { SunnyMeadows, FrozenPeaks, DesertRuins, CandyKingdom, SpookyWoods, VolcanoValley, CosmicZone, Count };

constexpr int kWorldCount = static_cast<int>(WorldId::Count);

// Colours used by the placeholder tile renderer for each world.
struct WorldTheme {
    SDL_Color ground;
    SDL_Color groundAlt;
    SDL_Color wall;
    SDL_Color wallShade;
    SDL_Color accent;
    SDL_Color water;
};

// Per-world physics modifiers: each world introduces one mechanic, mostly
// expressed as data so Player/Level code stays generic.
struct WorldRules {
    float frictionScale = 1.0f;  // < 1 = slippery (Frozen Peaks ice)
    float gravityScale = 1.0f;   // < 1 = floaty hops (Cosmic Zone)
    float speedScale = 1.0f;
};

struct WorldDef {
    WorldId id;
    const char* name;
    const char* mechanic;      // short description shown on level select
    const char* bossName;
    int levelCount;            // levels planned for this world
    bool implemented;          // false = shown as "coming soon"
    WorldTheme theme;
    WorldRules rules;
};

const WorldDef& worldDef(WorldId id);
const WorldDef& worldDef(int index);

} // namespace pd
