#pragma once

#include "Math.h"

#include <SDL.h>

#include <array>
#include <cstdint>

namespace pd {

// Small, fixed-size pool of short-lived visual effects. Nothing is
// allocated after construction: spawning when the pool is full recycles the
// oldest effect. Each effect is a little burst of particles whose motion is
// a pure function of its age (and a per-effect random seed), so nothing
// needs simulating and the pool costs almost nothing to update.
class Effects {
public:
    enum class Type {
        Sparkle,  // soft glowing burst (pickups, checkpoints)
        Dust,     // puff of dust (landing, dashing)
        Splash,   // water droplets
        HitStars, // little stars flying out (stomps, boss hits)
        Leaves,   // leaves fluttering down (secret hedges)
        Chips,    // wood or stone chips thrown up (smashed blocks)
    };

    void spawn(Type type, Vec2 pos, SDL_Color color);
    void update(float dt);
    void render(SDL_Renderer* r, Vec2 camera) const;
    void clear();
    int activeCount() const;

    static float lifetime(Type type);

private:
    struct Effect {
        Type type = Type::Sparkle;
        Vec2 pos;
        SDL_Color color{255, 255, 255, 255};
        float age = 0.0f;
        float life = 0.0f; // 0 = inactive
        uint32_t seed = 0;
    };
    static constexpr int kMaxEffects = 96;
    std::array<Effect, kMaxEffects> pool_{};
    int next_ = 0;
    uint32_t seed_ = 1;
};

} // namespace pd
