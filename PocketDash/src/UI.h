#pragma once

#include "SdlPtr.h"

#include <SDL.h>

#include <string>
#include <string_view>

namespace pd {

// Chunky built-in 5x7 pixel font (6x8 cell), baked into one texture at
// startup. Needs no font file, is crisp at every integer scale and costs a
// single texture bind per string. Lower-case letters render as capitals.
class BitmapFont {
public:
    static constexpr int kGlyphW = 5;
    static constexpr int kGlyphH = 7;
    static constexpr int kCellW = 6;
    static constexpr int kCellH = 8;

    bool create(SDL_Renderer* renderer);

    void draw(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color) const;
    // Draws with a 1-scaled-pixel drop shadow for readability on busy backgrounds.
    void drawShadowed(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color,
                      SDL_Color shadow = SDL_Color{20, 16, 40, 255}) const;
    void drawCentered(SDL_Renderer* r, int centerX, int y, std::string_view text, int scale,
                      SDL_Color color, bool shadow = true) const;

    static int textWidth(std::string_view text, int scale);
    static int lineHeight(int scale) { return kCellH * scale; }

    // Checks the embedded glyph table (used by the unit tests).
    static bool validateGlyphData(std::string* error);

private:
    TexturePtr atlas_;
};

namespace ui {

// Palette shared by all UI elements so screens look consistent.
constexpr SDL_Color kWhite{255, 255, 255, 255};
constexpr SDL_Color kInk{30, 22, 48, 255};
constexpr SDL_Color kYellow{255, 214, 64, 255};
constexpr SDL_Color kPink{255, 120, 160, 255};
constexpr SDL_Color kSky{120, 200, 255, 255};
constexpr SDL_Color kMint{120, 230, 160, 255};
constexpr SDL_Color kGrey{150, 150, 170, 255};
constexpr SDL_Color kPanel{34, 28, 60, 220};
constexpr SDL_Color kPanelBorder{255, 255, 255, 255};

// Rounded-looking framed panel (alpha-blended fill + 2px border).
void drawPanel(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill = kPanel, SDL_Color border = kPanelBorder);

// Full-screen translucent dim, e.g. behind the pause menu.
void dimScreen(SDL_Renderer* r, Uint8 alpha);

} // namespace ui

} // namespace pd
