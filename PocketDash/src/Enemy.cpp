#include "Enemy.h"

#include "Constants.h"
#include "Draw.h"
#include "Level.h"
#include "Sprites.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

constexpr float kPi = 3.14159265f;

// Slime
constexpr float kSlimeWait = 1.1f;
constexpr float kSlimeSquash = 0.3f; // telegraph before every hop
constexpr float kSlimeHop = 0.45f;
constexpr float kSlimeHopSpeed = 80.0f;
constexpr float kSlimeChaseRange = 6.0f * kTileSize;

// Beetle
constexpr float kBeetleSpeed = 55.0f;
constexpr float kBeetlePatrol = 3.0f * kTileSize; // distance from home before turning
constexpr float kBeetleTurnPause = 0.35f;

// Mushroom
constexpr float kMushroomIdle = 2.2f;
constexpr float kMushroomTelegraph = 0.6f;
constexpr float kMushroomBigBounce = 0.5f;
constexpr float kRingSpeed = 110.0f;

constexpr float kDeathTime = 0.35f;

const Vec2 kWanderDirs[4] = {{1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {0.0f, -1.0f}};

bool blocksEnemy(Tile t) { return Level::isSolid(t) || Level::isDanger(t) || t == Tile::SecretWall || t == Tile::Exit; }

} // namespace

Enemy::Enemy(const EnemySpawn& spawn) : type_(spawn.type), pos_(spawn.pos), home_(spawn.pos), dir_(spawn.dir) {
    // Stagger timers by position so groups of enemies don't move in lockstep.
    const float stagger = std::fmod(std::fabs(spawn.pos.x * 0.013f + spawn.pos.y * 0.007f), 1.0f);
    timer_ = stagger;
    wanderIndex_ = static_cast<int>(spawn.pos.x + spawn.pos.y) % 4;
}

RectF Enemy::hitbox() const {
    // Ground-plane box at the feet, a little smaller than the sprite: fair to the player.
    return RectF{pos_.x - 11.0f, pos_.y - 12.0f, 22.0f, 14.0f};
}

bool Enemy::tryMove(const Level& level, Vec2 delta) {
    // Moves one axis at a time; refuses any step that touches a tile enemies avoid.
    bool blocked = false;
    for (int axis = 0; axis < 2; ++axis) {
        const float d = axis == 0 ? delta.x : delta.y;
        if (d == 0.0f) continue;
        RectF box = hitbox();
        (axis == 0 ? box.x : box.y) += d;
        const int x0 = static_cast<int>(std::floor(box.left() / kTileSize));
        const int x1 = static_cast<int>(std::floor((box.right() - 0.001f) / kTileSize));
        const int y0 = static_cast<int>(std::floor(box.top() / kTileSize));
        const int y1 = static_cast<int>(std::floor((box.bottom() - 0.001f) / kTileSize));
        bool ok = true;
        for (int ty = y0; ty <= y1 && ok; ++ty)
            for (int tx = x0; tx <= x1 && ok; ++tx)
                if (blocksEnemy(level.tileAt(tx, ty))) ok = false;
        if (ok)
            (axis == 0 ? pos_.x : pos_.y) += d;
        else
            blocked = true;
    }
    return !blocked;
}

void Enemy::update(float dt, const Level& level, Vec2 playerPos) {
    if (!alive_) {
        if (deathTimer_ > 0.0f) deathTimer_ -= dt;
        return;
    }
    timer_ += dt;
    animTimer_ += dt;
    switch (type_) {
    case EnemyType::Slime: updateSlime(dt, level, playerPos); break;
    case EnemyType::Beetle: updateBeetle(dt, level); break;
    case EnemyType::Mushroom: updateMushroom(dt); break;
    default: break; // later worlds
    }
}

void Enemy::updateSlime(float dt, const Level& level, Vec2 playerPos) {
    switch (phase_) {
    case 0: // resting
        if (timer_ >= kSlimeWait) {
            phase_ = 1;
            timer_ = 0.0f;
        }
        break;
    case 1: // squish: the telegraph
        if (timer_ >= kSlimeSquash) {
            const Vec2 toPlayer = playerPos - pos_;
            Vec2 dir;
            if (toPlayer.length() < kSlimeChaseRange) {
                dir = toPlayer.normalized();
            } else {
                dir = kWanderDirs[wanderIndex_];
                wanderIndex_ = (wanderIndex_ + 1) % 4;
            }
            vel_ = dir * kSlimeHopSpeed;
            phase_ = 2;
            timer_ = 0.0f;
        }
        break;
    case 2: // hop
        if (!tryMove(level, vel_ * dt)) vel_ = {};
        z_ = std::sin(kPi * std::min(1.0f, timer_ / kSlimeHop)) * 12.0f;
        if (timer_ >= kSlimeHop) {
            z_ = 0.0f;
            vel_ = {};
            phase_ = 0;
            timer_ = 0.0f;
        }
        break;
    default: break;
    }
}

void Enemy::updateBeetle(float dt, const Level& level) {
    if (phase_ == 1) { // pausing to turn around
        if (timer_ >= kBeetleTurnPause) {
            dir_ = -dir_;
            phase_ = 0;
            timer_ = 0.0f;
        }
        return;
    }
    const bool moved = tryMove(level, dir_ * (kBeetleSpeed * dt));
    if (!moved || dot(pos_ - home_, dir_) > kBeetlePatrol) {
        phase_ = 1;
        timer_ = 0.0f;
    }
}

