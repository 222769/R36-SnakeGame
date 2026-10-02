#include "Draw.h"

#include <algorithm>
#include <cmath>

namespace pd::draw {

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
