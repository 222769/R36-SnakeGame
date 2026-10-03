#include "LevelSession.h"

#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace pd {

namespace {

constexpr SDL_Color kDustColor{245, 240, 225, 200};
constexpr SDL_Color kGold{255, 214, 64, 255};
constexpr SDL_Color kWhite{255, 255, 255, 255};
constexpr SDL_Color kLeaf{110, 170, 70, 255};
constexpr SDL_Color kWater{170, 220, 255, 255};
constexpr SDL_Color kMint{120, 230, 200, 255};

// How high the player must be to pass over enemies and thorns unharmed.
constexpr float kClearHeight = 8.0f;
// A stomp only counts near the end of a hop (feet close to the enemy's head).
constexpr float kStompHeight = 16.0f;

// Body centre, used for pickups (the position is the feet).
Vec2 bodyCenter(const Player& p) { return p.position() - Vec2{0.0f, 10.0f}; }

} // namespace

namespace {
constexpr float kMagnetRadius = 4.0f * kTileSize;
constexpr SDL_Color kChips{196, 136, 74, 255};
constexpr SDL_Color kShieldBlue{150, 220, 255, 255};
} // namespace

LevelSession::LevelSession(const Level& level, Difficulty difficulty)
    : source_(level), level_(level), difficulty_(difficulty), rules_(difficultyRules(difficulty)) {
    restart();
}

void LevelSession::restart() {
    level_ = source_; // un-smash every block
    player_ = Player(level_.spawn);
    coins_.reset(level_.coins);
    heartPickups_.reset(level_.hearts);
    items_.reset(level_.stars, level_.gems, level_.powerUps, level_.keys);
    powers_.clear();
    goldenStar_ = false;
    keysHeld_ = 0;
    lockHintLatch_ = false;
    exitHintLatch_ = false;
    secretFound_.assign(static_cast<size_t>(level_.secretCount()), false);
    secretsFound_ = 0;
    friends_.clear();
    for (Vec2 f : level_.friends) friends_.push_back({f, false, 0.0f});
    rafts_.clear();
    for (const RaftSpawn& r : level_.rafts) rafts_.push_back({r.pos, r.dir});
    readingSign_ = -1;

    enemies_.clear();
    enemies_.reserve(level_.enemies.size());
    for (const EnemySpawn& s : level_.enemies) enemies_.emplace_back(s);
    if (level_.hasBoss) boss_.reset(level_.bossSpawn, level_.bossArena);

    checkpoints_.clear();
    for (const CheckpointSpawn& c : level_.checkpoints)
        if (checkpointEnabled(c.hardest, difficulty_)) checkpoints_.push_back({c.pos, false});
    activeCheckpoint_ = -1;

    effects_.clear();
    state_ = State::Playing;
    time_ = 0.0f;
    stateTime_ = 0.0f;
    hearts_ = rules_.maxHearts;
    lastSafe_ = level_.spawn;
    stats_ = SessionStats{};
}

int LevelSession::enemiesAlive() const {
    int n = 0;
    for (const Enemy& e : enemies_)
        if (e.alive()) ++n;
    return n;
}

Vec2 LevelSession::respawnPoint() const {
    return activeCheckpoint_ >= 0 ? checkpoints_[static_cast<size_t>(activeCheckpoint_)].pos : level_.spawn;
}