void Enemy::updateMushroom(float dt) {
    if (ringRadius_ > 0.0f) {
        ringRadius_ += kRingSpeed * dt;
        if (ringRadius_ > kRingMaxRadius) ringRadius_ = 0.0f;
    }
    switch (phase_) {
    case 0: // happy little bounces
        z_ = std::fabs(std::sin(timer_ * 5.0f)) * 4.0f;
        if (timer_ >= kMushroomIdle) {
            phase_ = 1;
            timer_ = 0.0f;
            z_ = 0.0f;
        }
        break;
    case 1: // shiver: the telegraph
        if (timer_ >= kMushroomTelegraph) {
            phase_ = 2;
            timer_ = 0.0f;
        }
        break;
    case 2: // big bounce, shockwave on landing
        z_ = std::sin(kPi * std::min(1.0f, timer_ / kMushroomBigBounce)) * 26.0f;
        if (timer_ >= kMushroomBigBounce) {
            z_ = 0.0f;
            ringRadius_ = 6.0f;
            phase_ = 0;
            timer_ = 0.0f;
        }
        break;
    default: break;
    }
}

void Enemy::defeat() {
    if (!alive_) return;
    alive_ = false;
    deathTimer_ = kDeathTime;
    ringRadius_ = 0.0f;
    z_ = 0.0f;
}

bool Enemy::ringHits(Vec2 p) const {
    if (ringRadius_ <= 0.0f) return false;
    return std::fabs((p - pos_).length() - ringRadius_) < kRingThickness * 0.5f;
}

void Enemy::render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera) const {
    if (!visible()) return;
    const int sx = static_cast<int>(std::lround(pos_.x - camera.x));
    const int sy = static_cast<int>(std::lround(pos_.y - camera.y));
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Shockwave ring: a circle of chunky pixels that fades as it grows.
    if (ringRadius_ > 0.0f) {
        const float t = ringRadius_ / kRingMaxRadius;
        const SDL_Color c{255, 240, 160, static_cast<Uint8>(230 * (1.0f - t * 0.7f))};
        const int dots = 28;
        for (int i = 0; i < dots; ++i) {
            const float a = static_cast<float>(i) * (2.0f * kPi / dots);
            const int x = sx + static_cast<int>(std::cos(a) * ringRadius_);
            const int y = sy + static_cast<int>(std::sin(a) * ringRadius_ * 0.6f); // flattened: it's on the ground
            draw::fillRect(r, x - 2, y - 2, 5, 4, c);
        }
    }

    const int rx = std::max(5, 11 - static_cast<int>(z_ * 0.2f));
    draw::fillEllipse(r, sx, sy + 2, rx, 4, SDL_Color{0, 0, 0, 80});
    if (!sheet) return;

    int row = 0;
    int frame = 0;
    int w = Sprites::kEnemyFrame;
    int h = Sprites::kEnemyFrame;
    int offsetX = 0;
    double angle = 0.0;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    switch (type_) {
    case EnemyType::Slime:
        row = Sprites::kRowSlime;
        frame = (alive_ && phase_ == 1) ? 1 : 0; // squashed while telegraphing
        break;
    case EnemyType::Beetle:
        row = Sprites::kRowBeetle;
        frame = (alive_ && phase_ == 0 && static_cast<int>(animTimer_ / 0.15f) % 2) ? 1 : 0;
        if (dir_.x < 0) flip = SDL_FLIP_HORIZONTAL;
        else if (dir_.y > 0) angle = 90.0;
        else if (dir_.y < 0) angle = -90.0;
        break;
    case EnemyType::Mushroom:
        row = Sprites::kRowMushroom;
        if (alive_ && phase_ == 1) { // shiver and squash before the big bounce
            offsetX = (static_cast<int>(timer_ * 30.0f) % 2) ? 2 : -2;
            w += 6;
            h -= 6;
        }
        break;
    default: break;
    }

    Uint8 alpha = 255;
    if (!alive_) { // squish flat and fade
        const float t = std::max(0.0f, deathTimer_ / kDeathTime);
        h = std::max(2, static_cast<int>(h * t));
        w += static_cast<int>(10 * (1.0f - t));
        alpha = static_cast<Uint8>(255 * t);
    }

    const SDL_Rect src{frame * Sprites::kEnemyFrame, row * Sprites::kEnemyFrame, Sprites::kEnemyFrame,
                       Sprites::kEnemyFrame};
    const SDL_Rect dst{sx - w / 2 + offsetX, sy + 3 - h - static_cast<int>(std::lround(z_)), w, h};
    SDL_SetTextureAlphaMod(sheet, alpha);
    SDL_RenderCopyEx(r, sheet, &src, &dst, angle, nullptr, flip);
    SDL_SetTextureAlphaMod(sheet, 255);
}

const char* enemyTypeName(EnemyType type) {
    switch (type) {
    case EnemyType::Slime: return "SLIME";
    case EnemyType::Beetle: return "BEETLE";
    case EnemyType::Mushroom: return "MUSHROOM";
    case EnemyType::Cloud: return "CLOUD";
    case EnemyType::Robot: return "ROBOT";
    case EnemyType::RollingRock: return "ROLLING ROCK";
    case EnemyType::SpinFlower: return "SPIN FLOWER";
    case EnemyType::Count: break;
    }
    return "UNKNOWN";
}

} // namespace pd
