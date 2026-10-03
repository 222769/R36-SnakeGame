#pragma once

// Small immediate-mode drawing helpers. Circles, ellipses and rounded panels
// are drawn from a few anti-aliased "skin" textures baked by initSkin(), so
// they have smooth edges; without the skin they fall back to plain spans.

#include "Math.h"

#include <SDL.h>

namespace pd::draw {

void setColor(SDL_Renderer* r, SDL_Color c);
void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c);
void rectOutline(SDL_Renderer* r, const RectF& rect, SDL_Color c);
void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
void fillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry, SDL_Color c);
// Soft-edged ellipse whose alpha fades towards the rim (shadows, glows).
void softEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry, SDL_Color c);

// Rounded rectangle (radius ~10 px) with an optional 2 px border and an
// optional soft drop shadow. Alpha 0 skips that part.
void roundedRect(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill, SDL_Color border,
                 SDL_Color shadow = SDL_Color{0, 0, 0, 0});

// Builds / frees the skin textures (call releaseSkin before the renderer dies).
void initSkin(SDL_Renderer* r);
void releaseSkin();

// Surface variants, used when baking static textures at load time.
void surfaceFillRect(SDL_Surface* s, int x, int y, int w, int h, SDL_Color c);
void surfaceFillCircle(SDL_Surface* s, int cx, int cy, int radius, SDL_Color c);

constexpr SDL_Color rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return SDL_Color{r, g, b, a}; }

} // namespace pd::draw
