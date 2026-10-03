#include "TileSet.h"

#include "Constants.h"
#include "Level.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

constexpr int S = TileSet::kArtSize;

SDL_Color shade(SDL_Color c, float f) {
    auto ch = [f](Uint8 v) { return static_cast<Uint8>(std::clamp(static_cast<float>(v) * f, 0.0f, 255.0f)); };
    return SDL_Color{ch(c.r), ch(c.g), ch(c.b), 255};
}

uint32_t hash(int a, int b, int salt = 0) {
    uint32_t h = static_cast<uint32_t>(a) * 374761393u + static_cast<uint32_t>(b) * 668265263u +
                 static_cast<uint32_t>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Draws into one 16x16 atlas slot; everything is clipped to the slot so
// shapes never bleed into neighbouring tiles.
struct Painter {
    SDL_Surface* surface;
    int ox;

    void rect(int x, int y, int w, int h, SDL_Color c) const {
        const int x0 = std::max(x, 0);
        const int y0 = std::max(y, 0);
        const int x1 = std::min(x + w, S);
        const int y1 = std::min(y + h, S);
        if (x1 <= x0 || y1 <= y0) return;
        const SDL_Rect r{ox + x0, y0, x1 - x0, y1 - y0};
        SDL_FillRect(surface, &r, SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a));
    }
    void px(int x, int y, SDL_Color c) const { rect(x, y, 1, 1, c); }
    void ellipse(int cx, int cy, int rx, int ry, SDL_Color c) const {
        for (int dy = -ry; dy <= ry; ++dy) {
            const float t = static_cast<float>(dy) / static_cast<float>(ry);
            const int half = static_cast<int>(std::lround(rx * std::sqrt(std::max(0.0f, 1.0f - t * t))));
            rect(cx - half, cy + dy, half * 2 + 1, 1, c);
        }
    }
    void circle(int cx, int cy, int r, SDL_Color c) const { ellipse(cx, cy, r, r, c); }
};

void paintGrass(const Painter& p, SDL_Color base, int variant) {
    p.rect(0, 0, S, S, base);
    const SDL_Color blade = shade(base, 0.78f);
    const SDL_Color light = shade(base, 1.12f);
    for (int i = 0; i < 3; ++i) {
        const uint32_t h = hash(variant, i, 7);
        const int x = 1 + static_cast<int>(h % 13);
        const int y = 2 + static_cast<int>((h >> 8) % 12);
        p.px(x, y, blade);       // little "v" tuft
        p.px(x + 1, y - 1, blade);
    }
    for (int i = 0; i < 2; ++i) {
        const uint32_t h = hash(variant, i, 11);
        p.px(static_cast<int>(h % 16), static_cast<int>((h >> 8) % 16), light);
    }
    if (variant == 3) { // a flower
        const SDL_Color petal{255, 255, 255, 255};
        p.px(10, 4, petal);
        p.px(9, 5, petal);
        p.px(11, 5, petal);
        p.px(10, 6, petal);
        p.px(10, 5, SDL_Color{255, 200, 60, 255});
    }
}

void paintHedgeTop(const Painter& p, const WorldTheme& t, bool secret) {
    p.rect(0, 0, S, S, t.wall);
    const SDL_Color light = shade(t.wall, 1.22f);
    const SDL_Color dark = shade(t.wall, 0.8f);
    for (int i = 0; i < 9; ++i) {
        const uint32_t h = hash(i, 3, 21);
        const int x = static_cast<int>(h % 15);
        const int y = static_cast<int>((h >> 8) % 15);
        p.rect(x, y, 2, 1, light);
    }
    for (int i = 0; i < 8; ++i) {
        const uint32_t h = hash(i, 5, 33);
        p.px(static_cast<int>(h % 16), static_cast<int>((h >> 8) % 16), dark);
    }
    if (secret) { // a subtle hint for sharp-eyed players
        p.px(7, 7, t.groundAlt);
        p.px(8, 8, t.groundAlt);
    }
}

void paintHedgeFront(const Painter& p, const WorldTheme& t, bool secret) {
    paintHedgeTop(p, t, secret);
    p.rect(0, 9, S, 1, shade(t.wall, 0.7f));
    p.rect(0, 10, S, 6, t.wallShade);
    const SDL_Color stripe = shade(t.wallShade, 0.8f);
    for (int x = 1; x < S; x += 3) p.rect(x, 11 + (x % 2), 1, 3, stripe);
    p.rect(0, 15, S, 1, shade(t.wallShade, 0.6f));
}