unsigned LevelSession::update(const PlayerInput& input, float dt) {
    unsigned events = kSessionNone;
    stateTime_ += dt;

    if (state_ == State::TimeUp) {
        effects_.update(dt);
        return events;
    }

    if (state_ == State::Cleared) {
        // Victory hops while the results are shown.
        PlayerInput celebrate;
        celebrate.hopPressed = !player_.isAirborne();
        player_.update(celebrate, dt, &level_);
        effects_.update(dt);
        return events;
    }

    if (state_ == State::KnockedOut) {
        effects_.update(dt);
        if (stateTime_ >= kKnockOutTime) {
            hearts_ = rules_.maxHearts;
            if (level_.hasBoss && boss_.fighting()) {
                // The boss naps again (keeping its injuries) and the gates open.
                boss_.sleep();
                setArenaGates(false);
            }
            player_.respawnAt(respawnPoint());
            lastSafe_ = respawnPoint();
            state_ = State::Playing;
            stateTime_ = 0.0f;
            effects_.spawn(Effects::Type::Sparkle, bodyCenter(player_), kMint);
            events |= kSessionRespawned;
        }
        return events;
    }

    time_ += dt;
    for (Friend& f : friends_)
        if (f.rescued) f.rescueTime += dt;
    updateRafts(dt); // carries the player before they move
    updatePowerUps(input.usePressed, dt, events);
    if (input.interactPressed) interact(events);
    const unsigned pe = player_.update(input, dt, &level_);
    if (pe & kEventHopped) events |= kSessionHopped;
    if (pe & kEventDashed) {
        events |= kSessionDashed;
        effects_.spawn(Effects::Type::Dust, player_.position(), kDustColor);
    }
    if (pe & kEventLanded) {
        events |= kSessionLanded;
        effects_.spawn(Effects::Type::Dust, player_.position(), kDustColor);
    }

    smashBlocks(events);
    updateLocksAndSecrets(events);
    updateEnemies(dt, events);
    if (level_.hasBoss) updateBoss(dt, events);
    if (state_ == State::Playing) updateHazards(events);
    if (state_ == State::Playing) updatePickups(dt, events);

    const bool atExit = state_ == State::Playing && level_.overlapsTile(player_.hitbox(), Tile::Exit);
    if (atExit && !objectiveComplete()) {
        if (!exitHintLatch_) events |= kSessionExitLocked;
        exitHintLatch_ = true;
    } else if (!atExit) {
        exitHintLatch_ = false;
    }
    if (atExit && objectiveComplete()) {
        state_ = State::Cleared;
        stateTime_ = 0.0f;
        player_.stop();
        events |= kSessionCleared;
        goldenStar_ = items_.starsCollected() == items_.starsTotal() && coins_.collected() == coins_.total() &&
                      items_.gemsCollected() == items_.gemsTotal() && friendsRescued() == static_cast<int>(friends_.size()) &&
                      secretsFound_ == level_.secretCount();
        if (goldenStar_) events |= kSessionGoldenStar;
        for (int i = 0; i < 3; ++i)
            effects_.spawn(Effects::Type::Sparkle, level_.exit + Vec2{(i - 1) * 14.0f, -10.0f - i * 6.0f}, kGold);
    }

    if (state_ == State::Playing && timeLimit() > 0.0f && time_ >= timeLimit()) {
        state_ = State::TimeUp;
        stateTime_ = 0.0f;
        player_.stop();
        events |= kSessionTimeUp;
    }

    effects_.update(dt);
    return events;
}

void LevelSession::updateEnemies(float dt, unsigned& events) {
    const RectF playerBox = player_.hitbox();
    for (Enemy& e : enemies_) {
        e.update(dt * rules_.enemySpeed, level_, player_.position());
        if (!e.alive() || state_ != State::Playing) continue;

        // Mushroom shockwave: hop over it!
        if (e.ringHits(player_.position()) && player_.height() < 4.0f) hurt(e.position(), events);

        if (!playerBox.intersects(e.hitbox())) continue;
        const bool stomp = player_.isFalling() && player_.height() < kStompHeight;
        if (stomp || player_.isDashing() || crushesEnemies()) {
            e.defeat();
            ++stats_.enemiesDefeated;
            events |= kSessionStomp;
            effects_.spawn(Effects::Type::HitStars, e.position() - Vec2{0.0f, 14.0f}, kGold);
            effects_.spawn(Effects::Type::Dust, e.position(), kDustColor);
            if (stomp) player_.bounce();
        } else if (e.dangerous() && player_.height() < kClearHeight) {
            hurt(e.position(), events);
        }
    }
}

bool LevelSession::debugDefeatBoss() {
    if (!level_.hasBoss || !boss_.fighting()) return false;
    boss_.debugDefeat();
    return true;
}

void LevelSession::setArenaGates(bool closed) {
    for (const auto& [x, y] : level_.bossGates) level_.setTile(x, y, closed ? Tile::Tree : Tile::Ground);
}

