#pragma once

#include <algorithm>
#include <cmath>

namespace pd {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec2 normalized() const {
        const float len = length();
        return len > 1e-6f ? Vec2{x / len, y / len} : Vec2{};
    }
};

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left() const { return x; }
    float right() const { return x + w; }
    float top() const { return y; }
    float bottom() const { return y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    bool intersects(const RectF& o) const {
        return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
    }
    bool contains(Vec2 p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
};

inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }

// Moves `value` towards `target` by at most `maxDelta`.
inline float approach(float value, float target, float maxDelta) {
    if (value < target) return std::min(value + maxDelta, target);
    return std::max(value - maxDelta, target);
}

// Moves vector `v` towards `target` by at most `maxDelta` (Euclidean length).
inline Vec2 approach(Vec2 v, Vec2 target, float maxDelta) {
    const Vec2 d = target - v;
    const float len = d.length();
    if (len <= maxDelta || len < 1e-6f) return target;
    return v + d * (maxDelta / len);
}

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

} // namespace pd
