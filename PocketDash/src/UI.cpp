#include "UI.h"

#include "Constants.h"
#include "Draw.h"

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

} // namespace

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

bool BitmapFont::create(SDL_Renderer* renderer) {
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
    return static_cast<int>(text.size()) * kCellW * scale - scale;
}

void BitmapFont::draw(SDL_Renderer* r, int x, int y, std::string_view text, int scale, SDL_Color color) const {
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
    draw(r, x + scale, y + scale, text, scale, shadow);
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
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Fill, with the corners notched off for a softer, rounded look.
    draw::fillRect(r, rect.x + 2, rect.y, rect.w - 4, rect.h, fill);
    draw::fillRect(r, rect.x, rect.y + 2, 2, rect.h - 4, fill);
    draw::fillRect(r, rect.x + rect.w - 2, rect.y + 2, 2, rect.h - 4, fill);
    // Border.
    draw::fillRect(r, rect.x + 4, rect.y, rect.w - 8, 2, border);
    draw::fillRect(r, rect.x + 4, rect.y + rect.h - 2, rect.w - 8, 2, border);
    draw::fillRect(r, rect.x, rect.y + 4, 2, rect.h - 8, border);
    draw::fillRect(r, rect.x + rect.w - 2, rect.y + 4, 2, rect.h - 8, border);
    draw::fillRect(r, rect.x + 2, rect.y + 2, 2, 2, border);
    draw::fillRect(r, rect.x + rect.w - 4, rect.y + 2, 2, 2, border);
    draw::fillRect(r, rect.x + 2, rect.y + rect.h - 4, 2, 2, border);
    draw::fillRect(r, rect.x + rect.w - 4, rect.y + rect.h - 4, 2, 2, border);
}

void dimScreen(SDL_Renderer* r, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    draw::fillRect(r, 0, 0, kScreenWidth, kScreenHeight, SDL_Color{10, 8, 24, alpha});
}

} // namespace ui

} // namespace pd
