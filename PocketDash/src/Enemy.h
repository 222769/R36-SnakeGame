#pragma once

#include "Math.h"

namespace pd {

// Cute, readable enemies. Each type has one simple behaviour pattern.
enum class EnemyType {
    Slime,        // hops slowly towards the player
    Beetle,       // walks back and forth along a line
    Mushroom,     // bounces in place, periodically jumps
    Cloud,        // drifts and drops rain below itself
    Robot,        // patrols a rectangle, turns at walls
    RollingRock,  // rolls along a lane, can be hopped over
    SpinFlower,   // stationary, spins petals around itself
    Count
};

// Phase 1 skeleton: data only. Behaviours, damage and rendering are
// implemented in Phase 3.
struct Enemy {
    EnemyType type = EnemyType::Slime;
    Vec2 pos;
    Vec2 vel;
    Vec2 home;          // spawn point / patrol anchor
    float timer = 0.0f; // behaviour state timer
    int health = 1;
    bool alive = true;

    RectF hitbox() const;
    void update(float dt, Vec2 playerPos, float speedScale);
};

const char* enemyTypeName(EnemyType type);

} // namespace pd
