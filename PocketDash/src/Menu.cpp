#include "Menu.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "UI.h"

#include <algorithm>

namespace pd::menu {

bool navigate(Game& game, int& index, int count, bool horizontal, bool wrap) {
    if (count <= 0) return false;
    InputManager& in = game.input();
    const Action prev = horizontal ? Action::Left : Action::Up;
    const Action next = horizontal ? Action::Right : Action::Down;
    int moved = index;
    if (in.repeated(prev)) moved = wrap ? (index + count - 1) % count : std::max(0, index - 1);
    else if (in.repeated(next)) moved = wrap ? (index + 1) % count : std::min(count - 1, index + 1);
    if (moved == index) return false;
    index = moved;
    game.audio().play(Sfx::MenuMove);
    return true;
}

int adjust(Game& game) {
    InputManager& in = game.input();
    if (in.repeated(Action::Left)) return -1;
    if (in.repeated(Action::Right)) return 1;
    return 0;
}

void drawHeader(SDL_Renderer* r, const BitmapFont& font, std::string_view title) {
    const int w = BitmapFont::textWidth(title, 4) + 48;
    draw::roundedRect(r, SDL_Rect{kScreenWidth / 2 - w / 2, 14, w, 50}, SDL_Color{30, 36, 58, 210},
                      SDL_Color{255, 255, 255, 200}, SDL_Color{0, 0, 0, 90});
    font.drawCentered(r, kScreenWidth / 2, 24, title, 4, ui::kYellow);
}

void drawHints(SDL_Renderer* r, const BitmapFont& font, std::string_view hints) {
    const int w = BitmapFont::textWidth(hints, 2) + 32;
    draw::roundedRect(r, SDL_Rect{kScreenWidth / 2 - w / 2, kScreenHeight - 38, w, 28}, SDL_Color{20, 24, 40, 190},
                      SDL_Color{0, 0, 0, 0}, SDL_Color{0, 0, 0, 60});
    font.drawCentered(r, kScreenWidth / 2, kScreenHeight - 33, hints, 2, ui::kWhite, false);
}

void drawRow(SDL_Renderer* r, const BitmapFont& font, const SDL_Rect& rect, std::string_view label,
             std::string_view value, bool selected, bool enabled) {
    const SDL_Color fill = selected ? SDL_Color{255, 250, 236, 240} : SDL_Color{24, 30, 50, 170};
    const SDL_Color border = selected ? SDL_Color{246, 196, 62, 255} : SDL_Color{0, 0, 0, 0};
    draw::roundedRect(r, rect, fill, border, SDL_Color{0, 0, 0, static_cast<Uint8>(selected ? 90 : 40)});
    SDL_Color ink = selected ? ui::kInk : ui::kWhite;
    if (!enabled) ink = selected ? SDL_Color{120, 116, 130, 255} : ui::kGrey;
    const int ty = rect.y + (rect.h - BitmapFont::lineHeight(2)) / 2 + 1;
    if (value.empty()) {
        font.draw(r, rect.x + rect.w / 2 - BitmapFont::textWidth(label, 2) / 2, ty, label, 2, ink);
        return;
    }
    font.draw(r, rect.x + 16, ty, label, 2, ink);
    const int vw = BitmapFont::textWidth(value, 2);
    const int vx = rect.x + rect.w - 16 - vw - (selected ? 18 : 0);
    font.draw(r, vx, ty, value, 2, selected ? SDL_Color{176, 110, 20, 255} : ui::kYellow);
    if (selected) {
        font.draw(r, vx - 18, ty, "<", 2, ui::kInk);
        font.draw(r, rect.x + rect.w - 26, ty, ">", 2, ui::kInk);
    }
}

void drawBarRow(SDL_Renderer* r, const BitmapFont& font, const SDL_Rect& rect, std::string_view label, int level,
                int max, bool selected) {
    const SDL_Color fill = selected ? SDL_Color{255, 250, 236, 240} : SDL_Color{24, 30, 50, 170};
    const SDL_Color border = selected ? SDL_Color{246, 196, 62, 255} : SDL_Color{0, 0, 0, 0};
    draw::roundedRect(r, rect, fill, border, SDL_Color{0, 0, 0, static_cast<Uint8>(selected ? 90 : 40)});
    const int ty = rect.y + (rect.h - BitmapFont::lineHeight(2)) / 2 + 1;
    font.draw(r, rect.x + 16, ty, label, 2, selected ? ui::kInk : ui::kWhite);

    constexpr int segW = 9, gap = 3;
    const int barW = max * (segW + gap) - gap;
    const int x0 = rect.x + rect.w - (selected ? 40 : 18) - barW;
    if (selected) {
        font.draw(r, x0 - 20, ty, "<", 2, ui::kInk);
        font.draw(r, rect.x + rect.w - 26, ty, ">", 2, ui::kInk);
    }
    const int h = rect.h - 16;
    for (int i = 0; i < max; ++i) {
        const bool on = i < level;
        SDL_Color c = on ? (selected ? SDL_Color{226, 150, 30, 255} : ui::kYellow) : SDL_Color{128, 128, 140, 120};
        const int segH = 6 + (h - 6) * (i + 1) / max; // rising steps
        draw::roundedRect(r, SDL_Rect{x0 + i * (segW + gap), rect.y + 8 + h - segH, segW, segH}, c, SDL_Color{0, 0, 0, 0});
    }
}

} // namespace pd::menu
