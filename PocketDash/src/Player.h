#pragma once

#include "Math.h"

#include <SDL.h>

namespace pd {

class Level;

enum class Facing { Down, Up, Left, Right };

Vec2 facingVector(Facing f);

// What the player wants to do this step. Built from InputManager by the
// scene, which keeps Player free of SDL input details (and unit-testable).
struct PlayerInput {
    Vec2 move;                // length 0..1
    bool hopPressed = false;  // A
    bool dashPressed = false; // B
};

// Movement feel. All speeds in pixels/second at 640x480.
struct PlayerTuning {
    float maxSpeed = 150.0f;
    float acceleration = 1600.0f;   // when steering
    float friction = 1900.0f;       // when releasing the stick
    float dashSpeed = 440.0f;
    float dashTime = 0.15f;
    float dashCooldown = 0.35f;     // after the dash ends
    float hopVelocity = 210.0f;     // vertical (z) take-off speed
    float gravity = 1150.0f;
    float knockbackSpeed = 300.0f;
    float stunTime = 0.22f;         // no control after being hit
    float invincibleTime = 1.3f;
};

// Bit flags returned from Player::update so the scene can react with
// sounds and effects without Player knowing about either.
enum PlayerEvent : unsigned {
    kEventNone = 0,
    kEventHopped = 1u << 0,
    kEventDashed = 1u << 1,
    kEventLanded = 1u << 2,
};

// Top-down player. Position is the centre of the feet on the ground plane;
// hops are simulated on a separate height axis (z) so the player can hop
// over low hazards without leaving the 2D map.
class Player {
public:
    static constexpr float kHitboxW = 18.0f;
    static constexpr float kHitboxH = 12.0f;
    // How far the player slips sideways around wall corners (see moveAndCollide).
    static constexpr float kCornerNudge = 7.0f;

    explicit Player(Vec2 spawn = {});

    // Advances one fixed step. With a level, movement collides with its
    // solid tiles; without one the player moves freely (tests, menus).
    // Returns a mask of PlayerEvent flags.
    unsigned update(const PlayerInput& input, float dt, const Level* level = nullptr);

    // Applies knockback away from `source` and starts invincibility.
    // Returns false (no effect) while already invincible.
    bool takeHit(Vec2 source);

    // Springs back up after stomping an enemy.
    void bounce();

    // Puts the player at `pos` (water fall, checkpoint), standing still.
    // With `freshInvincibility` the full invincibility time restarts so they
    // are never hurt again straight away; otherwise any running timer just
    // continues (so repeated falls can't chain invincibility forever).
    void respawnAt(Vec2 pos, bool freshInvincibility = true);

    // Keeps the hitbox inside `area`, cancelling velocity into the walls.
    void constrainTo(const RectF& area);

    void setPosition(Vec2 p) { pos_ = p; }
    void stop() { vel_ = {}; }

    Vec2 position() const { return pos_; }
    Vec2 velocity() const { return vel_; }
    float height() const { return z_; }
    Facing facing() const { return facing_; }
    int animFrame() const { return animFrame_; }
    RectF hitbox() const;

    bool isAirborne() const { return z_ > 0.0f || vz_ > 0.0f; }
    // Coming down from a hop: the moment a stomp counts.
    bool isFalling() const { return z_ > 0.0f && vz_ < 0.0f; }
    bool isDashing() const { return dashTimer_ > 0.0f; }
    bool isStunned() const { return stunTimer_ > 0.0f; }
    bool isInvincible() const { return invincibleTimer_ > 0.0f; }
    bool canDash() const { return dashCooldown_ <= 0.0f && !isDashing(); }

    PlayerTuning& tuning() { return tuning_; }
    const PlayerTuning& tuning() const { return tuning_; }

    // Draws the player at its position minus `camera` (screen = world - camera).
    void render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera) const;

private:
    void updateFacing(Vec2 dir);

    PlayerTuning tuning_;
    Vec2 pos_;
    Vec2 vel_;
    float z_ = 0.0f;
    float vz_ = 0.0f;
    Facing facing_ = Facing::Down;

    Vec2 dashDir_;
    float dashTimer_ = 0.0f;
    float dashCooldown_ = 0.0f;
    float stunTimer_ = 0.0f;
    float invincibleTimer_ = 0.0f;

    float animTimer_ = 0.0f;
    int animFrame_ = 0;
    float squashTimer_ = 0.0f; // brief squash on landing
};

} // namespace pd