void LevelSession::updateBoss(float dt, unsigned& events) {
    const Vec2 feet = player_.position();
    // Walking well into the arena (clear of the gates) wakes the boss.
    if (boss_.sleeping() && state_ == State::Playing) {
        constexpr float kInset = 24.0f;
        const RectF& a = level_.bossArena;
        if (feet.x > a.x + kInset && feet.x < a.right() - kInset && feet.y > a.y + kInset &&
            feet.y < a.bottom() - kInset / 2.0f) {
            boss_.wake(true);
            setArenaGates(true);
            events |= kSessionBossIntro;
        }
    }

    const bool wasDefeated = boss_.defeated();
    boss_.update(dt * rules_.enemySpeed, feet);
    if (!wasDefeated && boss_.defeated()) {
        setArenaGates(false);
        stats_.bossDefeated = true;
        events |= kSessionBossDefeated;
        for (int i = 0; i < 6; ++i)
            effects_.spawn(Effects::Type::Sparkle, boss_.position() + Vec2{(i - 2.5f) * 14.0f, -30.0f - (i % 3) * 12.0f},
                           i % 2 ? kGold : kWhite);
    }
    if (state_ != State::Playing || !boss_.fighting()) return;

    if (boss_.landedThisStep()) {
        for (int i = 0; i < 4; ++i)
            effects_.spawn(Effects::Type::Dust, boss_.position() + Vec2{(i - 1.5f) * 18.0f, 0.0f}, kDustColor);
        if ((feet - boss_.position()).length() < Boss::kCrushRadius) hurt(boss_.position(), events);
    }

    // Shockwave rings: on the ground, the ring passing under your feet hurts.
    for (Boss::Ring& ring : boss_.rings()) {
        if (!ring.active || ring.delay > 0.0f || ring.hitPlayer) continue;
        const float d = (feet - ring.center).length();
        if (std::fabs(d - ring.radius) < Boss::kRingThickness * 0.5f + 4.0f && player_.height() < 4.0f) {
            ring.hitPlayer = true;
            hurt(ring.center, events);
        }
    }

    // Body contact.
    if (boss_.height() > 16.0f) return; // in the air: the landing shadow is the danger
    if ((feet - boss_.position()).length() > Boss::kBodyRadius + 8.0f) return;
    const bool stomp = player_.isFalling() && player_.height() < kStompHeight;
    if (boss_.vulnerable() && (stomp || player_.isDashing() || crushesEnemies())) {
        boss_.hit(feet);
        events |= kSessionBossHit;
        effects_.spawn(Effects::Type::HitStars, boss_.position() - Vec2{0.0f, 70.0f}, kGold);
        effects_.spawn(Effects::Type::Leaves, boss_.position() - Vec2{0.0f, 60.0f}, kLeaf);
        effects_.spawn(Effects::Type::Sparkle, boss_.position() - Vec2{12.0f, 40.0f}, kWhite);
        if (stomp) player_.bounce();
    } else if (stomp) {
        player_.bounce(); // boing: its mossy head is springy, but only hurts it when dizzy
    } else if (boss_.hurtsOnContact() && player_.height() < kClearHeight) {
        hurt(boss_.position(), events);
    }
}

void LevelSession::updateHazards(unsigned& events) {
    const RectF box = player_.hitbox();
    const bool grounded = !player_.isAirborne();

    // Remember where the player last stood safely, for water rescues (the
    // whole hitbox must be clear so the rescue spot is never on the brink).
    if (grounded && !player_.isDashing() && !level_.overlapsTile(box, Tile::Water) &&
        !level_.overlapsTile(box, Tile::Hazard))
        lastSafe_ = player_.position();

    // Water: only the feet matter, so you can brush past the edge, and a
    // dash skims across.
    const Vec2 feet = player_.position();
    if (grounded && !player_.isDashing() && !onRaft() && level_.tileAtPixel(feet.x, feet.y) == Tile::Water) {
        effects_.spawn(Effects::Type::Splash, feet, kWater);
        ++stats_.splashes;
        events |= kSessionSplash;
        // A fall during invincibility (or Rainbow Star) is free, but doesn't
        // extend it. A shield bubble pops instead of a heart being lost.
        const bool costsHeart = !player_.isInvincible() && !invulnerable();
        if (costsHeart && powers_.active(PowerUpType::ShieldBubble)) {
            powers_.consume(PowerUpType::ShieldBubble);
            events |= kSessionShieldPop;
            effects_.spawn(Effects::Type::Sparkle, bodyCenter(player_), kShieldBlue);
            player_.respawnAt(lastSafe_, true);
            return;
        }
        if (costsHeart) {
            --hearts_;
            ++stats_.heartsLost;
            events |= kSessionHurt;
        }
        if (hearts_ <= 0)
            knockOut(events);
        else
            player_.respawnAt(lastSafe_, costsHeart);
        return;
    }

    // Thorns: like water, only the feet count. Brushing past is safe and a
    // hop over a one-tile patch has a generous timing window. Knock the
    // player back the way they came.
    if (player_.height() < 4.0f && level_.tileAtPixel(feet.x, feet.y) == Tile::Hazard) {
        Vec2 forward = player_.velocity().normalized();
        if (forward.lengthSq() == 0.0f) forward = facingVector(player_.facing());
        hurt(player_.position() + forward * 16.0f, events);
    }
}

