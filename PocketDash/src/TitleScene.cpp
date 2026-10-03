#include "TitleScene.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "PlayScene.h"

#include <cmath>
#include <cstdio>

namespace pd {

using draw::rgb;

TitleScene::TitleScene(Game& game) : Scene(game) {
    buildBackground(game.renderer());
    game.audio().playMusic(MusicTrack::Title);
}

void TitleScene::buildBackground(SDL_Renderer* r) {
    // Baked once into a texture: drawing it each frame is a single copy.
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, kScreenWidth, kScreenHeight, 32, SDL_PIXELFORMAT_RGBA32));
    if (!s) return;

    // Sky: banded vertical gradient (bands look deliberate in pixel art).
    const SDL_Color top = rgb(96, 170, 255);
    const SDL_Color bottom = rgb(190, 232, 255);
    for (int band = 0; band < 16; ++band) {
        const float t = band / 15.0f;
        const SDL_Color c = rgb(static_cast<Uint8>(top.r + (bottom.r - top.r) * t),
                                static_cast<Uint8>(top.g + (bottom.g - top.g) * t),
                                static_cast<Uint8>(top.b + (bottom.b - top.b) * t));
        draw::surfaceFillRect(s.get(), 0, band * 22, kScreenWidth, 22, c);
    }

    // Sun.
    draw::surfaceFillCircle(s.get(), 540, 70, 34, rgb(255, 236, 140));
    draw::surfaceFillCircle(s.get(), 540, 70, 26, rgb(255, 214, 64));

    // Rolling hills, back to front.
    for (int i = 0; i < 6; ++i) draw::surfaceFillCircle(s.get(), -40 + i * 140, 400, 120, rgb(86, 170, 96));
    for (int i = 0; i < 7; ++i) draw::surfaceFillCircle(s.get(), 30 + i * 110, 450, 110, rgb(112, 200, 88));
    draw::surfaceFillRect(s.get(), 0, 400, kScreenWidth, 80, rgb(112, 200, 88));
    draw::surfaceFillRect(s.get(), 0, 404, kScreenWidth, 4, rgb(140, 220, 110));

    // Flowers dotted over the meadow.
    const SDL_Color petals[] = {rgb(255, 255, 255), rgb(255, 214, 64), rgb(255, 130, 170)};
    for (int i = 0; i < 26; ++i) {
        const int x = (i * 97 + 31) % kScreenWidth;
        const int y = 420 + (i * 53) % 52;
        const SDL_Color c = petals[i % 3];
        draw::surfaceFillRect(s.get(), x - 2, y, 6, 2, c);
        draw::surfaceFillRect(s.get(), x, y - 2, 2, 6, c);
        draw::surfaceFillRect(s.get(), x, y, 2, 2, rgb(255, 170, 40));
    }

    background_.reset(SDL_CreateTextureFromSurface(r, s.get()));
}

void TitleScene::update(float dt) {
    time_ += dt;
    cloudScroll_ += dt * 12.0f;

    InputManager& in = game_.input();
    if (in.pressed(Action::A) || in.pressed(Action::Start)) {
        game_.audio().play(Sfx::MenuSelect);
        game_.changeScene(std::make_unique<PlayScene>(game_, game_.startLevel()));
    }
}

void TitleScene::render(SDL_Renderer* r) {
    if (background_) SDL_RenderCopy(r, background_.get(), nullptr, nullptr);

    // Drifting clouds (wrap around the screen).
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 4; ++i) {
        const int span = kScreenWidth + 160;
        const int x = static_cast<int>(std::fmod(cloudScroll_ * (1.0f + i * 0.3f) + i * 190.0f, static_cast<float>(span))) - 80;
        const int y = 40 + i * 38;
        const SDL_Color c{255, 255, 255, 230};
        draw::fillCircle(r, x, y, 18, c);
        draw::fillCircle(r, x + 22, y - 8, 22, c);
        draw::fillCircle(r, x + 46, y, 16, c);
        draw::fillRect(r, x, y, 46, 14, c);
    }

    const BitmapFont& font = game_.font();

    // Logo: each letter bobs on its own phase, with a thick outline.
    const char* title = "POCKET DASH";
    const int scale = 7;
    const int titleW = BitmapFont::textWidth(title, scale);
    int x = (kScreenWidth - titleW) / 2;
    const SDL_Color letterColors[] = {ui::kYellow, ui::kPink, ui::kSky, ui::kMint};
    for (int i = 0; title[i]; ++i) {
        const char glyph[2] = {title[i], '\0'};
        const int y = 70 + static_cast<int>(std::lround(std::sin(time_ * 3.0f + i * 0.55f) * 6.0f));
        for (int ox = -1; ox <= 1; ++ox)
            for (int oy = -1; oy <= 2; ++oy)
                if (ox || oy) font.draw(r, x + ox * 4, y + oy * 4, glyph, scale, ui::kInk);
        font.draw(r, x, y, glyph, scale, letterColors[i % 4]);
        x += BitmapFont::kCellW * scale;
    }

    font.drawCentered(r, kScreenWidth / 2, 150, "A POCKET-SIZED ADVENTURE", 2, ui::kWhite);

    // Hero hopping on the hill.
    if (SDL_Texture* sheet = game_.sprites().player()) {
        const float hop = std::fabs(std::sin(time_ * 4.0f)) * 18.0f;
        const SDL_Rect src{static_cast<int>(time_ * 6.0f) % 2 * Sprites::kPlayerFrameW, 0, Sprites::kPlayerFrameW,
                           Sprites::kPlayerFrameH};
        const int size = Sprites::kPlayerFrameW * 5;
        draw::fillEllipse(r, kScreenWidth / 2, 392, 30 - static_cast<int>(hop / 3), 7, SDL_Color{0, 0, 0, 70});
        const SDL_Rect dst{kScreenWidth / 2 - size / 2, 396 - size - static_cast<int>(hop), size, size};
        SDL_RenderCopy(r, sheet, &src, &dst);
    }

    // Blinking prompt.
    if (static_cast<int>(time_ * 2.0f) % 2 == 0)
        font.drawCentered(r, kScreenWidth / 2, 228, "PRESS A TO START", 3, ui::kWhite);

    char footer[64];
    std::snprintf(footer, sizeof(footer), "V%s  PHASE 1 BUILD", POCKETDASH_VERSION);
    font.drawShadowed(r, 8, kScreenHeight - 20, footer, 2, ui::kWhite);
}

} // namespace pd
