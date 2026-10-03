#include "Collectibles.h"

#include "Constants.h"
#include "Draw.h"
#include "Effects.h"
#include "Sprites.h"

#include <algorithm>
#include <cmath>

namespace pd {

void CoinField::reset(const std::vector<Vec2>& positions) {
    coins_.clear();
    coins_.reserve(positions.size());
    for (Vec2 p : positions) coins_.push_back({p, false});
    collected_ = 0;
}

int CoinField::collect(Vec2 playerCenter, Effects* effects) {
    int count = 0;
    const float r2 = kPickupRadius * kPickupRadius;
    for (Coin& c : coins_) {
        if (c.taken || (c.pos - playerCenter).lengthSq() > r2) continue;
        c.taken = true;
        ++count;
        if (effects) effects->spawn(Effects::Type::Sparkle, c.pos, SDL_Color{255, 220, 80, 255});
    }
    collected_ += count;
    return count;
}

void CoinField::render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera, float time) const {
    if (!sheet) return;
    constexpr int size = Sprites::kCoinSize * kPixelScale;
    // Spin sequence 0,1,2,1(mirrored).
    static const int kFrames[4] = {0, 1, 2, 1};
    const float viewLeft = camera.x - size;
    const float viewRight = camera.x + kScreenWidth + size;
    const float viewTop = camera.y - size;
    const float viewBottom = camera.y + kScreenHeight + size;

    for (size_t i = 0; i < coins_.size(); ++i) {
        const Coin& c = coins_[i];
        if (c.taken) continue;
        if (c.pos.x < viewLeft || c.pos.x > viewRight || c.pos.y < viewTop || c.pos.y > viewBottom) continue;

        // Each coin is out of phase with its neighbours so rows don't spin in lockstep.
        const float phase = time * 8.0f + static_cast<float>(i) * 0.7f;
        const int step = static_cast<int>(phase) % 4;
        const int bob = static_cast<int>(std::lround(std::sin(time * 3.0f + static_cast<float>(i)) * 2.0f));
        const int sx = static_cast<int>(c.pos.x - camera.x);
        const int sy = static_cast<int>(c.pos.y - camera.y);

        draw::fillEllipse(r, sx, sy + 12, 7, 2, SDL_Color{0, 0, 0, 60});
        const SDL_Rect src{kFrames[step] * Sprites::kCoinSize, 0, Sprites::kCoinSize, Sprites::kCoinSize};
        const SDL_Rect dst{sx - size / 2, sy - size / 2 + bob - 2, size, size};
        SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr, step == 3 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

void CoinField::attract(Vec2 target, float radius, float dt) {
    for (Coin& c : coins_) {
        if (c.taken) continue;
        const Vec2 to = target - c.pos;
        const float dist = to.length();
        if (dist > radius || dist < 0.5f) continue;
        // Speeds up as it gets closer, so coins visibly "snap" in.
        const float speed = 160.0f + 420.0f * (1.0f - dist / radius);
        c.pos += to * (std::min(dist, speed * dt) / dist);
    }
}

// ---------------------------------------------------------------------------

void ItemField::reset(const std::vector<Vec2>& stars, const std::vector<Vec2>& gems,
                      const std::vector<PowerUpSpawn>& powerUps) {
    items_.clear();
    items_.reserve(stars.size() + gems.size() + powerUps.size());
    for (size_t i = 0; i < stars.size(); ++i)
        items_.push_back({Kind::Star, PowerUpType::None, stars[i], static_cast<int>(i), false});
    for (Vec2 g : gems) items_.push_back({Kind::Gem, PowerUpType::None, g, 0, false});
    for (const PowerUpSpawn& p : powerUps) items_.push_back({Kind::PowerUp, p.type, p.pos, 0, false});
    starsTotal_ = static_cast<int>(stars.size());
    gemsTotal_ = static_cast<int>(gems.size());
    starsCollected_ = 0;
    gemsCollected_ = 0;
}

int ItemField::collect(Vec2 playerCenter, bool canTakePowerUp, Effects* effects, Pickup* out, int maxOut) {
    int n = 0;
    for (Item& it : items_) {
        if (n >= maxOut) break;
        if (it.taken || (it.pos - playerCenter).lengthSq() > kPickupRadius * kPickupRadius) continue;
        if (it.kind == Kind::PowerUp) {
            if (!canTakePowerUp) continue;
            canTakePowerUp = false; // one slot
        }
        it.taken = true;
        if (it.kind == Kind::Star) ++starsCollected_;
        if (it.kind == Kind::Gem) ++gemsCollected_;
        out[n++] = Pickup{it.kind, it.power, it.index};
        if (effects) {
            const SDL_Color c = it.kind == Kind::Star  ? SDL_Color{255, 230, 90, 255}
                                : it.kind == Kind::Gem ? SDL_Color{120, 240, 230, 255}
                                                       : powerUpInfo(it.power).color;
            effects->spawn(Effects::Type::Sparkle, it.pos, c);
            if (it.kind != Kind::PowerUp) effects->spawn(Effects::Type::Sparkle, it.pos - Vec2{0.0f, 10.0f}, c);
        }
    }
    return n;
}

bool ItemField::starCollected(int index) const {
    for (const Item& it : items_)
        if (it.kind == Kind::Star && it.index == index) return it.taken;
    return false;
}

void ItemField::render(SDL_Renderer* r, SDL_Texture* items, Vec2 camera, float time) const {
    if (!items) return;
    constexpr int size = Sprites::kItemSize * kPixelScale;
    for (size_t i = 0; i < items_.size(); ++i) {
        const Item& it = items_[i];
        if (it.taken) continue;
        const int sx = static_cast<int>(it.pos.x - camera.x);
        const int sy = static_cast<int>(it.pos.y - camera.y);
        if (sx < -40 || sy < -40 || sx > kScreenWidth + 40 || sy > kScreenHeight + 40) continue;
        const float phase = time * 3.0f + static_cast<float>(i);
        const int bob = static_cast<int>(std::lround(std::sin(phase) * 3.0f));
        draw::fillEllipse(r, sx, sy + 13, 8, 2, SDL_Color{0, 0, 0, 60});

        int frame = Sprites::kItemStar;
        if (it.kind == Kind::Gem) frame = Sprites::kItemGem;
        if (it.kind == Kind::PowerUp) {
            frame = Sprites::itemFrame(it.power);
            // Translucent bubble in the power-up's colour, with a shine.
            const SDL_Color c = powerUpInfo(it.power).color;
            draw::fillCircle(r, sx, sy + bob - 2, 15, SDL_Color{c.r, c.g, c.b, 110});
            draw::fillCircle(r, sx - 6, sy + bob - 9, 3, SDL_Color{255, 255, 255, 170});
        } else if (it.kind == Kind::Star) {
            // Stars twinkle: a little glint orbits them.
            const float a = time * 4.0f + static_cast<float>(i);
            draw::fillRect(r, sx + static_cast<int>(std::cos(a) * 16.0f) - 1,
                           sy + bob - 2 + static_cast<int>(std::sin(a) * 9.0f) - 1, 3, 3,
                           SDL_Color{255, 255, 255, 220});
        }
        const SDL_Rect src{frame * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
        const SDL_Rect dst{sx - size / 2, sy - size / 2 + bob - 2, size, size};
        const bool rainbow = it.kind == Kind::PowerUp && it.power == PowerUpType::RainbowStar;
        if (rainbow) {
            const float t = time * 6.0f;
            SDL_SetTextureColorMod(items, static_cast<Uint8>(150 + 105 * (0.5f + 0.5f * std::sin(t))),
                                   static_cast<Uint8>(150 + 105 * (0.5f + 0.5f * std::sin(t + 2.094f))),
                                   static_cast<Uint8>(150 + 105 * (0.5f + 0.5f * std::sin(t + 4.189f))));
        }
        SDL_RenderCopy(r, items, &src, &dst);
        if (rainbow) SDL_SetTextureColorMod(items, 255, 255, 255);
    }
}

// ---------------------------------------------------------------------------

void HeartPickups::reset(const std::vector<Vec2>& positions) {
    pickups_.clear();
    pickups_.reserve(positions.size());
    for (Vec2 p : positions) pickups_.push_back({p, false});
}

bool HeartPickups::collect(Vec2 playerCenter, bool canHeal, Effects* effects) {
    if (!canHeal) return false;
    for (Pickup& p : pickups_) {
        if (p.taken || (p.pos - playerCenter).lengthSq() > kPickupRadius * kPickupRadius) continue;
        p.taken = true;
        if (effects) effects->spawn(Effects::Type::Sparkle, p.pos, SDL_Color{255, 110, 140, 255});
        return true; // one heart per step is plenty
    }
    return false;
}

void HeartPickups::render(SDL_Renderer* r, SDL_Texture* heart, Vec2 camera, float time) const {
    if (!heart) return;
    for (size_t i = 0; i < pickups_.size(); ++i) {
        const Pickup& p = pickups_[i];
        if (p.taken) continue;
        const int sx = static_cast<int>(p.pos.x - camera.x);
        const int sy = static_cast<int>(p.pos.y - camera.y);
        if (sx < -32 || sy < -32 || sx > kScreenWidth + 32 || sy > kScreenHeight + 32) continue;
        // Gentle "heartbeat" pulse.
        const float beat = std::fabs(std::sin(time * 3.0f + static_cast<float>(i)));
        const int w = Sprites::kHeartW * 2 + static_cast<int>(beat * 4.0f);
        const int h = Sprites::kHeartH * 2 + static_cast<int>(beat * 4.0f);
        draw::fillEllipse(r, sx, sy + 12, 7, 2, SDL_Color{0, 0, 0, 60});
        const SDL_Rect dst{sx - w / 2, sy - h / 2 - 2, w, h};
        SDL_RenderCopy(r, heart, nullptr, &dst);
    }
}

int HeartPickups::remaining() const {
    int n = 0;
    for (const Pickup& p : pickups_)
        if (!p.taken) ++n;
    return n;
}

} // namespace pd
