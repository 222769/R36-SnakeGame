#pragma once

#include <SDL.h>

#include <string_view>

namespace pd {

class BitmapFont;
class Game;

// Small helpers shared by the menu scenes, so every screen moves, sounds and
// looks the same: rounded "pill" rows, a header, a hint bar at the bottom.
namespace menu {

// Moves `index` with Up/Down (or Left/Right when `horizontal`), with
// auto-repeat while held. Plays the move sound. Returns true if it moved.
bool navigate(Game& game, int& index, int count, bool horizontal = false, bool wrap = true);
// -1 / +1 for Left / Right (auto-repeating), 0 otherwise.
int adjust(Game& game);

// Big outlined title at the top of the screen.
void drawHeader(SDL_Renderer* r, const BitmapFont& font, std::string_view title);
// Rounded hint bar at the bottom, e.g. "A SELECT   B BACK".
void drawHints(SDL_Renderer* r, const BitmapFont& font, std::string_view hints);
// One pill-shaped row, highlighted when selected. `value` (if not empty) is
// drawn right-aligned inside the pill with < > arrows when selected.
void drawRow(SDL_Renderer* r, const BitmapFont& font, const SDL_Rect& rect, std::string_view label,
             std::string_view value, bool selected, bool enabled = true);

// A row whose value is a 0..max level bar (volumes).
void drawBarRow(SDL_Renderer* r, const BitmapFont& font, const SDL_Rect& rect, std::string_view label, int level,
                int max, bool selected);

} // namespace menu

} // namespace pd