void LevelSession::updatePickups(float dt, unsigned& events) {
    if (powers_.active(PowerUpType::Magnet)) coins_.attract(bodyCenter(player_), kMagnetRadius, dt);
    if (const int n = coins_.collect(bodyCenter(player_), &effects_); n > 0) {
        stats_.coinPoints += n * (powers_.active(PowerUpType::DoubleCoins) ? 2 : 1);
        events |= kSessionCoin;
    }

    ItemField::Pickup got[4];
    const int count = items_.collect(bodyCenter(player_), powers_.stored() == PowerUpType::None, &effects_, got, 4);
    for (int i = 0; i < count; ++i) {
        switch (got[i].kind) {
        case ItemField::Kind::Star: events |= kSessionStar; break;
        case ItemField::Kind::Gem: events |= kSessionGem; break;
        case ItemField::Kind::PowerUp:
            powers_.store(got[i].power);
            events |= kSessionPowerUpGet;
            break;
        case ItemField::Kind::Key:
            ++keysHeld_;
            events |= kSessionKey;
            break;
        }
    }

    if (heartPickups_.collect(bodyCenter(player_), hearts_ < rules_.maxHearts, &effects_)) {
        ++hearts_;
        events |= kSessionHeart;
    }

    for (size_t i = 0; i < checkpoints_.size(); ++i) {
        Checkpoint& cp = checkpoints_[i];
        if (static_cast<int>(i) == activeCheckpoint_) continue;
        if ((cp.pos - player_.position()).lengthSq() > kCheckpointRadius * kCheckpointRadius) continue;
        cp.reached = true;
        activeCheckpoint_ = static_cast<int>(i);
        hearts_ = rules_.maxHearts; // a checkpoint is also a rest stop
        events |= kSessionCheckpoint;
        for (int k = 0; k < 2; ++k)
            effects_.spawn(Effects::Type::Sparkle, cp.pos + Vec2{(k ? 8.0f : -8.0f), -20.0f - k * 8.0f}, kMint);
    }
}

void LevelSession::hurt(Vec2 source, unsigned& events) {
    if (invulnerable()) return;
    if (powers_.active(PowerUpType::ShieldBubble) && !player_.isInvincible()) {
        // The bubble pops: knockback and invincibility, but no heart lost.
        powers_.consume(PowerUpType::ShieldBubble);
        player_.takeHit(source);
        events |= kSessionShieldPop;
        effects_.spawn(Effects::Type::Sparkle, bodyCenter(player_), kShieldBlue);
        return;
    }
    if (!player_.takeHit(source)) return; // still invincible
    --hearts_;
    ++stats_.heartsLost;
    events |= kSessionHurt;
    if (hearts_ <= 0) knockOut(events);
}

void LevelSession::knockOut(unsigned& events) {
    hearts_ = 0;
    state_ = State::KnockedOut;
    stateTime_ = 0.0f;
    ++stats_.knockOuts;
    player_.stop();
    events |= kSessionKnockedOut;
}

