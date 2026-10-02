#include "Effects.h"

#include "Draw.h"

#include <cmath>

namespace pd {

void Effects::spawn(Type type, Vec2 pos, SDL_Color color) {
    Effect& e = pool_[next_];
    next_ = (next_ + 1) % kMaxEffects;
    e.type = type;
    e.pos = pos;
    e.color = color;
    e.age = 0.0f;
    e.life = type == Type::Sparkle ? 0.35f : type == Type::Splash ? 0.5f : 0.3f;
}

void Effects::update(float dt) {
    for (Effect& e : pool_) {
        if (e.life <= 0.0f) continue;
        e.age += dt;
        if (e.age >= e.life) e.life = 0.0f;
    }
}

void Effects::render(SDL_Renderer* r, Vec2 camera) const {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (const Effect& e : pool_) {
        if (e.life <= 0.0f) continue;
        const float t = e.age / e.life; // 0..1
        const int cx = static_cast<int>(std::lround(e.pos.x - camera.x));
        const int cy = static_cast<int>(std::lround(e.pos.y - camera.y));
        SDL_Color c = e.color;
        c.a = static_cast<Uint8>(255.0f * (1.0f - t));

        if (e.type == Type::Sparkle) {
            // Six chunky pixels bursting outwards, plus a shrinking centre flash.
            const float radius = 4.0f + 18.0f * t;
            const int size = t < 0.5f ? 4 : 2;
            for (int i = 0; i < 6; ++i) {
                const float ang = static_cast<float>(i) * 1.0472f + 0.5f; // 60 degrees apart
                const int x = cx + static_cast<int>(std::cos(ang) * radius);
                const int y = cy + static_cast<int>(std::sin(ang) * radius);
                draw::fillRect(r, x - size / 2, y - size / 2, size, size, c);
            }
            if (t < 0.4f) {
                const int s = static_cast<int>(10.0f * (1.0f - t / 0.4f));
                draw::fillRect(r, cx - s / 2, cy - s / 2, s, s, SDL_Color{255, 255, 255, c.a});
            }
        } else if (e.type == Type::Splash) {
            // Droplets thrown up and out, falling back down.
            for (int i = 0; i < 8; ++i) {
                const float ang = static_cast<float>(i) * 0.785f;
                const float dist = 6.0f + 20.0f * t;
                const float lift = 26.0f * t * (1.0f - t) * 4.0f * (0.6f + 0.4f * static_cast<float>(i % 2));
                const int x = cx + static_cast<int>(std::cos(ang) * dist);
                const int y = cy + static_cast<int>(std::sin(ang) * dist * 0.5f - lift);
                draw::fillRect(r, x - 2, y - 2, 4, 4, c);
            }
        } else {
            // Dust: a soft puff that grows and fades.
            const int radius = 3 + static_cast<int>(7.0f * t);
            draw::fillCircle(r, cx, cy - static_cast<int>(4.0f * t), radius, c);
        }
    }
}

void Effects::clear() {
    for (Effect& e : pool_) e.life = 0.0f;
}

int Effects::activeCount() const {
    int n = 0;
    for (const Effect& e : pool_)
        if (e.life > 0.0f) ++n;
    return n;
}

} // namespace pd
