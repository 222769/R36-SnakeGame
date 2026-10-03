#pragma once

// Plain data describing what a level places in the world. Kept separate
// from the runtime classes so Level stays free of gameplay/rendering code.

#include "Math.h"
#include "PowerUp.h"
#include "Settings.h"

namespace pd {

// Cute, readable enemies. Each type has one simple behaviour pattern.
enum class EnemyType {
    Slime,        // squishes (telegraph), then hops towards the player
    Beetle,       // walks back and forth along a line
    Mushroom,     // bounces in place; big bounces send a shockwave ring to hop over
    Cloud,        // (later) drifts and drops rain below itself
    Robot,        // (later) patrols a rectangle, turns at walls
    RollingRock,  // (later) rolls along a lane, can be hopped over
    SpinFlower,   // (later) stationary, spins petals around itself
    Count
};

struct EnemySpawn {
    EnemyType type = EnemyType::Slime;
    Vec2 pos;
    Vec2 dir{1.0f, 0.0f}; // patrol direction (beetles)
};

// A checkpoint exists on its own difficulty and every easier one, so
// Relaxed has the most checkpoints and Challenge the fewest.
struct CheckpointSpawn {
    Vec2 pos;
    Difficulty hardest = Difficulty::Challenge;
};

struct PowerUpSpawn {
    PowerUpType type = PowerUpType::SpeedShoes;
    Vec2 pos;
};

} // namespace pd
