#include "LevelSession.h"

#include "Constants.h"

namespace pd {

namespace {

constexpr SDL_Color kDustColor{245, 240, 225, 200};
constexpr SDL_Color kGold{255, 214, 64, 255};
constexpr SDL_Color kWhite{255, 255, 255, 255};
constexpr SDL_Color kWater{170, 220, 255, 255};
constexpr SDL_Color kMint{120, 230, 200, 255};

// How high the player must be to pass over enemies and thorns unharmed.
constexpr float kClearHeight = 8.0f;
// A stomp only counts near the end of a hop (feet close to the enemy's head).
constexpr float kStompHeight = 16.0f;

// Body centre, used for pickups (the position is the feet).
Vec2 bodyCenter(const Player& p) { return p.position() - Vec2{0.0f, 10.0f}; }

} // namespace

LevelSession::LevelSession(const Level& level, Difficulty difficulty)
    : level_(level), difficulty_(difficulty), rules_(difficultyRules(difficulty)) {
    restart();
}

void LevelSession::restart() {
    player_ = Player(level_.spawn);
    coins_.reset(level_.coins);
    heartPickups_.reset(level_.hearts);

    enemies_.clear();
    enemies_.reserve(level_.enemies.size());
    for (const EnemySpawn& s : level_.enemies) enemies_.emplace_back(s);

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

    updateEnemies(dt, events);
    if (state_ == State::Playing) updateHazards(events);
    if (state_ == State::Playing) updatePickups(events);

    if (state_ == State::Playing && level_.overlapsTile(player_.hitbox(), Tile::Exit)) {
        state_ = State::Cleared;
        stateTime_ = 0.0f;
        player_.stop();
        events |= kSessionCleared;
        for (int i = 0; i < 3; ++i)
            effects_.spawn(Effects::Type::Sparkle, level_.exit + Vec2{(i - 1) * 14.0f, -10.0f - i * 6.0f}, kGold);
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
        if (stomp || player_.isDashing()) {
            e.defeat();
            ++stats_.enemiesDefeated;
            events |= kSessionStomp;
            effects_.spawn(Effects::Type::Sparkle, e.position() - Vec2{0.0f, 10.0f}, kWhite);
            effects_.spawn(Effects::Type::Dust, e.position(), kDustColor);
            if (stomp) player_.bounce();
        } else if (e.dangerous() && player_.height() < kClearHeight) {
            hurt(e.position(), events);
        }
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
    if (grounded && !player_.isDashing() && level_.tileAtPixel(feet.x, feet.y) == Tile::Water) {
        effects_.spawn(Effects::Type::Splash, feet, kWater);
        ++stats_.splashes;
        events |= kSessionSplash;
        // A fall during invincibility is free, but doesn't extend it.
        const bool costsHeart = !player_.isInvincible();
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

void LevelSession::updatePickups(unsigned& events) {
    if (coins_.collect(bodyCenter(player_), &effects_) > 0) events |= kSessionCoin;

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
