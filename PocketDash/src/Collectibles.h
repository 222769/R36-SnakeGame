#pragma once

#include "Math.h"

#include <SDL.h>

#include <vector>

namespace pd {

class Effects;

// Coins placed in a level. Phase 4 adds stars, gems and hearts here.
class CoinField {
public:
    // Pickup radius around a coin centre, generous so kids don't miss them.
    static constexpr float kPickupRadius = 18.0f;

    void reset(const std::vector<Vec2>& positions);

    // Collects every coin within reach of `playerCenter`. Spawns a sparkle
    // per coin and returns how many were collected this step.
    int collect(Vec2 playerCenter, Effects* effects);

    void render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera, float time) const;

    int collected() const { return collected_; }
    int total() const { return static_cast<int>(coins_.size()); }

private:
    struct Coin {
        Vec2 pos;
        bool taken = false;
    };
    std::vector<Coin> coins_;
    int collected_ = 0;
};

// Heart pickups: restore one heart. They stay put while the player is at
// full health so they are still there when needed.
class HeartPickups {
public:
    static constexpr float kPickupRadius = 18.0f;

    void reset(const std::vector<Vec2>& positions);
    // Returns true if a heart was picked up (only when `canHeal`).
    bool collect(Vec2 playerCenter, bool canHeal, Effects* effects);
    void render(SDL_Renderer* r, SDL_Texture* heart, Vec2 camera, float time) const;
    int remaining() const;

private:
    struct Pickup {
        Vec2 pos;
        bool taken = false;
    };
    std::vector<Pickup> pickups_;
};

} // namespace pd
