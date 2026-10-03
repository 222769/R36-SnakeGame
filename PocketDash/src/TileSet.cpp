#include "TileSet.h"

#include "Canvas.h"
#include "Constants.h"
#include "Level.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

constexpr int S = TileSet::kArtSize;
constexpr float SF = static_cast<float>(S);

uint32_t hash(int a, int b, int salt = 0) {
    uint32_t h = static_cast<uint32_t>(a) * 374761393u + static_cast<uint32_t>(b) * 668265263u +
                 static_cast<uint32_t>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Deterministic random float in [0,1) for art placement.
float rnd(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0xffff) / 65536.0f;
}

Col fromSdl(SDL_Color c) { return Col::rgb(c.r, c.g, c.b, c.a); }

// --- Ground ------------------------------------------------------------------

// Seamless grass: two layers of tileable noise blend light and dark greens,
// with fine grain, then per-variant details (blades, clover, pebbles, flowers).
void paintGrass(Canvas& c, const WorldTheme& t, int variant, bool alt) {
    const Col base = fromSdl(alt ? t.groundAlt : t.ground);
    const Col dark = base.scaled(0.80f);
    const Col light = Col::mix(base.scaled(1.12f), Col::rgb(200, 210, 120), 0.08f);
    c.paint([&](int x, int y) {
        const float n = tileFbm(static_cast<float>(x), static_cast<float>(y), S, 7u, 3);
        const float grain = tileNoise(static_cast<float>(x) * 0.9f, static_cast<float>(y) * 0.9f, 29, 3u);
        Col col = Col::mix(dark, light, std::clamp(n * 1.4f - 0.2f, 0.0f, 1.0f));
        return col.scaled(0.94f + grain * 0.12f);
    });
    uint32_t rs = 1000u + static_cast<uint32_t>(variant) * 77u;
    // Grass blades: short strokes leaning slightly, darker at the base.
    const int blades = 10 + variant * 2;
    for (int i = 0; i < blades; ++i) {
        const float x = 3.0f + rnd(rs) * (SF - 6.0f);
        const float y = 5.0f + rnd(rs) * (SF - 8.0f);
        const float lean = (rnd(rs) - 0.5f) * 3.0f;
        const float h = 3.0f + rnd(rs) * 3.0f;
        const bool lit = rnd(rs) > 0.5f;
        c.line({x, y}, {x + lean, y - h}, 1.0f, (lit ? light.scaled(1.05f) : dark.scaled(0.9f)).withAlpha(0.8f));
    }
    if (variant == 1) { // clover patch
        for (int i = 0; i < 3; ++i) {
            const float x = 9.0f + rnd(rs) * 14.0f;
            const float y = 9.0f + rnd(rs) * 14.0f;
            for (int k = 0; k < 3; ++k) {
                const float a = static_cast<float>(k) * 2.094f;
                c.fillCircle(x + std::cos(a) * 1.6f, y + std::sin(a) * 1.6f, 1.5f, dark.scaled(0.85f));
            }
        }
    } else if (variant == 2) { // pebbles
        for (int i = 0; i < 3; ++i) {
            const float x = 6.0f + rnd(rs) * 20.0f;
            const float y = 6.0f + rnd(rs) * 20.0f;
            c.softEllipse(x + 0.5f, y + 1.2f, 2.6f, 1.4f, Col::rgb(30, 40, 20, 90));
            c.sphere(x, y, 1.8f + rnd(rs), 1.3f + rnd(rs) * 0.6f, Col::rgb(170, 165, 150), 0.2f, 0.6f);
        }
    } else if (variant == 3) { // a few wild flowers
        const Col petals[] = {Col::rgb(250, 250, 245), Col::rgb(255, 214, 90), Col::rgb(240, 150, 180)};
        for (int i = 0; i < 2; ++i) {
            const float x = 7.0f + rnd(rs) * 18.0f;
            const float y = 7.0f + rnd(rs) * 18.0f;
            const Col p = petals[(variant + i) % 3];
            c.line({x, y + 1.0f}, {x, y + 4.0f}, 1.0f, dark);
            for (int k = 0; k < 5; ++k) {
                const float a = static_cast<float>(k) * 1.2566f;
                c.fillCircle(x + std::cos(a) * 1.7f, y + std::sin(a) * 1.7f, 1.3f, p);
            }
            c.fillCircle(x, y, 1.0f, Col::rgb(240, 170, 40));
        }
    }
}

