#pragma once

#include "Math.h"

#include <SDL.h>

#include <array>

namespace pd {

// Small, fixed-size pool of short-lived visual effects (sparkles, dust).
// Nothing is allocated after construction: spawning when the pool is full
// simply recycles the oldest effect. Phase 8 builds the full particle
// system on top of this.
class Effects {
public:
    enum class Type { Sparkle, Dust };

    void spawn(Type type, Vec2 pos, SDL_Color color);
    void update(float dt);
    void render(SDL_Renderer* r, Vec2 camera) const;
    void clear();
    int activeCount() const;

private:
    struct Effect {
        Type type = Type::Sparkle;
        Vec2 pos;
        SDL_Color color{255, 255, 255, 255};
        float age = 0.0f;
        float life = 0.0f; // 0 = inactive
    };
    static constexpr int kMaxEffects = 48;
    std::array<Effect, kMaxEffects> pool_{};
    int next_ = 0;
};

} // namespace pd
