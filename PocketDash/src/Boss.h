#pragma once

#include "Math.h"

#include <array>

namespace pd {

// The Meadow Guardian: World 1's boss, a big mossy creature that sleeps in a
// walled clearing (the arena). No rendering or SDL here; LevelSession drives
// it and the unit tests play the fight directly.
//
// Every attack is announced so younger players can read it:
//   Idle      - shuffles towards you; touching it hurts.
//   Telegraph - crouches and flashes: a leap is coming.
//   Leap      - jumps at the spot you were standing on. A shadow grows
//               where it will land, so step away.
//   Landed    - the landing sends out shockwave rings across the ground:
//               HOP over them.
//   Dazed     - after landing it is dizzy (stars spin): hop on it or dash
//               into it to land a hit.
//   Hurt      - flinches, then starts again, faster.
// Six hits in three phases. Phase 2 sends two rings per landing; phase 3
// leaps twice in a row. When it is beaten it calms down and the gates open.
class Boss {
public:
    enum class State { Sleeping, Intro, Idle, Telegraph, Leap, Landed, Dazed, Hurt, Defeated };

    static constexpr int kMaxHealth = 6;
    static constexpr float kBodyRadius = 30.0f;  // contact circle around the feet
    static constexpr float kCrushRadius = 42.0f; // landing on the player
    static constexpr float kLeapHeight = 130.0f;
    static constexpr float kRingThickness = 14.0f;
    static constexpr float kRingMaxRadius = 320.0f;
    static constexpr float kIntroTime = 2.4f;
    static constexpr float kShortIntroTime = 0.8f;
    static constexpr int kMaxRings = 4;

    struct Ring {
        Vec2 center;
        float radius = 0.0f;
        float speed = 0.0f;
        float delay = 0.0f; // seconds before it starts to spread
        bool active = false;
        bool hitPlayer = false; // each ring can only hurt once
    };

    // Timings for one phase (seconds; speeds in pixels per second).
    struct PhaseRules {
        float idleTime;
        float telegraphTime;
        float leapTime;
        float dazedTime;
        float ringSpeed;
        int ringsPerLanding;
        int leapsPerAttack;
        float walkSpeed;
    };
    static const PhaseRules& rules(int phase);

    // `arena` is the clearing in pixels; leaps never leave it.
    void reset(Vec2 home, RectF arena);
    // Back to sleep at home, keeping its health (the player was knocked out).
    void sleep();
    // Starts the fight. The long intro plays only the first time.
    void wake(bool longIntro);

    void update(float dt, Vec2 playerPos);

    // Debug: ends the fight (it flinches, then calms down as if beaten).
    void debugDefeat();

    // A stomp or dash landed. Only counts while dazed; returns true if so.
    bool hit(Vec2 from);

    State state() const { return state_; }
    float stateTime() const { return stateTime_; }
    bool sleeping() const { return state_ == State::Sleeping; }
    bool fighting() const { return state_ != State::Sleeping && state_ != State::Defeated; }
    bool defeated() const { return state_ == State::Defeated; }
    bool vulnerable() const { return state_ == State::Dazed; }
    // Touching it on the ground hurts (not while dizzy, flinching or asleep).
    bool hurtsOnContact() const;
    // True for the one update in which it landed from a leap.
    bool landedThisStep() const { return landed_; }

    int health() const { return health_; }
    int hitsTaken() const { return kMaxHealth - health_; }
    // 1..3: two hits per phase.
    int phase() const;

    Vec2 position() const { return pos_; }
    float height() const { return z_; }
    Vec2 leapTarget() const { return target_; }
    // 0..1 progress through the current leap (0 when not leaping).
    float leapProgress() const;
    const RectF& arena() const { return arena_; }
    const std::array<Ring, kMaxRings>& rings() const { return rings_; }
    std::array<Ring, kMaxRings>& rings() { return rings_; }
    int leapsDone() const { return leapsDone_; }

private:
    void enter(State s);
    void spawnRings();
    Vec2 clampToArena(Vec2 p) const;

    State state_ = State::Sleeping;
    float stateTime_ = 0.0f;
    float stateLength_ = 0.0f;
    Vec2 home_;
    RectF arena_;
    Vec2 pos_;
    float z_ = 0.0f;
    Vec2 leapStart_;
    Vec2 target_;
    Vec2 knockback_;
    int health_ = kMaxHealth;
    int leapsLeft_ = 0;
    int leapsDone_ = 0;
    bool landed_ = false;
    bool introShown_ = false;
    std::array<Ring, kMaxRings> rings_{};
};

const char* bossStateName(Boss::State s);

} // namespace pd