void paintTree(const Painter& p, const WorldTheme& t) {
    paintGrass(p, t.ground, 0);
    p.ellipse(8, 14, 6, 1, shade(t.ground, 0.7f));
    p.rect(7, 10, 2, 5, SDL_Color{120, 80, 50, 255});
    p.circle(8, 7, 6, t.wallShade);
    p.circle(8, 6, 5, t.wall);
    p.circle(6, 4, 2, shade(t.wall, 1.22f));
    p.px(10, 8, shade(t.wall, 1.22f));
}

void paintRock(const Painter& p, const WorldTheme& t) {
    paintGrass(p, t.ground, 1);
    p.ellipse(8, 14, 6, 1, shade(t.ground, 0.7f));
    p.circle(8, 9, 5, SDL_Color{112, 112, 130, 255});
    p.circle(8, 8, 5, SDL_Color{150, 150, 168, 255});
    p.circle(6, 6, 2, SDL_Color{200, 200, 214, 255});
    p.px(10, 10, SDL_Color{112, 112, 130, 255});
    p.px(11, 9, SDL_Color{112, 112, 130, 255});
}

void paintWater(const Painter& p, const WorldTheme& t, int frame) {
    p.rect(0, 0, S, S, t.water);
    const SDL_Color ripple = shade(t.water, 1.25f);
    for (int i = 0; i < 3; ++i) {
        const uint32_t h = hash(i, 9, 41);
        const int x = (static_cast<int>(h % 12) + frame * 2) % 13;
        const int y = 2 + i * 5;
        p.rect(x, y, 3, 1, ripple);
    }
}

void paintShore(const Painter& p, const WorldTheme& t, int frame) {
    paintWater(p, t, frame);
    p.rect(0, 0, S, 2, shade(t.ground, 0.7f));
    const SDL_Color foam{230, 245, 255, 255};
    for (int x = frame; x < S; x += 2) p.px(x, 2, foam);
}

void paintBridge(const Painter& p, const WorldTheme& t, bool northSouth) {
    paintWater(p, t, 0);
    const SDL_Color wood{186, 124, 70, 255};
    const SDL_Color gap{130, 82, 46, 255};
    const SDL_Color rail{100, 62, 36, 255};
    if (northSouth) {
        p.rect(2, 0, 12, S, wood);
        for (int y = 3; y < S; y += 4) p.rect(2, y, 12, 1, gap);
        p.rect(1, 0, 1, S, rail);
        p.rect(14, 0, 1, S, rail);
    } else {
        p.rect(0, 2, S, 12, wood);
        for (int x = 3; x < S; x += 4) p.rect(x, 2, 1, 12, gap);
        p.rect(0, 1, S, 1, rail);
        p.rect(0, 14, S, 1, rail);
    }
}

void paintThorns(const Painter& p, const WorldTheme& t) {
    paintGrass(p, t.ground, 0);
    const SDL_Color dark{110, 50, 90, 255};
    const SDL_Color tip{235, 205, 235, 255};
    const int centers[3][2] = {{4, 10}, {11, 6}, {11, 13}};
    for (const auto& c : centers) {
        p.rect(c[0] - 2, c[1] + 1, 5, 2, dark);
        p.rect(c[0] - 1, c[1] - 1, 3, 2, dark);
        p.px(c[0], c[1] - 3, tip);
        p.px(c[0], c[1] - 2, dark);
        p.px(c[0] - 3, c[1], tip);
        p.px(c[0] + 3, c[1], tip);
    }
}

void paintExit(const Painter& p, const WorldTheme& t, int frame) {
    paintGrass(p, t.groundAlt, 0);
    p.ellipse(8, 14, 5, 1, shade(t.ground, 0.7f));
    p.rect(3, 13, 6, 2, SDL_Color{150, 150, 168, 255});
    p.rect(5, 2, 1, 12, SDL_Color{235, 235, 245, 255});
    p.px(5, 1, t.accent);
    // Waving pennant: widths per row change between the two frames.
    static const int widths[2][5] = {{7, 6, 5, 3, 1}, {6, 7, 5, 4, 2}};
    const SDL_Color flag{232, 67, 79, 255};
    for (int i = 0; i < 5; ++i) p.rect(6, 2 + i + (frame && i > 2 ? 1 : 0), widths[frame][i], 1, flag);
    p.px(7, 3, shade(flag, 1.3f));
}

