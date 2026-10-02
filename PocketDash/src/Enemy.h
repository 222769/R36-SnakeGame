#pragma once

#include "EntityTypes.h"
#include "Math.h"

#include <SDL.h>

namespace pd {

class Level;

// One enemy. Behaviours are deliberately simple and always telegraphed so
// younger players can read them:
//   Slime    - squishes for a moment, then hops a short way (towards the
//              player when close, otherwise wandering).
//   Beetle   - walks back and forth along one axis, pausing to turn.
//   Mushroom - bounces in place; every few seconds it shivers, does a big
//              bounce and sends out a shockwave ring you can hop over.
//
// Enemies never walk into walls, water, thorns, secret walls or the exit.
class Enemy {
public:
    explicit Enemy(const EnemySpawn& spawn);

    // `dt` is already scaled by the difficulty's enemy speed.
    void update(float dt, const Level& level, Vec2 playerPos);

    void defeat();
    bool alive() const { return alive_; }
    // Still visible (alive, or playing the defeat animation).
    bool visible() const { return alive_ || deathTimer_ > 0.0f; }
    // Touching it hurts right now (mid-hop slimes fly over you).
    bool dangerous() const { return alive_ && z_ < 6.0f; }

    // Mushroom shockwave: true if the ring currently passes over `p`.
    bool ringHits(Vec2 p) const;
    bool ringActive() const { return ringRadius_ > 0.0f; }
    float ringRadius() const { return ringRadius_; }

    EnemyType type() const { return type_; }
    Vec2 position() const { return pos_; }
    float height() const { return z_; }
    RectF hitbox() const;

    void render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera) const;

    static constexpr float kRingMaxRadius = 64.0f;
    static constexpr float kRingThickness = 10.0f;

private:
    bool tryMove(const Level& level, Vec2 delta);
    void updateSlime(float dt, const Level& level, Vec2 playerPos);
    void updateBeetle(float dt, const Level& level);
    void updateMushroom(float dt);

    EnemyType type_;
    Vec2 pos_;
    Vec2 home_;
    Vec2 dir_;
    Vec2 vel_;
    float z_ = 0.0f;
    float timer_ = 0.0f;
    float phaseTime_ = 0.0f; // length of the current phase
    int phase_ = 0;          // behaviour-specific state
    int wanderIndex_ = 0;
    float animTimer_ = 0.0f;
    float ringRadius_ = 0.0f;
    bool alive_ = true;
    float deathTimer_ = 0.0f;
};

const char* enemyTypeName(EnemyType type);

} // namespace pd
