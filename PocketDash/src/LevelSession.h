#pragma once

#include "Collectibles.h"
#include "Difficulty.h"
#include "Effects.h"
#include "Enemy.h"
#include "Level.h"
#include "Player.h"

#include <vector>

namespace pd {

// Things that happened during one update, for sounds, shakes and tests.
enum SessionEvent : unsigned {
    kSessionNone = 0,
    kSessionHopped = 1u << 0,
    kSessionDashed = 1u << 1,
    kSessionLanded = 1u << 2,
    kSessionCoin = 1u << 3,
    kSessionHurt = 1u << 4,       // lost a heart (enemy, thorns, shockwave)
    kSessionSplash = 1u << 5,     // fell in water
    kSessionKnockedOut = 1u << 6, // hearts ran out
    kSessionRespawned = 1u << 7,  // back at a checkpoint after a knock-out
    kSessionStomp = 1u << 8,      // enemy defeated
    kSessionCheckpoint = 1u << 9,
    kSessionHeart = 1u << 10,     // heart pickup
    kSessionCleared = 1u << 11,
};

struct SessionStats {
    int enemiesDefeated = 0;
    int heartsLost = 0;
    int knockOuts = 0;
    int splashes = 0;
};

// The gameplay simulation of one level: player, enemies, coins, hearts,
// hazards, checkpoints and the exit. Contains no rendering, audio or input
// code, so whole play-throughs can be unit-tested with ASCII maps.
//
// Rules (forgiving by design):
//  * Hop onto an enemy (stomp) or dash into it to defeat it. Touching one
//    on foot costs a heart; you are briefly invincible afterwards.
//  * Thorns hurt when your feet touch them, unless you hop over. Water can
//    be hopped over or skimmed while dashing; falling in costs a heart and
//    puts you back on the last safe spot.
//  * At zero hearts there is no game over: after a short "oops" you return
//    to the last checkpoint (or the start) with full hearts, keeping coins.
class LevelSession {
public:
    enum class State { Playing, KnockedOut, Cleared };

    static constexpr float kKnockOutTime = 1.2f;
    static constexpr float kCheckpointRadius = 24.0f;

    LevelSession(const Level& level, Difficulty difficulty);

    void restart();
    unsigned update(const PlayerInput& input, float dt);

    State state() const { return state_; }
    float time() const { return time_; }
    float stateTime() const { return stateTime_; }
    int hearts() const { return hearts_; }
    int maxHearts() const { return rules_.maxHearts; }
    Difficulty difficulty() const { return difficulty_; }
    const DifficultyRules& rules() const { return rules_; }
    const SessionStats& stats() const { return stats_; }

    const Level& level() const { return level_; }
    const Player& player() const { return player_; }
    Player& player() { return player_; }
    const CoinField& coins() const { return coins_; }
    const HeartPickups& heartPickups() const { return heartPickups_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    int enemiesAlive() const;
    const Effects& effects() const { return effects_; }

    struct Checkpoint {
        Vec2 pos;
        bool reached = false;
    };
    const std::vector<Checkpoint>& checkpoints() const { return checkpoints_; }
    int activeCheckpoint() const { return activeCheckpoint_; }
    Vec2 respawnPoint() const;
    Vec2 lastSafePosition() const { return lastSafe_; }

    // Debug: puts the player on the first free tile near `target`.
    bool warpNear(Vec2 target);

private:
    void hurt(Vec2 source, unsigned& events);
    void knockOut(unsigned& events);
    void updateHazards(unsigned& events);
    void updateEnemies(float dt, unsigned& events);
    void updatePickups(unsigned& events);

    const Level& level_;
    Difficulty difficulty_;
    DifficultyRules rules_;

    Player player_;
    CoinField coins_;
    HeartPickups heartPickups_;
    std::vector<Enemy> enemies_;
    std::vector<Checkpoint> checkpoints_;
    int activeCheckpoint_ = -1;
    Effects effects_;

    State state_ = State::Playing;
    float time_ = 0.0f;
    float stateTime_ = 0.0f;
    int hearts_ = 3;
    Vec2 lastSafe_;
    SessionStats stats_;
};

} // namespace pd
