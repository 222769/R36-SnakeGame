#include "Canvas.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

uint32_t hash3(int x, int y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float lattice(int x, int y, int period, uint32_t seed) {
    const int px = ((x % period) + period) % period;
    const int py = ((y % period) + period) % period;
    return static_cast<float>(hash3(px, py, seed) & 0xffff) / 65535.0f;
}

float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace

float tileNoise(float x, float y, int period, uint32_t seed) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float fx = smooth(x - static_cast<float>(xi));
    const float fy = smooth(y - static_cast<float>(yi));
    const float a = lattice(xi, yi, period, seed);
    const float b = lattice(xi + 1, yi, period, seed);
    const float c = lattice(xi, yi + 1, period, seed);
    const float d = lattice(xi + 1, yi + 1, period, seed);
    return (a + (b - a) * fx) * (1.0f - fy) + (c + (d - c) * fx) * fy;
}

float tileFbm(float x, float y, int period, uint32_t seed, int octaves) {
    // Each octave doubles frequency; the lattice period doubles with it so
    // the result still repeats every `period` source pixels.
    float sum = 0.0f;
    float amp = 0.5f;
    float norm = 0.0f;
    int cells = std::max(1, period / 8); // base octave: 8 px cells
    for (int o = 0; o < octaves; ++o) {
        const float scale = static_cast<float>(cells) / static_cast<float>(period);
        sum += amp * tileNoise(x * scale, y * scale, cells, seed + static_cast<uint32_t>(o) * 101u);
        norm += amp;
        amp *= 0.5f;
        cells *= 2;
    }
    return sum / norm;
}

Canvas::Canvas(int width, int height)
    : w_(width), h_(height), px_(static_cast<size_t>(width * height), Col{0, 0, 0, 0}) {}

void Canvas::clear(Col c) { std::fill(px_.begin(), px_.end(), c); }

Col Canvas::get(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return {0, 0, 0, 0};
    return px_[static_cast<size_t>(y * w_ + x)];
}

void Canvas::blend(int x, int y, Col c, float coverage) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    const float a = clamp01(c.a * coverage);
    if (a <= 0.0f) return;
    Col& d = px_[static_cast<size_t>(y * w_ + x)];
    // Standard "source over" with straight alpha.
    const float outA = a + d.a * (1.0f - a);
    if (outA <= 0.0f) return;
    d.r = (c.r * a + d.r * d.a * (1.0f - a)) / outA;
    d.g = (c.g * a + d.g * d.a * (1.0f - a)) / outA;
    d.b = (c.b * a + d.b * d.a * (1.0f - a)) / outA;
    d.a = outA;
}

template <class Sdf>
void Canvas::fillSdf(float x0, float y0, float x1, float y1, Sdf sdf, Col c) {
    const int ix0 = std::max(0, static_cast<int>(std::floor(x0)) - 1);
    const int iy0 = std::max(0, static_cast<int>(std::floor(y0)) - 1);
    const int ix1 = std::min(w_ - 1, static_cast<int>(std::ceil(x1)) + 1);
    const int iy1 = std::min(h_ - 1, static_cast<int>(std::ceil(y1)) + 1);
    for (int y = iy0; y <= iy1; ++y) {
        for (int x = ix0; x <= ix1; ++x) {
            // Distance from the pixel centre to the edge; 1 px of feathering.
            const float d = sdf(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            const float cov = clamp01(0.5f - d);
            if (cov > 0.0f) blend(x, y, c, cov);
        }
    }
}

void Canvas::fillCircle(float cx, float cy, float r, Col c) { fillEllipse(cx, cy, r, r, c); }

void Canvas::fillEllipse(float cx, float cy, float rx, float ry, Col c) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    const float k = std::min(rx, ry);
    fillSdf(cx - rx, cy - ry, cx + rx, cy + ry,
            [&](float x, float y) {
                const float nx = (x - cx) / rx;
                const float ny = (y - cy) / ry;
                return (std::sqrt(nx * nx + ny * ny) - 1.0f) * k;
            },
            c);
}

void Canvas::fillRoundRect(float x, float y, float w, float h, float radius, Col c) {
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float hx = w * 0.5f - r;
    const float hy = h * 0.5f - r;
    fillSdf(x, y, x + w, y + h,
            [&](float px, float py) {
                const float qx = std::fabs(px - cx) - hx;
                const float qy = std::fabs(py - cy) - hy;
                const float ox = std::max(qx, 0.0f);
                const float oy = std::max(qy, 0.0f);
                return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
            },
            c);
}

void Canvas::strokeRoundRect(float x, float y, float w, float h, float radius, float width, Col c) {
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float hx = w * 0.5f - r;
    const float hy = h * 0.5f - r;
    fillSdf(x - width, y - width, x + w + width, y + h + width,
            [&](float px, float py) {
                const float qx = std::fabs(px - cx) - hx;
                const float qy = std::fabs(py - cy) - hy;
                const float ox = std::max(qx, 0.0f);
                const float oy = std::max(qy, 0.0f);
                const float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
                return std::fabs(d + width * 0.5f) - width * 0.5f; // band just inside the edge
            },
            c);
}

void Canvas::line(Vec2 a, Vec2 b, float width, Col c) {
    const float r = width * 0.5f;
    const Vec2 ab = b - a;
    const float len2 = std::max(1e-6f, ab.lengthSq());
    fillSdf(std::min(a.x, b.x) - r, std::min(a.y, b.y) - r, std::max(a.x, b.x) + r, std::max(a.y, b.y) + r,
            [&](float x, float y) {
                const Vec2 p{x - a.x, y - a.y};
                const float t = clamp01(dot(p, ab) / len2);
                return (p - ab * t).length() - r;
            },
            c);
}