// --- Hedges --------------------------------------------------------------------

// A dense bush made of many small lit leaf clusters. Clusters are repeated at
// +-32 px so leaves crossing a tile edge continue seamlessly on the neighbour.
void paintLeaves(Canvas& c, const WorldTheme& t, uint32_t seed, int count, float yMax, bool wrapY, float tint) {
    const Col wall = fromSdl(t.wall);
    uint32_t rs = seed;
    struct Clump {
        float x, y, r;
        Col col;
    };
    std::vector<Clump> clumps;
    for (int i = 0; i < count; ++i) {
        const float k = 0.82f + rnd(rs) * 0.36f;
        Col col = wall.scaled(k);
        if (tint > 0.0f && rnd(rs) < 0.15f) col = Col::mix(col, Col::rgb(170, 190, 90), tint);
        clumps.push_back({rnd(rs) * SF, rnd(rs) * yMax, 4.0f + rnd(rs) * 3.5f, col});
    }
    std::sort(clumps.begin(), clumps.end(), [](const Clump& a, const Clump& b) { return a.y < b.y; });
    for (const Clump& cl : clumps)
        for (int ox = -1; ox <= 1; ++ox)
            for (int oy = wrapY ? -1 : 0; oy <= (wrapY ? 1 : 0); ++oy)
                c.sphere(cl.x + static_cast<float>(ox) * SF, cl.y + static_cast<float>(oy) * SF, cl.r, cl.r * 0.9f,
                         cl.col, 0.12f, 0.45f);
}

void paintHedgeTop(Canvas& c, const WorldTheme& t, bool secret) {
    c.clear(fromSdl(t.wallShade).scaled(0.8f));
    paintLeaves(c, t, 4242u, 34, SF, true, secret ? 0.5f : 0.0f);
}

void paintHedgeFront(Canvas& c, const WorldTheme& t, bool secret) {
    paintHedgeTop(c, t, secret);
    // The front face: leaves in shadow, darkening towards the ground.
    const Col shade = fromSdl(t.wallShade);
    for (int y = 18; y < S; ++y) {
        const float k = static_cast<float>(y - 18) / static_cast<float>(S - 18);
        for (int x = 0; x < S; ++x) {
            const float n = tileFbm(static_cast<float>(x), static_cast<float>(y) * 2.0f, S, 91u, 2);
            const Col leaf = shade.scaled(0.75f + n * 0.4f - k * 0.25f);
            c.blend(x, y, leaf, std::min(1.0f, k * 1.6f + 0.25f));
        }
    }
    c.gradientRect(0, SF - 3.0f, SF, 3.0f, Col::rgb(0, 0, 0, 40), Col::rgb(0, 0, 0, 120));
}

void paintTinyGap(Canvas& c, const WorldTheme& t) {
    paintHedgeTop(c, t, false);
    // A small, deep hole at the bottom: only a tiny hero fits.
    c.fillRoundRect(10.0f, 17.0f, 12.0f, 16.0f, 6.0f, Col::rgb(20, 24, 18));
    c.fillRoundRect(12.0f, 20.0f, 8.0f, 13.0f, 4.0f, Col::rgb(8, 10, 8));
    c.strokeRoundRect(9.5f, 16.5f, 13.0f, 17.0f, 6.0f, 1.2f, fromSdl(t.wall).scaled(0.6f));
}

// --- Objects on grass ----------------------------------------------------------

