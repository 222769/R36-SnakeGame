#include "Draw.h"

#include "Canvas.h"
#include "SdlPtr.h"

#include <algorithm>
#include <cmath>

namespace pd::draw {

namespace {

constexpr int kDiscSize = 64;
constexpr int kSliceSize = 32;   // rounded-rect source textures
constexpr int kSliceCorner = 12; // corner size of the 9-slice
constexpr int kShadowSize = 48;
constexpr int kShadowCorner = 20;

struct Skin {
    TexturePtr disc;   // anti-aliased white disc
    TexturePtr soft;   // radial fade disc
    TexturePtr fill;   // white rounded rect
    TexturePtr border; // white rounded outline
    TexturePtr shadow; // blurred rounded rect
};
Skin gSkin;

TexturePtr bake(SDL_Renderer* r, const Canvas& c) {
    SurfacePtr s = c.toSurface();
    if (!s) return nullptr;
    TexturePtr t(SDL_CreateTextureFromSurface(r, s.get()));
    if (t) SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
    return t;
}

void tinted(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect& dst, SDL_Color c) {
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
    SDL_RenderCopy(r, t, src, &dst);
}

// Stretches texture `t` (size x size, corners `corner`) over `dst` as a 9-slice.
void nineSlice(SDL_Renderer* r, SDL_Texture* t, int size, int corner, const SDL_Rect& dst, SDL_Color c) {
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
    const int k = std::min({corner, dst.w / 2, dst.h / 2});
    if (k <= 0) return;
    const int sx[4] = {0, corner, size - corner, size};
    const int dx[4] = {dst.x, dst.x + k, dst.x + dst.w - k, dst.x + dst.w};
    const int dy[4] = {dst.y, dst.y + k, dst.y + dst.h - k, dst.y + dst.h};
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            const SDL_Rect s{sx[i], sx[j], sx[i + 1] - sx[i], sx[j + 1] - sx[j]};
            const SDL_Rect d{dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j]};
            if (d.w > 0 && d.h > 0) SDL_RenderCopy(r, t, &s, &d);
        }
    }
}

} // namespace

void initSkin(SDL_Renderer* r) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    const Col white{1, 1, 1, 1};
    {
        Canvas c(kDiscSize, kDiscSize);
        c.fillCircle(kDiscSize / 2.0f, kDiscSize / 2.0f, kDiscSize / 2.0f - 1.0f, white);
        gSkin.disc = bake(r, c);
    }
    {
        Canvas c(kDiscSize, kDiscSize);
        c.softEllipse(kDiscSize / 2.0f, kDiscSize / 2.0f, kDiscSize / 2.0f, kDiscSize / 2.0f, white);
        gSkin.soft = bake(r, c);
    }
    {
        Canvas c(kSliceSize, kSliceSize);
        c.fillRoundRect(0, 0, kSliceSize, kSliceSize, 10.0f, white);
        gSkin.fill = bake(r, c);
    }
    {
        Canvas c(kSliceSize, kSliceSize);
        c.strokeRoundRect(1.0f, 1.0f, kSliceSize - 2.0f, kSliceSize - 2.0f, 9.0f, 2.0f, white);
        gSkin.border = bake(r, c);
    }
    {
        // Shadow: rounded rect whose alpha falls off over ~10 px.
        Canvas c(kShadowSize, kShadowSize);
        const float half = kShadowSize / 2.0f;
        c.paint([&](int x, int y) {
            const float px = std::fabs(static_cast<float>(x) + 0.5f - half) - (half - 20.0f);
            const float py = std::fabs(static_cast<float>(y) + 0.5f - half) - (half - 20.0f);
            const float d = std::hypot(std::max(px, 0.0f), std::max(py, 0.0f)) + std::min(std::max(px, py), 0.0f);
            const float t = std::clamp((d - 4.0f) / 14.0f, 0.0f, 1.0f); // 0 inside .. 1 at the rim
            return Col{0, 0, 0, (1.0f - t) * (1.0f - t)};
        });
        gSkin.shadow = bake(r, c);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
}

void releaseSkin() { gSkin = Skin{}; }

void setColor(SDL_Renderer* r, SDL_Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a); }

void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c) {
    setColor(r, c);
    const SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void rectOutline(SDL_Renderer* r, const RectF& rect, SDL_Color c) {
    setColor(r, c);
    const SDL_Rect out{static_cast<int>(std::floor(rect.x)), static_cast<int>(std::floor(rect.y)),
                       static_cast<int>(std::round(rect.w)), static_cast<int>(std::round(rect.h))};
    SDL_RenderDrawRect(r, &out);
}

void fillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry, SDL_Color c) {
    if (rx <= 0 || ry <= 0) return;
    if (gSkin.disc) {
        tinted(r, gSkin.disc.get(), nullptr, SDL_Rect{cx - rx, cy - ry, rx * 2 + 1, ry * 2 + 1}, c);
        return;
    }
    // One horizontal span per row, submitted as a single batch.
    SDL_Rect spans[128];
    int count = 0;
    for (int dy = -ry; dy <= ry && count < 128; ++dy) {
        const float t = static_cast<float>(dy) / static_cast<float>(ry);
        const int half = static_cast<int>(std::lround(rx * std::sqrt(std::max(0.0f, 1.0f - t * t))));
        spans[count++] = SDL_Rect{cx - half, cy + dy, half * 2 + 1, 1};
    }
    setColor(r, c);
    SDL_RenderFillRects(r, spans, count);
}

void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c) {
    fillEllipse(r, cx, cy, radius, radius, c);
}

void softEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry, SDL_Color c) {
    if (rx <= 0 || ry <= 0) return;
    if (!gSkin.soft) {
        fillEllipse(r, cx, cy, rx, ry, SDL_Color{c.r, c.g, c.b, static_cast<Uint8>(c.a / 2)});
        return;
    }
    tinted(r, gSkin.soft.get(), nullptr, SDL_Rect{cx - rx, cy - ry, rx * 2, ry * 2}, c);
}

void roundedRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill, SDL_Color border, SDL_Color shadow) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (!gSkin.fill) { // fallback: plain rectangles
        if (fill.a) fillRect(r, rect.x, rect.y, rect.w, rect.h, fill);
        return;
    }
    if (shadow.a) {
        constexpr int grow = 10;
        nineSlice(r, gSkin.shadow.get(), kShadowSize, kShadowCorner,
                  SDL_Rect{rect.x - grow + 1, rect.y - grow + 4, rect.w + grow * 2 - 2, rect.h + grow * 2}, shadow);
    }
    if (fill.a) nineSlice(r, gSkin.fill.get(), kSliceSize, kSliceCorner, rect, fill);
    if (border.a) nineSlice(r, gSkin.border.get(), kSliceSize, kSliceCorner, rect, border);
}

void surfaceFillRect(SDL_Surface* s, int x, int y, int w, int h, SDL_Color c) {
    const SDL_Rect rect{x, y, w, h};
    SDL_FillRect(s, &rect, SDL_MapRGBA(s->format, c.r, c.g, c.b, c.a));
}

void surfaceFillCircle(SDL_Surface* s, int cx, int cy, int radius, SDL_Color c) {
    const Uint32 color = SDL_MapRGBA(s->format, c.r, c.g, c.b, c.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        const int half = static_cast<int>(std::lround(std::sqrt(static_cast<float>(radius * radius - dy * dy))));
        const SDL_Rect span{cx - half, cy + dy, half * 2 + 1, 1};
        SDL_FillRect(s, &span, color);
    }
}

} // namespace pd::draw