void Canvas::ring(float cx, float cy, float r0, float r1, Col c) {
    const float mid = (r0 + r1) * 0.5f;
    const float half = (r1 - r0) * 0.5f;
    fillSdf(cx - r1, cy - r1, cx + r1, cy + r1,
            [&](float x, float y) {
                const float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                return std::fabs(d - mid) - half;
            },
            c);
}

void Canvas::fillPolygon(const std::vector<Vec2>& pts, Col c) {
    if (pts.size() < 3) return;
    float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for (const Vec2& p : pts) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    auto inside = [&](float x, float y) {
        bool in = false;
        for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
            const Vec2& a = pts[i];
            const Vec2& b = pts[j];
            if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
        }
        return in;
    };
    const int x0 = std::max(0, static_cast<int>(std::floor(minX)));
    const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(maxX)));
    const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
    const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(maxY)));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int hits = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx)
                    if (inside(static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / 4.0f,
                               static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / 4.0f))
                        ++hits;
            if (hits) blend(x, y, c, static_cast<float>(hits) / 16.0f);
        }
    }
}

void Canvas::sphere(float cx, float cy, float rx, float ry, Col base, float gloss, float ambient) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    // Light from the upper left, towards the viewer.
    const float lx = -0.45f, ly = -0.6f, lz = 0.66f;
    const float hx = lx, hy = ly, hz = lz + 1.0f; // half vector (unnormalised)
    const float hl = std::sqrt(hx * hx + hy * hy + hz * hz);
    const float k = std::min(rx, ry);
    const int ix0 = std::max(0, static_cast<int>(cx - rx) - 1);
    const int iy0 = std::max(0, static_cast<int>(cy - ry) - 1);
    const int ix1 = std::min(w_ - 1, static_cast<int>(cx + rx) + 1);
    const int iy1 = std::min(h_ - 1, static_cast<int>(cy + ry) + 1);
    for (int y = iy0; y <= iy1; ++y) {
        for (int x = ix0; x <= ix1; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
            const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
            const float d = std::sqrt(nx * nx + ny * ny);
            const float cov = clamp01((1.0f - d) * k + 0.5f);
            if (cov <= 0.0f) continue;
            const float nz = std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, d * d)));
            const float diffuse = std::max(0.0f, nx * lx + ny * ly + nz * lz);
            const float spec = std::pow(std::max(0.0f, (nx * hx + ny * hy + nz * hz) / hl), 24.0f) * gloss;
            // Darken towards the rim for roundness.
            const float rim = 1.0f - 0.25f * std::pow(d, 4.0f);
            const float light = (ambient + (1.0f - ambient) * diffuse) * rim;
            Col c{std::min(1.0f, base.r * light + spec), std::min(1.0f, base.g * light + spec),
                  std::min(1.0f, base.b * light + spec), base.a};
            blend(x, y, c, cov);
        }
    }
}

void Canvas::gradientRect(float x, float y, float w, float h, Col top, Col bottom) {
    const int iy0 = std::max(0, static_cast<int>(std::floor(y)));
    const int iy1 = std::min(h_ - 1, static_cast<int>(std::ceil(y + h)) - 1);
    const int ix0 = std::max(0, static_cast<int>(std::floor(x)));
    const int ix1 = std::min(w_ - 1, static_cast<int>(std::ceil(x + w)) - 1);
    for (int py = iy0; py <= iy1; ++py) {
        const float t = h > 1.0f ? (static_cast<float>(py) - y) / (h - 1.0f) : 0.0f;
        const Col c = Col::mix(top, bottom, clamp01(t));
        for (int px = ix0; px <= ix1; ++px) blend(px, py, c);
    }
}

void Canvas::softEllipse(float cx, float cy, float rx, float ry, Col c) {
    const int ix0 = std::max(0, static_cast<int>(cx - rx));
    const int iy0 = std::max(0, static_cast<int>(cy - ry));
    const int ix1 = std::min(w_ - 1, static_cast<int>(cx + rx) + 1);
    const int iy1 = std::min(h_ - 1, static_cast<int>(cy + ry) + 1);
    for (int y = iy0; y <= iy1; ++y) {
        for (int x = ix0; x <= ix1; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
            const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
            const float d = nx * nx + ny * ny;
            if (d >= 1.0f) continue;
            const float f = (1.0f - d);
            blend(x, y, c, f * f);
        }
    }
}

void Canvas::blitTo(SDL_Surface* s, int ox, int oy) const {
    if (!s) return;
    SDL_LockSurface(s);
    for (int y = 0; y < h_; ++y) {
        const int sy = oy + y;
        if (sy < 0 || sy >= s->h) continue;
        auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(s->pixels) + sy * s->pitch);
        for (int x = 0; x < w_; ++x) {
            const int sx = ox + x;
            if (sx < 0 || sx >= s->w) continue;
            const Col& c = px_[static_cast<size_t>(y * w_ + x)];
            auto ch = [](float v) { return static_cast<Uint8>(std::lround(clamp01(v) * 255.0f)); };
            row[sx] = SDL_MapRGBA(s->format, ch(c.r), ch(c.g), ch(c.b), ch(c.a));
        }
    }
    SDL_UnlockSurface(s);
}

SurfacePtr Canvas::toSurface() const {
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, w_, h_, 32, SDL_PIXELFORMAT_RGBA32));
    if (s) blitTo(s.get(), 0, 0);
    return s;
}

} // namespace pd
