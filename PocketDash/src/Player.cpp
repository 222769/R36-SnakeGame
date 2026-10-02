#include "Player.h"

#include "Constants.h"
#include "Draw.h"
#include "Level.h"
#include "Sprites.h"

#include <cmath>

namespace pd {

Vec2 facingVector(Facing f) {
    switch (f) {
    case Facing::Up: return {0.0f, -1.0f};
    case Facing::Down: return {0.0f, 1.0f};
    case Facing::Left: return {-1.0f, 0.0f};
    case Facing::Right: return {1.0f, 0.0f};
    }
    return {0.0f, 1.0f};
}

Player::Player(Vec2 spawn) : pos_(spawn) {}

void Player::updateFacing(Vec2 dir) {
    const float ax = std::fabs(dir.x);
    const float ay = std::fabs(dir.y);
    if (ax < 0.2f && ay < 0.2f) return;
    // On a near-perfect diagonal keep the current facing if it is one of the
    // two components; this stops the sprite flickering between frames.
    if (std::fabs(ax - ay) < 0.15f) {
        const bool horizontalOk = (facing_ == Facing::Left && dir.x < 0) || (facing_ == Facing::Right && dir.x > 0);
        const bool verticalOk = (facing_ == Facing::Up && dir.y < 0) || (facing_ == Facing::Down && dir.y > 0);
        if (horizontalOk || verticalOk) return;
    }
    if (ax >= ay)
        facing_ = dir.x < 0 ? Facing::Left : Facing::Right;
    else
        facing_ = dir.y < 0 ? Facing::Up : Facing::Down;
}

unsigned Player::update(const PlayerInput& input, float dt, const Level* level) {
    unsigned events = kEventNone;

    Vec2 move = input.move;
    if (move.lengthSq() > 1.0f) move = move.normalized();

    if (invincibleTimer_ > 0.0f) invincibleTimer_ -= dt;
    if (stunTimer_ > 0.0f) stunTimer_ -= dt;
    if (dashCooldown_ > 0.0f) dashCooldown_ -= dt;
    if (squashTimer_ > 0.0f) squashTimer_ -= dt;

    // --- Horizontal (ground plane) movement --------------------------------
    if (dashTimer_ > 0.0f) {
        dashTimer_ -= dt;
        vel_ = dashDir_ * tuning_.dashSpeed;
        if (dashTimer_ <= 0.0f) {
            // Exit the dash at walking speed so it flows straight into running.
            vel_ = dashDir_ * tuning_.maxSpeed;
            dashCooldown_ = tuning_.dashCooldown;
        }
    } else if (stunTimer_ > 0.0f) {
        vel_ = approach(vel_, Vec2{}, tuning_.friction * 0.5f * dt);
    } else if (input.dashPressed && dashCooldown_ <= 0.0f) {
        // Dash in the stick direction, or straight ahead when idle.
        dashDir_ = move.lengthSq() > 0.04f ? move.normalized() : facingVector(facing_);
        dashTimer_ = tuning_.dashTime;
        vel_ = dashDir_ * tuning_.dashSpeed;
        updateFacing(dashDir_);
        events |= kEventDashed;
    } else {
        const Vec2 target = move * tuning_.maxSpeed;
        float rate = move.lengthSq() > 1e-4f ? tuning_.acceleration : tuning_.friction;
        // Turning around should feel instant: brake and accelerate together.
        if (dot(vel_, target) < 0.0f) rate += tuning_.friction;
        vel_ = approach(vel_, target, rate * dt);
        updateFacing(move);
    }

    if (level) {
        CollisionResult hit;
        pos_ += moveAndCollide(*level, hitbox(), vel_ * dt, kCornerNudge, &hit);
        if (hit.hitX) {
            vel_.x = 0.0f;
            dashDir_.x = 0.0f;
        }
        if (hit.hitY) {
            vel_.y = 0.0f;
            dashDir_.y = 0.0f;
        }
    } else {
        pos_ += vel_ * dt;
    }

    // --- Hop (height axis) -------------------------------------------------
    if (input.hopPressed && !isAirborne() && stunTimer_ <= 0.0f) {
        vz_ = tuning_.hopVelocity;
        events |= kEventHopped;
    }
    if (isAirborne()) {
        vz_ -= tuning_.gravity * dt;
        z_ += vz_ * dt;
        if (z_ <= 0.0f) {
            z_ = 0.0f;
            vz_ = 0.0f;
            squashTimer_ = 0.08f;
            events |= kEventLanded;
        }
    }

    // --- Walk animation ----------------------------------------------------
    const float speed = vel_.length();
    if (speed > 12.0f && !isAirborne()) {
        // Faster movement = faster steps.
        animTimer_ += dt * std::max(0.5f, speed / tuning_.maxSpeed);
        if (animTimer_ >= 0.12f) {
            animTimer_ -= 0.12f;
            animFrame_ ^= 1;
        }
    } else {
        animTimer_ = 0.0f;
        animFrame_ = isAirborne() ? 1 : 0;
    }

    return events;
}

bool Player::takeHit(Vec2 source) {
    if (isInvincible()) return false;
    Vec2 away = (pos_ - source).normalized();
    if (away.lengthSq() < 1e-6f) away = facingVector(facing_) * -1.0f;
    vel_ = away * tuning_.knockbackSpeed;
    dashTimer_ = 0.0f;
    stunTimer_ = tuning_.stunTime;
    invincibleTimer_ = tuning_.invincibleTime;
    return true;
}

void Player::bounce() {
    vz_ = tuning_.hopVelocity * 0.85f;
    if (z_ <= 0.0f) z_ = 0.01f;
}

void Player::respawnAt(Vec2 pos, bool freshInvincibility) {
    pos_ = pos;
    vel_ = {};
    z_ = 0.0f;
    vz_ = 0.0f;
    dashTimer_ = 0.0f;
    dashCooldown_ = 0.0f;
    stunTimer_ = 0.0f;
    if (freshInvincibility) invincibleTimer_ = tuning_.invincibleTime;
    facing_ = Facing::Down;
}

RectF Player::hitbox() const {
    // Feet-level box: top-down games feel fairest when only the lower body collides.
    return RectF{pos_.x - kHitboxW * 0.5f, pos_.y - kHitboxH + 2.0f, kHitboxW, kHitboxH};
}

void Player::constrainTo(const RectF& area) {
    const RectF box = hitbox();
    if (box.left() < area.left()) {
        pos_.x += area.left() - box.left();
        if (vel_.x < 0) vel_.x = 0;
    } else if (box.right() > area.right()) {
        pos_.x -= box.right() - area.right();
        if (vel_.x > 0) vel_.x = 0;
    }
    if (box.top() < area.top()) {
        pos_.y += area.top() - box.top();
        if (vel_.y < 0) vel_.y = 0;
    } else if (box.bottom() > area.bottom()) {
        pos_.y -= box.bottom() - area.bottom();
        if (vel_.y > 0) vel_.y = 0;
    }
}

void Player::render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera) const {
    const int sx = static_cast<int>(std::lround(pos_.x - camera.x));
    const int sy = static_cast<int>(std::lround(pos_.y - camera.y));

    // Shadow shrinks as the player rises so hop height is easy to read.
    const float lift = std::min(1.0f, z_ / 30.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw::fillEllipse(r, sx, sy + 2, static_cast<int>(11 - 4 * lift), static_cast<int>(4 - 1 * lift),
                      SDL_Color{0, 0, 0, static_cast<Uint8>(90 - 40 * lift)});

    if (!sheet) return;

    // Blink while invincible.
    if (isInvincible() && !isStunned() && static_cast<int>(invincibleTimer_ * 16.0f) % 2 == 0) return;

    int row = Sprites::kRowFront;
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    switch (facing_) {
    case Facing::Down: row = Sprites::kRowFront; break;
    case Facing::Up: row = Sprites::kRowBack; break;
    case Facing::Right: row = Sprites::kRowSide; break;
    case Facing::Left: row = Sprites::kRowSide; flip = SDL_FLIP_HORIZONTAL; break;
    }
    const SDL_Rect src{animFrame_ * Sprites::kPlayerFrameW, row * Sprites::kPlayerFrameH, Sprites::kPlayerFrameW,
                       Sprites::kPlayerFrameH};

    int w = Sprites::kPlayerFrameW * kPixelScale;
    int h = Sprites::kPlayerFrameH * kPixelScale;
    if (squashTimer_ > 0.0f) { // landing squash
        w += 6;
        h -= 6;
    } else if (isAirborne() && vz_ > 0.0f) { // take-off stretch
        w -= 4;
        h += 4;
    }
    const int bob = (animFrame_ == 1 && !isAirborne()) ? -kPixelScale : 0;
    const int baseY = sy + 3 - static_cast<int>(std::lround(z_)) + bob;

    // Dash afterimages: two faded copies trailing behind.
    if (isDashing()) {
        for (int i = 2; i >= 1; --i) {
            const Vec2 back = dashDir_ * (-10.0f * static_cast<float>(i));
            const SDL_Rect ghost{sx + static_cast<int>(back.x) - w / 2, baseY + static_cast<int>(back.y) - h, w, h};
            SDL_SetTextureAlphaMod(sheet, static_cast<Uint8>(120 / (i + 1)));
            SDL_RenderCopyEx(r, sheet, &src, &ghost, 0.0, nullptr, flip);
        }
        SDL_SetTextureAlphaMod(sheet, 255);
    }

    const SDL_Rect dst{sx - w / 2, baseY - h, w, h};
    SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr, flip);
}

} // namespace pd