PlayerModifiers LevelSession::modifiersFromPowers() const {
    PlayerModifiers m;
    if (powers_.active(PowerUpType::SpeedShoes)) m.speedScale = 1.5f;
    if (powers_.active(PowerUpType::SuperDash)) {
        m.dashTimeScale = 2.0f;
        m.dashCooldownScale = 0.5f;
    }
    if (powers_.active(PowerUpType::TinyMode)) {
        m.size = 0.5f;
        m.visualScale = 0.6f;
        m.pass |= kPassTinyGaps;
    }
    if (powers_.active(PowerUpType::GiantMode)) {
        m.size = 1.45f;
        m.visualScale = 1.6f;
    }
    m.rainbow = powers_.active(PowerUpType::RainbowStar);
    return m;
}

void LevelSession::updatePowerUps(bool usePressed, float dt, unsigned& events) {
    const bool wasTiny = powers_.active(PowerUpType::TinyMode);
    bool anyBefore = false;
    for (int i = 1; i < kPowerUpCount; ++i) {
        const auto t = static_cast<PowerUpType>(i);
        if (t != PowerUpType::ShieldBubble && powers_.active(t)) anyBefore = true;
    }

    powers_.update(dt);

    // Never shrink back to normal size inside a tiny gap: Tiny Mode keeps
    // going until the hero has walked out.
    if (wasTiny && !powers_.active(PowerUpType::TinyMode)) {
        PlayerModifiers normal = modifiersFromPowers();
        if (level_.overlapsSolid(player_.hitboxWith(normal))) powers_.extend(PowerUpType::TinyMode, 0.25f);
    }

    bool anyAfter = false;
    for (int i = 1; i < kPowerUpCount; ++i) {
        const auto t = static_cast<PowerUpType>(i);
        if (t != PowerUpType::ShieldBubble && powers_.active(t)) anyAfter = true;
    }
    if (anyBefore && !anyAfter) events |= kSessionPowerUpEnd;

    if (usePressed && powers_.stored() != PowerUpType::None && tryActivate(powers_.stored(), events))
        powers_.store(PowerUpType::None);

    player_.setModifiers(modifiersFromPowers());
}

bool LevelSession::tryActivate(PowerUpType type, unsigned& events) {
    if (type == PowerUpType::GiantMode) {
        // Growing must not trap the hero in a wall: find a free spot nearby
        // for the bigger body, or refuse (the power-up stays in the slot).
        PlayerModifiers giant = modifiersFromPowers();
        giant.size = 1.45f;
        giant.pass = kPassNone;
        const Vec2 home = player_.position();
        bool placed = false;
        for (int r = 0; r <= 12 && !placed; r += 2) {
            for (int dy = -r; dy <= r && !placed; dy += 2) {
                for (int dx = -r; dx <= r && !placed; dx += 2) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue; // ring only
                    player_.setPosition(home + Vec2{static_cast<float>(dx), static_cast<float>(dy)});
                    const RectF box = player_.hitboxWith(giant);
                    placed = !level_.overlapsSolid(box) && !level_.overlapsTile(box, Tile::Water);
                }
            }
        }
        if (!placed) {
            player_.setPosition(home);
            events |= kSessionNoRoom;
            return false;
        }
    }
    powers_.activate(type);
    ++stats_.powerUpsUsed;
    events |= kSessionPowerUpUse;
    effects_.spawn(Effects::Type::Sparkle, bodyCenter(player_), powerUpInfo(type).color);
    return true;
}

