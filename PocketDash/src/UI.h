#pragma once

#include "SdlPtr.h"

#include <SDL.h>

#include <string>
#include <string_view>

namespace pd {

// Game font. Normally a smooth anti-aliased TrueType face (assets/fonts/),
// pre-rendered into one glyph atlas per size at startup so drawing a string
// costs one texture bind and no per-frame rasterising. If the font file or
// SDL_ttf is unavailable it falls back to a built-in 5x7 pixel font (6x8
// cell, lower-case letters render as capitals).
//
// Sizes are given as an integer `scale`: text at scale N occupies a line of
// lineHeight(N) = 8*N pixels in either backend, so layouts are shared.
class BitmapFont {
public:
    static constexpr int kGlyphW = 5;
    static constexpr int kGlyphH = 7;
    static constexpr int kCellW = 6;
    static constexpr int kCellH = 8;

    static constexpr int kMaxScale = 8;

    // `ttfPath` may be empty or missing; the pixel font is then used.
    bool create(SDL_Renderer* renderer, const std::string& ttfPath = {});
    ~BitmapFont();
    bool smooth() const { return smooth_; }

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
    struct GlyphInfo {
        SDL_Rect src{0, 0, 0, 0}; // in the atlas of its size
        int offsetY = 0;           // from the line top to the bitmap top
        int advance = 0;
    };
    struct SmoothSize {
        TexturePtr atlas;
        GlyphInfo glyphs[96];      // characters 32..127
    };

    bool createSmooth(SDL_Renderer* renderer, const std::string& ttfPath);
    const GlyphInfo* smoothGlyph(int scale, char c) const;
    void drawSmooth(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color) const;

    TexturePtr atlas_;
    bool smooth_ = false;
    SmoothSize sizes_[kMaxScale + 1];
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
constexpr SDL_Color kPanel{30, 36, 58, 225};
constexpr SDL_Color kPanelBorder{255, 255, 255, 200};

// Rounded, anti-aliased panel: soft shadow, alpha-blended fill and an
// optional 2 px border (alpha 0 = none).
void drawPanel(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill = kPanel, SDL_Color border = kPanelBorder);

// Full-screen translucent dim, e.g. behind the pause menu.
void dimScreen(SDL_Renderer* r, Uint8 alpha);

} // namespace ui

} // namespace pd