void paintTree(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 0, false);
    c.softEllipse(17.0f, 27.0f, 14.0f, 5.0f, Col::rgb(10, 30, 10, 120));
    // Trunk: a cylinder (lit from the left).
    for (int y = 18; y < 29; ++y)
        for (int x = 13; x < 19; ++x) {
            const float u = (static_cast<float>(x) - 13.0f) / 6.0f;
            c.blend(x, y, Col::rgb(110, 76, 48).scaled(1.15f - u * 0.5f));
        }
    const Col leaf = fromSdl(t.wall);
    c.sphere(16.0f, 13.0f, 13.0f, 11.5f, leaf.scaled(0.85f), 0.1f, 0.45f);
    c.sphere(10.0f, 11.0f, 7.5f, 6.5f, leaf.scaled(1.0f), 0.1f, 0.5f);
    c.sphere(21.0f, 10.0f, 7.5f, 6.5f, leaf.scaled(0.95f), 0.1f, 0.5f);
    c.sphere(15.0f, 6.5f, 7.0f, 5.5f, leaf.scaled(1.1f), 0.12f, 0.55f);
    // Leaf texture speckles.
    uint32_t rs = 55u;
    for (int i = 0; i < 26; ++i) {
        const float x = 5.0f + rnd(rs) * 22.0f;
        const float y = 2.0f + rnd(rs) * 20.0f;
        c.fillCircle(x, y, 1.0f, leaf.scaled(rnd(rs) > 0.5f ? 1.25f : 0.7f).withAlpha(0.6f));
    }
}

void paintRock(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 1, false);
    c.softEllipse(17.0f, 25.0f, 13.0f, 5.0f, Col::rgb(10, 30, 10, 110));
    c.sphere(16.0f, 18.0f, 11.5f, 9.5f, Col::rgb(132, 128, 124), 0.18f, 0.45f);
    c.sphere(11.0f, 20.0f, 6.0f, 5.0f, Col::rgb(120, 116, 112), 0.15f, 0.45f);
    c.paint([&](int x, int y) {
        const float n = tileNoise(static_cast<float>(x) * 0.6f, static_cast<float>(y) * 0.6f, 19, 9u);
        const Col here = c.get(x, y);
        if (here.a < 0.9f || here.g > here.r + 0.05f) return Col{0, 0, 0, 0}; // only on the stone
        return Col::rgb(40, 38, 36, static_cast<int>(n * 60.0f));
    });
}

void paintBoulder(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 1, false);
    c.softEllipse(17.0f, 27.0f, 15.0f, 5.0f, Col::rgb(10, 30, 10, 130));
    c.sphere(16.0f, 16.0f, 14.0f, 13.0f, Col::rgb(138, 126, 112), 0.15f, 0.42f);
    // Deep cracks: a strong giant could smash this.
    const Col crack = Col::rgb(50, 42, 36);
    c.line({17, 4}, {14, 11}, 1.4f, crack);
    c.line({14, 11}, {18, 17}, 1.4f, crack);
    c.line({18, 17}, {15, 25}, 1.2f, crack);
    c.line({18, 17}, {24, 20}, 1.0f, crack);
    c.line({14, 11}, {8, 13}, 1.0f, crack);
}

void paintCrate(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 0, false);
    c.softEllipse(17.0f, 28.0f, 15.0f, 4.0f, Col::rgb(10, 30, 10, 120));
    const Col edge = Col::rgb(92, 58, 30);
    c.fillRoundRect(3.0f, 3.0f, 26.0f, 25.0f, 2.5f, edge);
    // Planks with wood grain (noise stretched horizontally).
    for (int y = 5; y < 26; ++y)
        for (int x = 5; x < 27; ++x) {
            const float g = tileNoise(static_cast<float>(x) * 0.25f, static_cast<float>(y) * 1.4f, 32, 21u);
            c.blend(x, y, Col::rgb(186, 128, 72).scaled(0.82f + g * 0.3f));
        }
    for (float y : {11.5f, 18.5f}) c.line({5, y}, {27, y}, 1.0f, edge.withAlpha(0.8f));
    c.line({6, 24}, {26, 6}, 3.0f, edge);
    c.line({6, 23}, {25, 5}, 1.0f, Col::rgb(220, 170, 110, 160));
    for (float x : {6.5f, 25.5f})
        for (float y : {6.5f, 24.5f}) c.sphere(x, y, 1.2f, 1.2f, Col::rgb(170, 170, 175), 0.6f);
    c.gradientRect(5.0f, 5.0f, 22.0f, 4.0f, Col::rgb(255, 230, 190, 50), Col::rgb(255, 230, 190, 0));
}