void LevelSession::smashBlocks(unsigned& events) {
    const bool giant = powers_.active(PowerUpType::GiantMode);
    if (!giant && !player_.isDashing()) return;
    // Look just beyond the hitbox: a block we bumped into this step is smashed
    // and the path is clear on the next one.
    RectF reach = player_.hitbox();
    reach.x -= 3.0f;
    reach.y -= 3.0f;
    reach.w += 6.0f;
    reach.h += 6.0f;
    const int x0 = static_cast<int>(std::floor(reach.left() / kTileSize));
    const int x1 = static_cast<int>(std::floor((reach.right() - 0.001f) / kTileSize));
    const int y0 = static_cast<int>(std::floor(reach.top() / kTileSize));
    const int y1 = static_cast<int>(std::floor((reach.bottom() - 0.001f) / kTileSize));
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            if (!Level::isBreakable(level_.tileAt(tx, ty), giant)) continue;
            level_.setTile(tx, ty, Tile::Ground);
            ++stats_.blocksBroken;
            events |= kSessionBlockBroken;
            const Vec2 c = Level::tileCenter(tx, ty);
            effects_.spawn(Effects::Type::Dust, c + Vec2{0.0f, 8.0f}, kDustColor);
            effects_.spawn(Effects::Type::Chips, c + Vec2{0.0f, 8.0f}, kChips);
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 5: objectives, rafts, keys, friends, signs, secrets
// ---------------------------------------------------------------------------

int LevelSession::friendsRescued() const {
    int n = 0;
    for (const Friend& f : friends_)
        if (f.rescued) ++n;
    return n;
}

bool LevelSession::objectiveComplete() const {
    int have = 0;
    int need = 0;
    objectiveProgress(have, need);
    return have >= need;
}

void LevelSession::objectiveProgress(int& have, int& need) const {
    switch (level_.objective) {
    case Objective::ReachExit: have = need = 0; return;
    case Objective::Coins: have = coins_.collected(); need = level_.goal; return;
    case Objective::Stars: have = items_.starsCollected(); need = items_.starsTotal(); return;
    case Objective::Rescue: have = friendsRescued(); need = static_cast<int>(friends_.size()); return;
    case Objective::DefeatAll:
        need = static_cast<int>(enemies_.size());
        have = need - enemiesAlive();
        return;
    case Objective::Boss:
        need = Boss::kMaxHealth;
        have = level_.hasBoss ? boss_.hitsTaken() : need;
        return;
    }
}

float LevelSession::timeLimit() const {
    if (level_.timeLimit <= 0.0f) return 0.0f;
    return difficulty_ == Difficulty::Relaxed ? level_.timeLimit * 1.5f : level_.timeLimit;
}

float LevelSession::timeLeft() const {
    const float limit = timeLimit();
    return limit > 0.0f ? std::max(0.0f, limit - time_) : 0.0f;
}

bool LevelSession::onRaft() const {
    // Forgiving: feet within 10 px of a raft count as on it, so stepping off
    // just as it turns around never drops you in the water.
    const Vec2 feet = player_.position();
    for (const Raft& r : rafts_) {
        RectF safe = r.rect();
        safe.x -= 10.0f;
        safe.y -= 10.0f;
        safe.w += 20.0f;
        safe.h += 20.0f;
        if (safe.contains(feet)) return true;
    }
    return false;
}

void LevelSession::updateRafts(float dt) {
    const Vec2 feet = player_.position();
    for (Raft& r : rafts_) {
        // Turn around once the raft's centre reaches the last water tile.
        // (Checking its leading edge instead stopped it short of the bank,
        // leaving a gap of water exactly where you step aboard.) Being
        // 40 px wide, it then overlaps each bank by a few pixels.
        const Vec2 lead = r.pos + r.dir * (static_cast<float>(kTileSize) * 0.5f);
        if (level_.tileAtPixel(lead.x, lead.y) != Tile::Water) r.dir = -r.dir;
        const Vec2 delta = r.dir * (kRaftSpeed * dt);
        // A hero standing on the raft (or hopping above it) rides along;
        // walls still stop them.
        const bool riding = r.rect().contains(feet) && player_.height() < 40.0f;
        r.pos += delta;
        if (riding) {
            const Vec2 moved = moveAndCollide(level_, player_.hitbox(), delta, 0.0f, nullptr, player_.modifiers().pass);
            player_.setPosition(player_.position() + moved);
        }
    }
}

void LevelSession::updateLocksAndSecrets(unsigned& events) {
    // Gates: bump into one while holding a key and the whole gate opens.
    RectF reach = player_.hitbox();
    reach.x -= 4.0f;
    reach.y -= 4.0f;
    reach.w += 8.0f;
    reach.h += 8.0f;
    if (level_.overlapsTile(reach, Tile::Lock)) {
        if (keysHeld_ > 0) {
            const int x0 = static_cast<int>(std::floor(reach.left() / kTileSize));
            const int y0 = static_cast<int>(std::floor(reach.top() / kTileSize));
            const int x1 = static_cast<int>(std::floor((reach.right() - 0.001f) / kTileSize));
            const int y1 = static_cast<int>(std::floor((reach.bottom() - 0.001f) / kTileSize));
            std::vector<std::pair<int, int>> stack;
            for (int ty = y0; ty <= y1; ++ty)
                for (int tx = x0; tx <= x1; ++tx)
                    if (level_.tileAt(tx, ty) == Tile::Lock) stack.push_back({tx, ty});
            // Flood-fill the connected gate so a wide gate opens in one go.
            while (!stack.empty()) {
                const auto [tx, ty] = stack.back();
                stack.pop_back();
                if (level_.tileAt(tx, ty) != Tile::Lock) continue;
                level_.setTile(tx, ty, Tile::Ground);
                effects_.spawn(Effects::Type::Sparkle, Level::tileCenter(tx, ty), kGold);
                const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : dirs) stack.push_back({tx + d[0], ty + d[1]});
            }
            --keysHeld_;
            events |= kSessionGateOpened;
        } else if (!lockHintLatch_) {
            events |= kSessionLocked;
        }
        lockHintLatch_ = true;
    } else {
        lockHintLatch_ = false;
    }

    // Secrets: the first step into a hidden passage.
    const Vec2 feet = player_.position();
    const int group = level_.secretAt(static_cast<int>(std::floor(feet.x / kTileSize)),
                                      static_cast<int>(std::floor(feet.y / kTileSize)));
    if (group >= 0 && !secretFound_[static_cast<size_t>(group)]) {
        secretFound_[static_cast<size_t>(group)] = true;
        ++secretsFound_;
        events |= kSessionSecret;
        effects_.spawn(Effects::Type::Sparkle, bodyCenter(player_), kMint);
        effects_.spawn(Effects::Type::Leaves, feet, kLeaf);
    }
}

