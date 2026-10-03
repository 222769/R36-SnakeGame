#pragma once

// Small software painter used to generate all the game's artwork at load
// time: anti-aliased shapes, gradients, lit "3D" spheres and tileable noise.
// Nothing here runs per frame; the results become ordinary SDL textures.

#include "Math.h"
#include "SdlPtr.h"

#include <cstdint>
#include <vector>

namespace pd {

// Straight-alpha colour, channels 0..1.
struct Col {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    static Col rgb(int r, int g, int b, int a = 255) {
        return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f,
                static_cast<float>(a) / 255.0f};
    }
    Col scaled(float k) const { return {r * k, g * k, b * k, a}; }
    Col withAlpha(float alpha) const { return {r, g, b, alpha}; }
    static Col mix(Col x, Col y, float t) {
        return {x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t};
    }
};

// Smooth value noise in [0,1], tileable with the given period (pixels), so
// textures repeat seamlessly across tiles.
float tileNoise(float x, float y, int period, uint32_t seed);
// Fractal (several octaves) version of tileNoise.
float tileFbm(float x, float y, int period, uint32_t seed, int octaves = 4);

class Canvas {
public:
    Canvas(int width, int height);

    int width() const { return w_; }
    int height() const { return h_; }

    void clear(Col c = {0, 0, 0, 0});
    // Composites `c` over the pixel with the given coverage (0..1).
    void blend(int x, int y, Col c, float coverage = 1.0f);
    Col get(int x, int y) const;

    // --- Anti-aliased shapes (coverage from the distance to the edge) ---
    void fillCircle(float cx, float cy, float r, Col c);
    void fillEllipse(float cx, float cy, float rx, float ry, Col c);
    void fillRoundRect(float x, float y, float w, float h, float radius, Col c);
    void strokeRoundRect(float x, float y, float w, float h, float radius, float width, Col c);
    // Thick line with round caps.
    void line(Vec2 a, Vec2 b, float width, Col c);
    // Any simple polygon, 4x4 supersampled.
    void fillPolygon(const std::vector<Vec2>& points, Col c);
    // Ring between radii r0 < r1.
    void ring(float cx, float cy, float r0, float r1, Col c);

    // --- Shading ---
    // Ellipse lit like a sphere from the upper left: diffuse light plus a
    // specular highlight (`gloss` 0 = matte, 1 = shiny).
    void sphere(float cx, float cy, float rx, float ry, Col base, float gloss = 0.3f, float ambient = 0.5f);
    // Vertical gradient over a rectangle.
    void gradientRect(float x, float y, float w, float h, Col top, Col bottom);
    // Soft round shadow / glow: alpha fades from the centre to the edge.
    void softEllipse(float cx, float cy, float rx, float ry, Col c);

    // Per-pixel paint: f(x, y) returns a colour composited over the canvas.
    template <class F>
    void paint(F f) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) blend(x, y, f(x, y));
    }

    // Copies into an RGBA32 surface at (ox, oy).
    void blitTo(SDL_Surface* surface, int ox, int oy) const;
    SurfacePtr toSurface() const;

private:
    // Calls f(px, py, coverage) for every pixel near the box with an SDF
    // distance (negative inside).
    template <class Sdf>
    void fillSdf(float x0, float y0, float x1, float y1, Sdf sdf, Col c);

    int w_;
    int h_;
    std::vector<Col> px_;
};

} // namespace pd
