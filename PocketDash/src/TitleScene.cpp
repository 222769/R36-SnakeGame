#include "TitleScene.h"

#include "CollectionScene.h"
#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "HighScoresScene.h"
#include "LevelLoader.h"
#include "LevelSelectScene.h"
#include "Menu.h"
#include "PlayScene.h"
#include "SettingsScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {

namespace {

constexpr const char* kItems[] = {"PLAY", "LEVEL SELECT", "COLLECTION", "HIGH SCORES", "SETTINGS", "QUIT"};
constexpr int kItemCount = 6;
constexpr float kSlideTime = 0.3f;

float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

} // namespace

TitleScene::TitleScene(Game& game, bool startInMenu) : Scene(game), menuOpen_(startInMenu), menuT_(startInMenu ? 1.0f : 0.0f) {
    game.audio().playMusic(MusicTrack::Title);
}

void TitleScene::update(float dt) {
    time_ += dt;
    InputManager& in = game_.input();
    if (!menuOpen_) {
        if (in.pressed(Action::A) || in.pressed(Action::Start)) {
            game_.audio().play(Sfx::MenuSelect);
            menuOpen_ = true;
        }
        return;
    }
    menuT_ = std::min(1.0f, menuT_ + dt / kSlideTime);
    menu::navigate(game_, index_, kItemCount);
    if (in.pressed(Action::A) || in.pressed(Action::Start)) {
        game_.audio().play(Sfx::MenuSelect);
        activate(index_);
    } else if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuMove);
        menuOpen_ = false;
        menuT_ = 0.0f;
    }
}

void TitleScene::activate(int item) {
    switch (item) {
    case 0: game_.changeScene(std::make_unique<PlayScene>(game_, game_.playLevel())); break;
    case 1: game_.changeScene(std::make_unique<LevelSelectScene>(game_)); break;
    case 2: game_.changeScene(std::make_unique<CollectionScene>(game_)); break;
    case 3: game_.changeScene(std::make_unique<HighScoresScene>(game_, std::string(), false)); break;
    case 4: game_.changeScene(std::make_unique<SettingsScene>(game_)); break;
    case 5: game_.quit(); break;
    default: break;
    }
}

void TitleScene::renderLogo(SDL_Renderer* r, int y) const {
    const BitmapFont& font = game_.font();
    // Logo: each letter bobs on its own phase, with a thick outline.
    const char* title = "POCKET DASH";
    const int scale = 7;
    const int titleW = BitmapFont::textWidth(title, scale) + scale * 10;
    int x = (kScreenWidth - titleW) / 2;
    const SDL_Color letterColors[] = {ui::kYellow, ui::kPink, ui::kSky, ui::kMint};
    for (int i = 0; title[i]; ++i) {
        const char glyph[2] = {title[i], '\0'};
        const int ly = y + static_cast<int>(std::lround(std::sin(time_ * 3.0f + i * 0.55f) * 6.0f));
        for (int ox = -1; ox <= 1; ++ox)
            for (int oy = -1; oy <= 2; ++oy)
                if (ox || oy) font.draw(r, x + ox * 3, ly + oy * 3, glyph, scale, ui::kInk);
        font.draw(r, x, ly, glyph, scale, letterColors[i % 4]);
        x += BitmapFont::textWidth(glyph, scale) + scale;
    }


}

void TitleScene::renderStats(SDL_Renderer* r) const {
    // Progress at a glance: levels cleared, stars and gems found.
    const Progress& p = game_.progress();
    const auto& ids = levels::worldLevelIds(1);
    char text[64];
    std::snprintf(text, sizeof(text), "CLEARED %d/%d", p.levelsCleared(), static_cast<int>(ids.size()));
    const BitmapFont& font = game_.font();
    const int x = 24;
    const int y = 366;
    draw::roundedRect(r, SDL_Rect{x, y, 220, 64}, SDL_Color{24, 30, 50, 170}, SDL_Color{0, 0, 0, 0},
                      SDL_Color{0, 0, 0, 50});
    font.draw(r, x + 14, y + 8, text, 2, ui::kWhite);
    SDL_Texture* items = game_.sprites().items();
    constexpr int icon = Sprites::kItemSize;
    const SDL_Rect starSrc{Sprites::kItemStar * icon, 0, icon, icon};
    const SDL_Rect gemSrc{Sprites::kItemGem * icon, 0, icon, icon};
    SDL_Rect dst{x + 10, y + 34, 22, 22};
    if (items) SDL_RenderCopy(r, items, &starSrc, &dst);
    std::snprintf(text, sizeof(text), "%d", p.totalStars());
    font.draw(r, x + 38, y + 37, text, 2, ui::kYellow);
    dst.x = x + 100;
    if (items) SDL_RenderCopy(r, items, &gemSrc, &dst);
    std::snprintf(text, sizeof(text), "%d", p.totalGems());
    font.draw(r, x + 128, y + 37, text, 2, ui::kMint);
}

void TitleScene::render(SDL_Renderer* r) {
    game_.backdrop().render(r, time_);
    const BitmapFont& font = game_.font();
    const float m = menuOpen_ ? easeOut(menuT_) : 0.0f;

    renderLogo(r, static_cast<int>(std::lround(70.0f - 40.0f * m)));
    if (!menuOpen_) font.drawCentered(r, kScreenWidth / 2, 150, "A POCKET-SIZED ADVENTURE", 2, ui::kWhite);

    // Hero hopping on the hill; steps aside when the menu opens.
    if (SDL_Texture* sheet = game_.sprites().heroLarge()) {
        const float hop = std::fabs(std::sin(time_ * 4.0f)) * 18.0f;
        constexpr int w = Sprites::kHeroLargeW;
        constexpr int h = Sprites::kHeroLargeH;
        const int cx = static_cast<int>(std::lround(kScreenWidth / 2 - 190.0f * m));
        const int groundY = static_cast<int>(std::lround(392.0f - 50.0f * m));
        const SDL_Rect src{static_cast<int>(time_ * 6.0f) % 2 * w, 0, w, h};
        draw::softEllipse(r, cx, groundY, 34 - static_cast<int>(hop / 3), 9, SDL_Color{0, 0, 0, 110});
        const SDL_Rect dst{cx - w / 2, groundY + 4 - h - static_cast<int>(hop), w, h};
        SDL_RenderCopy(r, sheet, &src, &dst);
    }

    if (!menuOpen_) {
        if (static_cast<int>(time_ * 2.0f) % 2 == 0)
            font.drawCentered(r, kScreenWidth / 2, 228, "PRESS A TO START", 3, ui::kWhite);
    } else {
        // Menu slides in from the right.
        const int x = static_cast<int>(std::lround(300.0f + (1.0f - m) * 360.0f));
        for (int i = 0; i < kItemCount; ++i)
            menu::drawRow(r, font, SDL_Rect{x, 132 + i * 44, 300, 36}, kItems[i], {}, i == index_);
        renderStats(r);
        menu::drawHints(r, font, "A SELECT    B BACK");
    }

    char footer[32];
    std::snprintf(footer, sizeof(footer), "V%s", POCKETDASH_VERSION);
    font.drawShadowed(r, kScreenWidth - BitmapFont::textWidth(footer, 1) - 8, kScreenHeight - 14, footer, 1, ui::kWhite);
}

} // namespace pd
