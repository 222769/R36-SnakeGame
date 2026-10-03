#pragma once

#include "EntityTypes.h"
#include "Math.h"
#include "PowerUp.h"

#include <SDL.h>

#include <vector>

namespace pd {

class Effects;

// Coins placed in a level.
class CoinField {
public:
    // Pickup radius around a coin centre, generous so kids don't miss them.
    static constexpr float kPickupRadius = 18.0f;

    void reset(const std::vector<Vec2>& positions);

    // Collects every coin within reach of `playerCenter`. Spawns a sparkle
    // per coin and returns how many were collected this step.
    int collect(Vec2 playerCenter, Effects* effects);

    // Magnet: coins within `radius` fly towards `target`, faster when close.
    void attract(Vec2 target, float radius, float dt);

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

// Stars (three hidden per level), gems (rare) and power-up bubbles.
class ItemField {
public:
    enum class Kind { Star, Gem, PowerUp, Key };
    static constexpr float kPickupRadius = 18.0f;

    struct Pickup {
        Kind kind = Kind::Star;
        PowerUpType power = PowerUpType::None;
        int index = 0; // star number within the level (0..2)
    };

    void reset(const std::vector<Vec2>& stars, const std::vector<Vec2>& gems,
               const std::vector<PowerUpSpawn>& powerUps, const std::vector<Vec2>& keys = {});

    // Collects items within reach. Power-ups are only taken when
    // `canTakePowerUp` (the slot is empty); otherwise they wait on the ground.
    // Writes up to `maxOut` pickups to `out` and returns how many.
    int collect(Vec2 playerCenter, bool canTakePowerUp, Effects* effects, Pickup* out, int maxOut);

    void render(SDL_Renderer* r, SDL_Texture* items, Vec2 camera, float time) const;

    int starsTotal() const { return starsTotal_; }
    int starsCollected() const { return starsCollected_; }
    bool starCollected(int index) const;
    int gemsTotal() const { return gemsTotal_; }
    int gemsCollected() const { return gemsCollected_; }

private:
    struct Item {
        Kind kind;
        PowerUpType power;
        Vec2 pos;
        int index;
        bool taken;
    };
    std::vector<Item> items_;
    int starsTotal_ = 0;
    int starsCollected_ = 0;
    int gemsTotal_ = 0;
    int gemsCollected_ = 0;
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
