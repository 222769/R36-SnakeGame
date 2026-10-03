#include "UI.h"

#include "Constants.h"
#include "Draw.h"

#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace pd {

namespace {

struct Glyph {
    char ch;
    const char* rows[BitmapFont::kGlyphH];
};

// '#' = ink, '.' = empty. 5 columns x 7 rows per glyph.
const Glyph kGlyphs[] = {
    {' ', {".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"####.", "....#", "....#", ".###.", "....#", "....#", "####."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    {'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
    {',', {".....", ".....", ".....", ".....", ".##..", "..#..", ".#..."}},
    {':', {".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."}},
    {';', {".....", ".##..", ".##..", ".....", ".##..", "..#..", ".#..."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {'-', {".....", ".....", ".....", ".###.", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {'[', {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."}},
    {']', {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'\'', {"..#..", "..#..", ".#...", ".....", ".....", ".....", "....."}},
    {'"', {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."}},
    {'%', {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"}},
    {'*', {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."}},
    {'#', {".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#."}},
    {'&', {".##..", "#..#.", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#"}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {'x', {".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#"}}, // multiply sign
};

constexpr int kFirstChar = 32;
constexpr int kLastChar = 127;
constexpr int kAtlasCols = 16;
constexpr int kAtlasRows = (kLastChar - kFirstChar + kAtlasCols) / kAtlasCols;

const Glyph* findGlyph(char c) {
    for (const auto& g : kGlyphs)
        if (g.ch == c) return &g;
    return nullptr;
}

// Maps an input character to the atlas slot it should render with.
int glyphIndex(char c) {
    // 'x' has its own glyph (a small multiply sign for "x3" counters);
    // all other lower-case letters render as capitals.
    if (c != 'x' && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (!findGlyph(c)) c = '?';
    return static_cast<unsigned char>(c) - kFirstChar;
}

// The font whose metrics the static textWidth() uses (the last one created).
const BitmapFont* gActiveFont = nullptr;

// Pixel size of the TrueType face at a given scale: chosen so capitals are
// about as tall as the pixel font's (7 px per scale) without being wider.
int smoothPointSize(int scale) { return std::max(8, static_cast<int>(std::lround(scale * 8.6f))); }

} // namespace

BitmapFont::~BitmapFont() {
    if (gActiveFont == this) gActiveFont = nullptr;
}

bool BitmapFont::createSmooth(SDL_Renderer* renderer, const std::string& ttfPath) {
    if (ttfPath.empty() || !TTF_WasInit()) return false;
    for (int scale = 1; scale <= kMaxScale; ++scale) {
        TTF_Font* font = TTF_OpenFont(ttfPath.c_str(), smoothPointSize(scale));
        if (!font) {
            SDL_Log("[ui] TTF font %s unavailable (%s); using the pixel font", ttfPath.c_str(), TTF_GetError());
            return false;
        }
        TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
        const int ascent = TTF_FontAscent(font);
        // Baseline where the pixel font's baseline is (7 px per scale down).
        const int baseline = 7 * scale;

        // Render every glyph, then pack them in one row-wrapped atlas.
        SurfacePtr bitmaps[96];
        int atlasW = 0, rowW = 0, rowH = 0, atlasH = 0;
        constexpr int kAtlasWidth = 1024;
        SmoothSize& size = sizes_[scale];
        for (int i = 0; i < 96; ++i) {
            const Uint16 ch = static_cast<Uint16>(kFirstChar + i);
            GlyphInfo& g = size.glyphs[i];
            int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
            if (TTF_GlyphMetrics(font, ch, &minx, &maxx, &miny, &maxy, &advance) != 0) continue;
            g.advance = advance;
            g.offsetY = baseline - ascent;
            if (ch == ' ' || ch == 127) continue;
            bitmaps[i].reset(TTF_RenderGlyph_Blended(font, ch, SDL_Color{255, 255, 255, 255}));
            if (!bitmaps[i]) continue;
            const int w = bitmaps[i]->w, h = bitmaps[i]->h;
            if (rowW + w + 1 > kAtlasWidth) {
                atlasH += rowH + 1;
                rowW = 0;
                rowH = 0;
            }
            g.src = SDL_Rect{rowW, atlasH, w, h};
            rowW += w + 1;
            rowH = std::max(rowH, h);
            atlasW = std::max(atlasW, rowW);
        }
        atlasH += rowH;
        TTF_CloseFont(font);

        SurfacePtr atlas(SDL_CreateRGBSurfaceWithFormat(0, std::max(1, atlasW), std::max(1, atlasH), 32,
                                                        SDL_PIXELFORMAT_RGBA32));
        if (!atlas) return false;
        SDL_FillRect(atlas.get(), nullptr, SDL_MapRGBA(atlas->format, 0, 0, 0, 0));
        for (int i = 0; i < 96; ++i) {
            if (!bitmaps[i]) continue;
            SDL_SetSurfaceBlendMode(bitmaps[i].get(), SDL_BLENDMODE_NONE);
            SDL_Rect dst = size.glyphs[i].src;
            SDL_BlitSurface(bitmaps[i].get(), nullptr, atlas.get(), &dst);
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        size.atlas.reset(SDL_CreateTextureFromSurface(renderer, atlas.get()));
        if (!size.atlas) return false;
        SDL_SetTextureBlendMode(size.atlas.get(), SDL_BLENDMODE_BLEND);
    }
    return true;
}

const BitmapFont::GlyphInfo* BitmapFont::smoothGlyph(int scale, char c) const {
    const int s = std::clamp(scale, 1, kMaxScale);
    int index = static_cast<unsigned char>(c) - kFirstChar;
    if (index < 0 || index >= 96) index = '?' - kFirstChar;
    return &sizes_[s].glyphs[index];
}

void BitmapFont::drawSmooth(SDL_Renderer* r, int x, int y, std::string_view text, int scale,
                            SDL_Color color) const {
    SDL_Texture* atlas = sizes_[std::clamp(scale, 1, kMaxScale)].atlas.get();
    if (!atlas) return;
    SDL_SetTextureColorMod(atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(atlas, color.a);
    int penX = x;
    for (char c : text) {
        if (c == '\n') {
            penX = x;
            y += lineHeight(scale);
            continue;
        }
        const GlyphInfo* g = smoothGlyph(scale, c);
        if (g->src.w > 0) {
            const SDL_Rect dst{penX, y + g->offsetY, g->src.w, g->src.h};
            SDL_RenderCopy(r, atlas, &g->src, &dst);
        }
        penX += g->advance;
    }
}

bool BitmapFont::validateGlyphData(std::string* error) {
    for (const auto& g : kGlyphs) {
        for (int row = 0; row < kGlyphH; ++row) {
            const char* line = g.rows[row];
            if (!line || std::strlen(line) != static_cast<size_t>(kGlyphW)) {
                if (error) *error = std::string("glyph '") + g.ch + "' row " + std::to_string(row) + " has wrong width";
                return false;
            }
            for (const char* p = line; *p; ++p) {
                if (*p != '#' && *p != '.') {
                    if (error) *error = std::string("glyph '") + g.ch + "' has invalid pixel character";
                    return false;
                }
            }
        }
    }
    return true;
}

bool BitmapFont::create(SDL_Renderer* renderer, const std::string& ttfPath) {
    gActiveFont = this;
    smooth_ = createSmooth(renderer, ttfPath);
    if (smooth_) SDL_Log("[ui] Using TrueType font %s", ttfPath.c_str());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SurfacePtr surface(SDL_CreateRGBSurfaceWithFormat(0, kAtlasCols * kCellW, kAtlasRows * kCellH, 32,
                                                      SDL_PIXELFORMAT_RGBA32));
    if (!surface) {
        SDL_Log("[ui] Failed to create font surface: %s", SDL_GetError());
        return false;
    }
    SDL_FillRect(surface.get(), nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    const Uint32 ink = SDL_MapRGBA(surface->format, 255, 255, 255, 255);

    for (const auto& g : kGlyphs) {
        const int slot = static_cast<unsigned char>(g.ch) - kFirstChar;
        const int ox = (slot % kAtlasCols) * kCellW;
        const int oy = (slot / kAtlasCols) * kCellH;
        for (int y = 0; y < kGlyphH; ++y) {
            for (int x = 0; x < kGlyphW; ++x) {
                if (g.rows[y][x] != '#') continue;
                const SDL_Rect px{ox + x, oy + y, 1, 1};
                SDL_FillRect(surface.get(), &px, ink);
            }
        }
    }

    atlas_.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    if (!atlas_) {
        SDL_Log("[ui] Failed to create font texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(atlas_.get(), SDL_BLENDMODE_BLEND);
    return true;
}

int BitmapFont::textWidth(std::string_view text, int scale) {
    if (text.empty()) return 0;
    if (gActiveFont && gActiveFont->smooth_) {
        int width = 0, best = 0;
        for (char c : text) {
            if (c == '\n') {
                best = std::max(best, width);
                width = 0;
                continue;
            }
            width += gActiveFont->smoothGlyph(scale, c)->advance;
        }
        return std::max(best, width);
    }
    return static_cast<int>(text.size()) * kCellW * scale - scale;
}

void BitmapFont::draw(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color) const {
    if (smooth_) {
        drawSmooth(r, x, y, text, scale, color);
        return;
    }
    if (!atlas_) return;
    SDL_SetTextureColorMod(atlas_.get(), color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(atlas_.get(), color.a);
    int penX = x;
    for (char c : text) {
        if (c == '\n') {
            penX = x;
            y += kCellH * scale;
            continue;
        }
        if (c != ' ') {
            const int slot = glyphIndex(c);
            const SDL_Rect src{(slot % kAtlasCols) * kCellW, (slot / kAtlasCols) * kCellH, kCellW, kCellH};
            const SDL_Rect dst{penX, y, kCellW * scale, kCellH * scale};
            SDL_RenderCopy(r, atlas_.get(), &src, &dst);
        }
        penX += kCellW * scale;
    }
}

void BitmapFont::drawShadowed(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color,
                              SDL_Color shadow) const {
    if (smooth_) {
        // Soft drop shadow: offset copy, slightly transparent.
        const int off = std::max(1, (scale * 2 + 2) / 3);
        draw(r, x + off / 2, y + off, text, scale, SDL_Color{shadow.r, shadow.g, shadow.b, static_cast<Uint8>(shadow.a * 3 / 4)});
    } else {
        draw(r, x + scale, y + scale, text, scale, shadow);
    }
    draw(r, x, y, text, scale, color);
}

void BitmapFont::drawCentered(SDL_Renderer* r, int centerX, int y, std::string_view text, int scale,
                              SDL_Color color, bool shadow) const {
    const int x = centerX - textWidth(text, scale) / 2;
    if (shadow)
        drawShadowed(r, x, y, text, scale, color);
    else
        draw(r, x, y, text, scale, color);
}

namespace ui {

void drawPanel(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
    // Rounded, anti-aliased panel with a soft drop shadow; translucent HUD
    // backings (no border) get a lighter shadow.
    const Uint8 shadowAlpha = border.a ? 110 : static_cast<Uint8>(fill.a / 3);
    draw::roundedRect(r, rect, fill, border, SDL_Color{0, 0, 0, shadowAlpha});
}

void formatTime(char* out, size_t size, float seconds, bool tenths) {
    seconds = std::clamp(seconds, 0.0f, 99.0f * 60.0f + 59.9f); // keep the text short
    const int total = static_cast<int>(seconds);
    if (tenths)
        std::snprintf(out, size, "%d:%02d.%d", total / 60, total % 60, static_cast<int>(seconds * 10.0f) % 10);
    else
        std::snprintf(out, size, "%d:%02d", total / 60, total % 60);
}

void formatScore(char* out, size_t size, int score) {
    score = std::clamp(score, 0, 999999999);
    if (score >= 1000000)
        std::snprintf(out, size, "%d,%03d,%03d", score / 1000000, score / 1000 % 1000, score % 1000);
    else if (score >= 1000)
        std::snprintf(out, size, "%d,%03d", score / 1000, score % 1000);
    else
        std::snprintf(out, size, "%d", score);
}

void dimScreen(SDL_Renderer* r, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw::fillRect(r, 0, 0, kScreenWidth, kScreenHeight, SDL_Color{10, 8, 24, alpha});
}

} // namespace ui

} // namespace pd
