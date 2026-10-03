#include "Effects.h"

#include "Draw.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

// 0..1 pseudo-random value for particle `i` of an effect.
float rand01(uint32_t seed, int i) {
    uint32_t h = seed * 2654435761u + static_cast<uint32_t>(i) * 2246822519u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

SDL_Color withAlpha(SDL_Color c, float a) {
    c.a = static_cast<Uint8>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}

} // namespace

float Effects::lifetime(Type type) {
    switch (type) {
    case Type::Sparkle: return 0.45f;
    case Type::Dust: return 0.4f;
    case Type::Splash: return 0.55f;
    case Type::HitStars: return 0.5f;
    case Type::Leaves: return 1.1f;
    case Type::Chips: return 0.7f;
    }
    return 0.4f;
}

void Effects::spawn(Type type, Vec2 pos, SDL_Color color) {
    Effect& e = pool_[static_cast<size_t>(next_)];
    next_ = (next_ + 1) % kMaxEffects;
    e.type = type;
    e.pos = pos;
    e.color = color;
    e.age = 0.0f;
    e.life = lifetime(type);
    e.seed = seed_++;
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
        const float cx = e.pos.x - camera.x;
        const float cy = e.pos.y - camera.y;
        if (cx < -60.0f || cy < -80.0f || cx > 700.0f || cy > 540.0f) continue;
        const float fade = 1.0f - t;
        auto at = [&](float x, float y) { return std::make_pair(static_cast<int>(std::lround(cx + x)), static_cast<int>(std::lround(cy + y))); };

        switch (e.type) {
        case Type::Sparkle: {
            // Soft glow in the middle and glints flying outwards.
            if (t < 0.5f) {
                const int g = static_cast<int>(16.0f * (1.0f - t * 2.0f)) + 4;
                draw::softEllipse(r, static_cast<int>(cx), static_cast<int>(cy), g, g, withAlpha(e.color, 0.9f));
            }
            for (int i = 0; i < 7; ++i) {
                const float ang = 6.2832f * (static_cast<float>(i) + rand01(e.seed, i) * 0.6f) / 7.0f;
                const float dist = (6.0f + 22.0f * rand01(e.seed, i + 10)) * std::sqrt(t);
                const auto [x, y] = at(std::cos(ang) * dist, std::sin(ang) * dist - 6.0f * t);
                const int size = std::max(1, static_cast<int>(3.0f * fade + 0.5f));
                draw::fillCircle(r, x, y, size, withAlpha(SDL_Color{255, 255, 255, 255}, fade));
                draw::softEllipse(r, x, y, size * 3, size * 3, withAlpha(e.color, fade * 0.7f));
            }
            break;
        }
        case Type::Dust: {
            // A few soft puffs drifting apart and up.
            for (int i = 0; i < 3; ++i) {
                const float dx = (static_cast<float>(i) - 1.0f) * (6.0f + 10.0f * t);
                const int rad = static_cast<int>(4.0f + 8.0f * t + rand01(e.seed, i) * 3.0f);
                const auto [x, y] = at(dx, -5.0f * t - rand01(e.seed, i + 3) * 3.0f);
                draw::softEllipse(r, x, y, rad, rad * 3 / 4 + 1, withAlpha(e.color, fade * 0.8f));
            }
            break;
        }
        case Type::Splash: {
            // Droplets thrown up and out, falling back down, and a ripple.
            const int ripple = static_cast<int>(6.0f + 22.0f * t);
            draw::softEllipse(r, static_cast<int>(cx), static_cast<int>(cy), ripple, ripple / 3 + 1,
                              withAlpha(SDL_Color{230, 245, 255, 255}, fade * 0.5f));
            for (int i = 0; i < 9; ++i) {
                const float ang = 6.2832f * static_cast<float>(i) / 9.0f + rand01(e.seed, i);
                const float dist = (6.0f + 18.0f * rand01(e.seed, i + 20)) * t;
                const float lift = 4.0f * t * (1.0f - t) * (18.0f + 14.0f * rand01(e.seed, i + 40));
                const auto [x, y] = at(std::cos(ang) * dist, std::sin(ang) * dist * 0.5f - lift);
                draw::fillCircle(r, x, y, 2, withAlpha(e.color, fade));
                draw::fillCircle(r, x - 1, y - 1, 1, withAlpha(SDL_Color{255, 255, 255, 255}, fade));
            }
            break;
        }
        case Type::HitStars: {
            // Five little stars bursting out in an arc, spinning as they go.
            for (int i = 0; i < 5; ++i) {
                const float ang = -2.7f + 2.3f * static_cast<float>(i) / 4.0f;
                const float dist = 30.0f * std::sqrt(t) + 4.0f;
                const auto [x, y] = at(std::cos(ang) * dist, std::sin(ang) * dist + 20.0f * t * t);
                const float spin = t * 9.0f + static_cast<float>(i);
                const int arm = static_cast<int>(5.0f * fade) + 2;
                const SDL_Color c = withAlpha(e.color, std::min(1.0f, fade * 1.5f));
                draw::softEllipse(r, x, y, arm * 2, arm * 2, withAlpha(e.color, fade * 0.5f));
                for (int k = 0; k < 2; ++k) {
                    const float a = spin + static_cast<float>(k) * 1.5708f;
                    const int ax = static_cast<int>(std::cos(a) * arm), ay = static_cast<int>(std::sin(a) * arm);
                    draw::fillCircle(r, x + ax, y + ay, 1, c);
                    draw::fillCircle(r, x - ax, y - ay, 1, c);
                }
                draw::fillCircle(r, x, y, 2, withAlpha(SDL_Color{255, 255, 255, 255}, fade));
            }
            break;
        }
        case Type::Leaves: {
            // Leaves pop up, then flutter down swaying side to side.
            for (int i = 0; i < 7; ++i) {
                const float vx = (rand01(e.seed, i) - 0.5f) * 60.0f;
                const float up = 40.0f + 30.0f * rand01(e.seed, i + 7);
                const float time = e.age;
                const float y = -up * time + 45.0f * time * time;
                const float sway = std::sin(time * 7.0f + static_cast<float>(i)) * 6.0f;
                const auto [x, yy] = at(vx * time + sway, y - 8.0f);
                const float flip = std::fabs(std::cos(time * 6.0f + static_cast<float>(i) * 1.3f));
                const int w = std::max(1, static_cast<int>(5.0f * flip));
                const SDL_Color leaf = i % 2 ? e.color : SDL_Color{static_cast<Uint8>(e.color.r * 3 / 4), static_cast<Uint8>(e.color.g * 3 / 4), static_cast<Uint8>(e.color.b * 3 / 4), e.color.a};
                draw::fillEllipse(r, x, yy, w, 3, withAlpha(leaf, std::min(1.0f, fade * 2.0f)));
            }
            break;
        }
        case Type::Chips: {
            // Chunks thrown up with gravity, bouncing to a stop.
            for (int i = 0; i < 8; ++i) {
                const float vx = (rand01(e.seed, i) - 0.5f) * 120.0f;
                const float vy = -90.0f - 70.0f * rand01(e.seed, i + 8);
                const float time = e.age;
                const float y = std::min(6.0f, vy * time + 380.0f * time * time);
                const auto [x, yy] = at(vx * time, y - 6.0f);
                const int s = 2 + static_cast<int>(rand01(e.seed, i + 16) * 3.0f);
                draw::roundedRect(r, SDL_Rect{x - s, yy - s / 2, s * 2, s + 1}, withAlpha(e.color, std::min(1.0f, fade * 2.0f)),
                                  SDL_Color{0, 0, 0, 0});
            }
            break;
        }
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