void paintThorns(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 0, false);
    const Col vine = Col::rgb(84, 52, 64);
    const Col tip = Col::rgb(205, 190, 175);
    const float centers[3][2] = {{9, 21}, {22, 12}, {22, 26}};
    for (const auto& cc : centers) {
        c.softEllipse(cc[0] + 1.0f, cc[1] + 3.0f, 8.0f, 3.0f, Col::rgb(10, 30, 10, 90));
        for (int k = 0; k < 5; ++k) {
            const float a = -2.6f + static_cast<float>(k) * 0.55f;
            const Vec2 base{cc[0], cc[1]};
            const Vec2 end{cc[0] + std::cos(a) * 8.0f, cc[1] + std::sin(a) * 7.0f};
            c.line(base, end, 1.6f, vine);
            // Thorns along each stem.
            const Vec2 mid = base + (end - base) * 0.6f;
            c.fillPolygon({mid + Vec2{-1.2f, 0}, mid + Vec2{1.2f, 0}, mid + Vec2{0, -3.0f}}, tip);
            c.fillPolygon({end + Vec2{-1.0f, 0.5f}, end + Vec2{1.0f, 0.5f}, end + Vec2{0, -2.5f}}, tip);
        }
        c.fillCircle(cc[0] + 3.0f, cc[1] - 2.0f, 1.3f, Col::rgb(170, 30, 50));
    }
}

// --- Water ---------------------------------------------------------------------

void paintWater(Canvas& c, const WorldTheme& t, int frame) {
    // Calm water: a gentle low-contrast swell and a few scattered wave glints
    // (random short arcs) so the 32 px repeat does not read as a grid.
    const Col deep = fromSdl(t.water).scaled(0.9f);
    const Col shallow = fromSdl(t.water).scaled(1.04f);
    const float shift = static_cast<float>(frame) * 8.0f;
    c.paint([&](int x, int y) {
        const float n = tileFbm(static_cast<float>(x) + shift, static_cast<float>(y), S, 31u, 3);
        return Col::mix(deep, shallow, n);
    });
    const Col glint = Col::rgb(226, 240, 250);
    for (int i = 0; i < 5; ++i) {
        const float gx = static_cast<float>((hash(i, 11) % 32 + frame * 8) % 32);
        const float gy = 3.0f + static_cast<float>(hash(i, 12) % 26);
        const float len = 3.0f + static_cast<float>(hash(i, 13) % 4);
        const float alpha = 0.25f + static_cast<float>(hash(i, 14) % 20) / 100.0f;
        // Draw wrapped so glints crossing the tile edge continue seamlessly.
        for (float wrap : {-SF, 0.0f, SF})
            c.line({gx + wrap - len, gy + 0.6f}, {gx + wrap + len, gy - 0.4f}, 1.1f, glint.withAlpha(alpha));
    }
}

void paintShore(Canvas& c, const WorldTheme& t, int frame) {
    paintWater(c, t, frame);
    // Muddy grass lip, wet sand, then a line of foam.
    c.gradientRect(0, 0, SF, 3.0f, fromSdl(t.ground).scaled(0.55f), Col::rgb(150, 130, 90));
    c.gradientRect(0, 3.0f, SF, 3.0f, Col::rgb(196, 176, 128), Col::rgb(170, 160, 130, 120));
    for (int x = 0; x < S; ++x) {
        const float n = tileNoise(static_cast<float>(x) * 0.4f + static_cast<float>(frame) * 2.0f, 1.0f, 13, 5u);
        c.blend(x, 6, Col::rgb(240, 248, 250), 0.4f + n * 0.5f);
        c.blend(x, 7, Col::rgb(240, 248, 250), n * 0.4f);
    }
}

