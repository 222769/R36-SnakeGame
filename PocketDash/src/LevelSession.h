#pragma once

#include "Boss.h"
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
    kSessionGoldenStar = 1u << 20,  // cleared with everything found
    kSessionKey = 1u << 21,         // picked up a key
    kSessionGateOpened = 1u << 22,  // a key opened a gate (the level changed)
    kSessionLocked = 1u << 23,      // bumped a gate without a key
    kSessionRescue = 1u << 24,      // a lost friend was rescued
    kSessionSign = 1u << 25,        // started reading a sign (see readingSign())
    kSessionSecret = 1u << 26,      // walked into a secret passage for the first time
    kSessionExitLocked = 1u << 27,  // touched the flag before finishing the objective
    kSessionTimeUp = 1u << 28,      // the time limit ran out
    kSessionBossIntro = 1u << 29,   // the boss woke up and the arena gates closed
    kSessionBossHit = 1u << 30,     // the boss took a hit
    kSessionBossDefeated = 1u << 31, // the boss calmed down and the gates opened
};

struct SessionStats {
    int coinPoints = 0; // coins counted for score (Double Coins counts them twice)
    int blocksBroken = 0;
    int powerUpsUsed = 0;
    int enemiesDefeated = 0;
    int heartsLost = 0;
    int knockOuts = 0;
    int splashes = 0;
    bool bossDefeated = false;
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
//  * Keys open locked gates (one key per gate). Rafts carry you over water.
//  * Y rescues lost friends and reads signs.
//  * The exit flag only opens once the level's objective is complete.
//  * A time limit (50% longer on Relaxed) ends in "time up" and a retry.
//  * Boss levels: walking into the arena wakes the boss and closes the
//    gates behind you (see Boss.h). A knock-out sends the boss back to
//    sleep, keeping the hits you landed.
//
// The session works on its own copy of the level, because smashed blocks
// change the map; restart() restores the original.
class LevelSession {
public:
    enum class State { Playing, KnockedOut, Cleared, TimeUp };

    static constexpr float kKnockOutTime = 1.2f;
    static constexpr float kCheckpointRadius = 24.0f;
    static constexpr float kInteractRadius = 40.0f;
    static constexpr float kRaftSpeed = 50.0f;
    static constexpr float kRaftHalfSize = 20.0f; // rafts are 40x40, generous to stand on

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

    // --- Phase 5: objectives and level furniture ---------------------------
    bool objectiveComplete() const;
    // Progress for the HUD, e.g. friends 1 of 3. `need` is 0 for "reach exit".
    void objectiveProgress(int& have, int& need) const;
    // Effective time limit for this difficulty (0 = none) and time left.
    float timeLimit() const;
    float timeLeft() const;

    int keysHeld() const { return keysHeld_; }
    int secretsFound() const { return secretsFound_; }
    int secretsTotal() const { return level_.secretCount(); }

    struct Friend {
        Vec2 pos;
        bool rescued = false;
        float rescueTime = 0.0f; // for the happy-hop-away animation
    };
    const std::vector<Friend>& friends() const { return friends_; }
    int friendsRescued() const;

    struct Raft {
        Vec2 pos;
        Vec2 dir;
        RectF rect() const { return RectF{pos.x - kRaftHalfSize, pos.y - kRaftHalfSize, 2 * kRaftHalfSize, 2 * kRaftHalfSize}; }
    };
    const std::vector<Raft>& rafts() const { return rafts_; }

    // What Y would interact with right now (for the on-screen prompt).
    enum class InteractKind { None, Friend, Sign };
    InteractKind interactTarget(Vec2* where = nullptr) const;
    // Index of the sign just read (valid after kSessionSign).
    int readingSign() const { return readingSign_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    bool hasBoss() const { return level_.hasBoss; }
    const Boss& boss() const { return boss_; }
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

    // Debug: calms an awake boss at once. Returns false if there is none.
    bool debugDefeatBoss();

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
    void updateRafts(float dt);
    void updateLocksAndSecrets(unsigned& events);
    void interact(unsigned& events);
    void updateBoss(float dt, unsigned& events);
    void setArenaGates(bool closed);
    bool onRaft() const;
    int nearestFriend() const;
    int nearestSign() const;
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
    int keysHeld_ = 0;
    bool lockHintLatch_ = false; // one "need a key" hint per bump
    bool exitHintLatch_ = false; // one "not yet" hint per visit to the flag
    std::vector<bool> secretFound_;
    int secretsFound_ = 0;
    std::vector<Friend> friends_;
    std::vector<Raft> rafts_;
    int readingSign_ = -1;
    std::vector<Enemy> enemies_;
    Boss boss_;
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
