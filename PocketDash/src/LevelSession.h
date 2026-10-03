#pragma once

#include "Collectibles.h"
#include "Difficulty.h"
#include "Effects.h"
#include "Enemy.h"
#include "Level.h"
#include "Player.h"
#include "PowerUp.h"

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
    kSessionStar = 1u << 12,
    kSessionGem = 1u << 13,
    kSessionPowerUpGet = 1u << 14,  // picked up into the slot
    kSessionPowerUpUse = 1u << 15,  // activated with X
    kSessionPowerUpEnd = 1u << 16,  // a timed power-up ran out
    kSessionShieldPop = 1u << 17,   // the shield bubble absorbed a hit
    kSessionBlockBroken = 1u << 18, // a crate/boulder was smashed (the level changed)
    kSessionNoRoom = 1u << 19,      // Giant Mode needs more space here
    kSessionGoldenStar = 1u << 20,  // cleared with every star, coin and gem
};

struct SessionStats {
    int coinPoints = 0; // coins counted for score (Double Coins counts them twice)
    int blocksBroken = 0;
    int powerUpsUsed = 0;
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
//  * One power-up at a time: a pickup goes into the slot and X uses it.
//    While the slot is full, other power-ups stay on the ground for later.
//  * Dashing smashes crates; Giant Mode smashes crates and boulders.
//
// The session works on its own copy of the level, because smashed blocks
// change the map; restart() restores the original.
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

    // The level as it is now (smashed blocks removed).
    const Level& level() const { return level_; }
    const Player& player() const { return player_; }
    Player& player() { return player_; }
    const CoinField& coins() const { return coins_; }
    const HeartPickups& heartPickups() const { return heartPickups_; }
    const ItemField& items() const { return items_; }
    const PowerUpState& powers() const { return powers_; }
    bool goldenStar() const { return goldenStar_; }
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
    void updatePickups(float dt, unsigned& events);
    void updatePowerUps(bool usePressed, float dt, unsigned& events);
    bool tryActivate(PowerUpType type, unsigned& events);
    void smashBlocks(unsigned& events);
    PlayerModifiers modifiersFromPowers() const;
    bool invulnerable() const { return powers_.active(PowerUpType::RainbowStar); }
    bool crushesEnemies() const {
        return powers_.active(PowerUpType::RainbowStar) || powers_.active(PowerUpType::GiantMode);
    }

    const Level& source_; // pristine level, for restart()
    Level level_;         // working copy (blocks can be smashed)
    Difficulty difficulty_;
    DifficultyRules rules_;

    Player player_;
    CoinField coins_;
    HeartPickups heartPickups_;
    ItemField items_;
    PowerUpState powers_;
    bool goldenStar_ = false;
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