void paintBridge(Canvas& c, const WorldTheme& t, bool northSouth) {
    paintWater(c, t, 0);
    Canvas planks(S, S);
    // Draw as a north-south bridge (planks run across) and transpose if needed.
    planks.softEllipse(16.0f, 16.0f, 16.0f, 18.0f, Col::rgb(0, 20, 40, 80));
    for (int i = 0; i < 5; ++i) {
        const float y = 1.0f + static_cast<float>(i) * 6.4f;
        const float tone = 0.88f + static_cast<float>(hash(i, 3) % 20) / 100.0f;
        for (int py = static_cast<int>(y); py < static_cast<int>(y) + 5; ++py)
            for (int px = 4; px < 28; ++px) {
                const float g = tileNoise(static_cast<float>(px) * 0.3f, static_cast<float>(py) * 1.5f, 32, 61u + i);
                const float top = py == static_cast<int>(y) ? 1.15f : 1.0f;
                planks.blend(px, py, Col::rgb(176, 122, 72).scaled(tone * (0.85f + g * 0.25f) * top));
            }
    }
    planks.gradientRect(2.0f, 0.0f, 3.0f, SF, Col::rgb(110, 72, 42), Col::rgb(90, 58, 34));
    planks.gradientRect(27.0f, 0.0f, 3.0f, SF, Col::rgb(110, 72, 42), Col::rgb(90, 58, 34));
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            const Col p = planks.get(northSouth ? x : y, northSouth ? y : x);
            c.blend(x, y, p);
        }
}

// --- Special tiles -------------------------------------------------------------

void paintExit(Canvas& c, const WorldTheme& t, int frame) {
    paintGrass(c, t, 0, true);
    c.softEllipse(13.0f, 28.0f, 10.0f, 3.5f, Col::rgb(10, 30, 10, 120));
    c.sphere(11.0f, 27.0f, 7.0f, 3.0f, Col::rgb(150, 148, 145), 0.2f);
    // Metal pole with a gold finial.
    for (int y = 4; y < 27; ++y)
        for (int x = 9; x < 12; ++x) c.blend(x, y, Col::rgb(205, 208, 215).scaled(x == 9 ? 1.1f : x == 11 ? 0.7f : 0.95f));
    c.sphere(10.5f, 3.5f, 2.3f, 2.3f, Col::rgb(240, 190, 60), 0.7f);
    // Waving cloth: the two frames bend the flag the other way.
    const float w = frame ? 1.0f : -1.0f;
    const std::vector<Vec2> flag = {{12, 5},          {17, 5.0f + w},  {22, 6.0f - w}, {28, 8.5f},
                                    {22, 12.0f - w}, {17, 13.0f + w}, {12, 14}};
    c.fillPolygon(flag, Col::rgb(214, 52, 58));
    // Fold shading.
    c.fillPolygon({{17, 5.0f + w}, {22, 6.0f - w}, {22, 12.0f - w}, {17, 13.0f + w}},
                  Col::rgb(0, 0, 0, frame ? 40 : 15));
    c.line({12.5f, 6.0f}, {22.0f, 7.5f - w}, 1.0f, Col::rgb(255, 255, 255, 70));
}

