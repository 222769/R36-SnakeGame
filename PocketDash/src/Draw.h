#pragma once

// Small immediate-mode drawing helpers for placeholder graphics.
// All renderer functions use (and restore nothing beyond) the colour passed in.

#include "Math.h"

#include <SDL.h>

namespace pd::draw {

void setColor(SDL_Renderer* r, SDL_Color c);
void fillRect(SDL_Renderer* r, int x, int y, int w, int h, SDL_Color c);
void rectOutline(SDL_Renderer* r, const RectF& rect, SDL_Color c);
void fillCircle(SDL_Renderer* r, int cx, int cy, int radius, SDL_Color c);
void fillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry, SDL_Color c);

// Surface variants, used when baking static textures at load time.
void surfaceFillRect(SDL_Surface* s, int x, int y, int w, int h, SDL_Color c);
void surfaceFillCircle(SDL_Surface* s, int cx, int cy, int radius, SDL_Color c);

constexpr SDL_Color rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return SDL_Color{r, g, b, a}; }

} // namespace pd::draw