void paintCrate(const Painter& p, const WorldTheme& t) {
    paintGrass(p, t.ground, 0);
    const SDL_Color dark{110, 66, 34, 255};
    const SDL_Color wood{196, 136, 74, 255};
    const SDL_Color light{230, 178, 110, 255};
    p.ellipse(8, 14, 7, 1, shade(t.ground, 0.7f));
    p.rect(1, 2, 14, 12, dark);
    p.rect(2, 3, 12, 10, wood);
    // Diagonal brace and frame: reads clearly as "breakable box".
    for (int i = 0; i < 10; ++i) p.rect(3 + i, 3 + i, 2, 1, dark);
    p.rect(2, 3, 12, 1, light);
    p.rect(2, 7, 12, 1, dark);
    p.rect(2, 3, 1, 10, light);
}

void paintBoulder(const Painter& p, const WorldTheme& t) {
    paintGrass(p, t.ground, 1);
    p.ellipse(8, 14, 7, 1, shade(t.ground, 0.7f));
    p.circle(8, 8, 7, SDL_Color{96, 92, 112, 255});
    p.circle(8, 7, 6, SDL_Color{132, 128, 150, 255});
    p.circle(6, 5, 2, SDL_Color{180, 176, 196, 255});
    // Big crack: hints that something strong could smash it.
    const SDL_Color crack{60, 56, 74, 255};
    p.px(9, 3, crack);
    p.px(9, 4, crack);
    p.px(10, 5, crack);
    p.px(10, 6, crack);
    p.px(9, 7, crack);
    p.px(11, 7, crack);
    p.px(12, 8, crack);
}

void paintTinyGap(const Painter& p, const WorldTheme& t) {
    paintHedgeTop(p, t, false);
    // A small arched hole at the bottom: only a tiny hero fits.
    const SDL_Color hole{24, 20, 36, 255};
    p.rect(6, 10, 4, 6, hole);
    p.rect(5, 11, 6, 5, hole);
    p.px(5, 10, shade(t.wall, 0.7f));
    p.px(10, 10, shade(t.wall, 0.7f));
}

} // namespace

bool TileSet::build(SDL_Renderer* renderer, const WorldTheme& theme) {
    border_ = theme.wallShade;
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, S * kArtCount, S, 32, SDL_PIXELFORMAT_RGBA32));
    if (!s) return false;

    auto slot = [&](Art a) { return Painter{s.get(), static_cast<int>(a) * S}; };
    for (int v = 0; v < 4; ++v) {
        paintGrass(slot(static_cast<Art>(kGrassA0 + v)), theme.ground, v);
        paintGrass(slot(static_cast<Art>(kGrassB0 + v)), theme.groundAlt, v);
    }
    paintHedgeTop(slot(kHedgeTop), theme, false);
    paintHedgeFront(slot(kHedgeFront), theme, false);
    paintHedgeTop(slot(kSecretTop), theme, true);
    paintHedgeFront(slot(kSecretFront), theme, true);
    paintTree(slot(kTree), theme);
    paintRock(slot(kRock), theme);
    paintWater(slot(kWater0), theme, 0);
    paintWater(slot(kWater1), theme, 1);
    paintShore(slot(kShore0), theme, 0);
    paintShore(slot(kShore1), theme, 1);
    paintBridge(slot(kBridgeV), theme, true);
    paintBridge(slot(kBridgeH), theme, false);
    paintThorns(slot(kThorns), theme);
    paintExit(slot(kExit0), theme, 0);
    paintExit(slot(kExit1), theme, 1);
    paintCrate(slot(kCrate), theme);
    paintBoulder(slot(kBoulder), theme);
    paintTinyGap(slot(kTinyGap), theme);

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
            case Tile::Bridge: a = (waterLike(x - 1, y) && waterLike(x + 1, y)) ? kBridgeV : kBridgeH; break;
            case Tile::Hazard: a = kThorns; break;
            case Tile::Exit: a = kExit0; break;
            case Tile::Crate: a = kCrate; break;
            case Tile::Boulder: a = kBoulder; break;
            case Tile::TinyGap: a = kTinyGap; break;
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