void paintLock(Canvas& c, const WorldTheme& t) {
    paintGrass(c, t, 0, false);
    c.softEllipse(16.0f, 29.0f, 16.0f, 3.0f, Col::rgb(10, 30, 10, 120));
    const Col wood = Col::rgb(150, 98, 56);
    for (int i = 0; i < 4; ++i) {
        const float x = 2.0f + static_cast<float>(i) * 8.0f;
        c.fillRoundRect(x, 2.0f, 5.0f, 27.0f, 2.0f, wood.scaled(0.75f));
        c.fillRoundRect(x + 1.0f, 2.5f, 2.5f, 26.0f, 1.2f, wood.scaled(1.1f));
    }
    c.fillRoundRect(0.0f, 7.0f, SF, 3.0f, 1.0f, wood.scaled(0.85f));
    c.fillRoundRect(0.0f, 20.0f, SF, 3.0f, 1.0f, wood.scaled(0.85f));
    // Padlock.
    c.ring(16.0f, 13.0f, 3.5f, 5.2f, Col::rgb(170, 170, 178));
    c.fillRoundRect(9.5f, 13.0f, 13.0f, 11.0f, 2.5f, Col::rgb(214, 160, 40));
    c.gradientRect(10.5f, 14.0f, 11.0f, 4.0f, Col::rgb(255, 230, 140, 160), Col::rgb(255, 230, 140, 0));
    c.fillCircle(16.0f, 18.0f, 1.6f, Col::rgb(50, 36, 20));
    c.line({16.0f, 18.5f}, {16.0f, 21.0f}, 1.4f, Col::rgb(50, 36, 20));
}

} // namespace

bool TileSet::build(SDL_Renderer* renderer, const WorldTheme& theme) {
    border_ = theme.wallShade;
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, S * kArtCount, S, 32, SDL_PIXELFORMAT_RGBA32));
    if (!s) return false;
    SDL_FillRect(s.get(), nullptr, 0);

    auto paint = [&](Art a, auto&& painter) {
        Canvas c(S, S);
        painter(c);
        c.blitTo(s.get(), static_cast<int>(a) * S, 0);
    };
    for (int v = 0; v < 4; ++v) {
        paint(static_cast<Art>(kGrassA0 + v), [&](Canvas& c) { paintGrass(c, theme, v, false); });
        paint(static_cast<Art>(kGrassB0 + v), [&](Canvas& c) { paintGrass(c, theme, v, true); });
    }
    paint(kHedgeTop, [&](Canvas& c) { paintHedgeTop(c, theme, false); });
    paint(kHedgeFront, [&](Canvas& c) { paintHedgeFront(c, theme, false); });
    paint(kSecretTop, [&](Canvas& c) { paintHedgeTop(c, theme, true); });
    paint(kSecretFront, [&](Canvas& c) { paintHedgeFront(c, theme, true); });
    paint(kTree, [&](Canvas& c) { paintTree(c, theme); });
    paint(kRock, [&](Canvas& c) { paintRock(c, theme); });
    paint(kWater0, [&](Canvas& c) { paintWater(c, theme, 0); });
    paint(kWater1, [&](Canvas& c) { paintWater(c, theme, 1); });
    paint(kShore0, [&](Canvas& c) { paintShore(c, theme, 0); });
    paint(kShore1, [&](Canvas& c) { paintShore(c, theme, 1); });
    paint(kBridgeV, [&](Canvas& c) { paintBridge(c, theme, true); });
    paint(kBridgeH, [&](Canvas& c) { paintBridge(c, theme, false); });
    paint(kThorns, [&](Canvas& c) { paintThorns(c, theme); });
    paint(kExit0, [&](Canvas& c) { paintExit(c, theme, 0); });
    paint(kExit1, [&](Canvas& c) { paintExit(c, theme, 1); });
    paint(kCrate, [&](Canvas& c) { paintCrate(c, theme); });
    paint(kBoulder, [&](Canvas& c) { paintBoulder(c, theme); });
    paint(kTinyGap, [&](Canvas& c) { paintTinyGap(c, theme); });
    paint(kLock, [&](Canvas& c) { paintLock(c, theme); });

    // Tiles are drawn 1:1, so keep nearest filtering: linear would bleed
    // neighbouring atlas slots into each other when the screen is scaled.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    atlas_.reset(SDL_CreateTextureFromSurface(renderer, s.get()));
    if (!atlas_) SDL_Log("[tiles] Failed to create tile atlas: %s", SDL_GetError());
    return atlas_ != nullptr;
}