int LevelSession::nearestFriend() const {
    int best = -1;
    float bestD = kInteractRadius * kInteractRadius;
    for (size_t i = 0; i < friends_.size(); ++i) {
        if (friends_[i].rescued) continue;
        const float d = (friends_[i].pos - player_.position()).lengthSq();
        if (d <= bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int LevelSession::nearestSign() const {
    int best = -1;
    float bestD = kInteractRadius * kInteractRadius;
    for (size_t i = 0; i < level_.signs.size(); ++i) {
        const float d = (level_.signs[i].pos - player_.position()).lengthSq();
        if (d <= bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

LevelSession::InteractKind LevelSession::interactTarget(Vec2* where) const {
    if (const int f = nearestFriend(); f >= 0) {
        if (where) *where = friends_[static_cast<size_t>(f)].pos;
        return InteractKind::Friend;
    }
    if (const int s = nearestSign(); s >= 0) {
        if (where) *where = level_.signs[static_cast<size_t>(s)].pos;
        return InteractKind::Sign;
    }
    return InteractKind::None;
}

void LevelSession::interact(unsigned& events) {
    // Friends first: rescuing is the more important action.
    if (const int f = nearestFriend(); f >= 0) {
        Friend& fr = friends_[static_cast<size_t>(f)];
        fr.rescued = true;
        fr.rescueTime = 0.0f;
        events |= kSessionRescue;
        effects_.spawn(Effects::Type::Sparkle, fr.pos - Vec2{0.0f, 12.0f}, SDL_Color{255, 150, 200, 255});
        effects_.spawn(Effects::Type::Sparkle, fr.pos - Vec2{0.0f, 24.0f}, kGold);
        return;
    }
    if (const int s = nearestSign(); s >= 0) {
        readingSign_ = s;
        events |= kSessionSign;
    }
}

bool LevelSession::warpNear(Vec2 target) {
    const int tx = static_cast<int>(target.x) / kTileSize;
    const int ty = static_cast<int>(target.y) / kTileSize;
    // The target tile itself first, then nearby tiles (below first, which
    // suits the exit flag alcoves).
    const int offsets[][2] = {{0, 0}, {0, 2}, {0, 1}, {-2, 0}, {2, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, -2}};
    for (const auto& o : offsets) {
        Player probe(Level::tileCenter(tx + o[0], ty + o[1]) + Vec2{0.0f, 6.0f});
        const RectF box = probe.hitbox();
        if (level_.overlapsSolid(box) || level_.overlapsTile(box, Tile::Exit) || level_.overlapsTile(box, Tile::Water) ||
            level_.overlapsTile(box, Tile::Hazard))
            continue;
        player_.respawnAt(probe.position(), false);
        lastSafe_ = probe.position();
        return true;
    }
    return false;
}

} // namespace pd