void TileSet::prepare(const Level& level) {
    width_ = level.width();
    height_ = level.height();
    art_.assign(static_cast<size_t>(width_ * height_), kGrassA0);

    auto hedgeLike = [&](int x, int y) {
        const Tile t = level.tileAt(x, y);
        return t == Tile::Wall || t == Tile::SecretWall || t == Tile::TinyGap;
    };
    auto waterLike = [&](int x, int y) {
        const Tile t = level.tileAt(x, y);
        return t == Tile::Water || t == Tile::Bridge;
    };

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            Art a = kGrassA0;
            switch (level.tileAt(x, y)) {
            case Tile::Ground: {
                // Flowers (variant 3) are rarer than plain grass.
                static const uint8_t kVariants[8] = {0, 0, 0, 1, 1, 2, 2, 3};
                const int v = kVariants[hash(x, y) % 8];
                a = static_cast<Art>(((x + y) % 2 ? kGrassB0 : kGrassA0) + v);
                break;
            }
            case Tile::Wall: a = hedgeLike(x, y + 1) ? kHedgeTop : kHedgeFront; break;
            case Tile::SecretWall: a = hedgeLike(x, y + 1) ? kSecretTop : kSecretFront; break;
            case Tile::Tree: a = kTree; break;
            case Tile::Rock: a = kRock; break;
            case Tile::Water: a = waterLike(x, y - 1) ? kWater0 : kShore0; break;
            case Tile::Bridge: {
                // Real water above and below means the bridge runs east-west;
                // water left and right means north-south. (Neighbouring bridge
                // tiles don't count as water, or a long bridge would alternate.)
                auto water = [&](int wx, int wy) { return level.tileAt(wx, wy) == Tile::Water; };
                if (water(x, y - 1) && water(x, y + 1)) a = kBridgeH;
                else if (water(x - 1, y) && water(x + 1, y)) a = kBridgeV;
                else a = water(x, y - 1) || water(x, y + 1) ? kBridgeH : kBridgeV;
                break;
            }
            case Tile::Hazard: a = kThorns; break;
            case Tile::Exit: a = kExit0; break;
            case Tile::Crate: a = kCrate; break;
            case Tile::Boulder: a = kBoulder; break;
            case Tile::TinyGap: a = kTinyGap; break;
            case Tile::Lock: a = kLock; break;
            }
            art_[static_cast<size_t>(y * width_ + x)] = a;
        }
    }
}

TileSet::Art TileSet::artAt(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_) return kHedgeTop;
    return art_[static_cast<size_t>(ty * width_ + tx)];
}

void TileSet::render(SDL_Renderer* r, Vec2 camera, float time) const {
    // Anything outside the map shows as dark hedge.
    SDL_SetRenderDrawColor(r, border_.r, border_.g, border_.b, 255);
    SDL_RenderFillRect(r, nullptr);
    if (!atlas_ || width_ == 0) return;

    const int x0 = std::max(0, static_cast<int>(std::floor(camera.x / kTileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor(camera.y / kTileSize)));
    const int x1 = std::min(width_ - 1, static_cast<int>(std::floor((camera.x + kScreenWidth - 1) / kTileSize)));
    const int y1 = std::min(height_ - 1, static_cast<int>(std::floor((camera.y + kScreenHeight - 1) / kTileSize)));

    const int waterFrame = static_cast<int>(time * 2.0f) % 2;
    const int flagFrame = static_cast<int>(time * 4.0f) % 2;
    const int camX = static_cast<int>(camera.x);
    const int camY = static_cast<int>(camera.y);

    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            int a = art_[static_cast<size_t>(ty * width_ + tx)];
            if (a == kWater0 || a == kShore0) a += waterFrame;
            else if (a == kExit0) a += flagFrame;
            const SDL_Rect src{a * S, 0, S, S};
            const SDL_Rect dst{tx * kTileSize - camX, ty * kTileSize - camY, kTileSize, kTileSize};
            SDL_RenderCopy(r, atlas_.get(), &src, &dst);
        }
    }
}

} // namespace pd
