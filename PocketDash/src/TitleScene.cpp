#include "TitleScene.h"

#include "Canvas.h"
#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "PlayScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {


TitleScene::TitleScene(Game& game) : Scene(game) {
    buildBackground(game.renderer());
    game.audio().playMusic(MusicTrack::Title);
}

void TitleScene::buildBackground(SDL_Renderer* r) {
    // Painted once with Canvas and baked into textures: drawing is one copy.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    Canvas c(kScreenWidth, kScreenHeight);
    const float W = static_cast<float>(kScreenWidth);

    // Sky: smooth gradient, warmer near the horizon.
    c.gradientRect(0, 0, W, 300, Col::rgb(70, 140, 214), Col::rgb(196, 226, 244));
    c.gradientRect(0, 300, W, 180, Col::rgb(196, 226, 244), Col::rgb(232, 238, 226));

    // Sun with a wide glow.
    c.softEllipse(540, 74, 120, 120, Col::rgb(255, 244, 200, 150));
    c.softEllipse(540, 74, 60, 60, Col::rgb(255, 250, 230, 200));
    c.fillCircle(540, 74, 28, Col::rgb(255, 246, 214));

    // Hills, back to front: each layer a noisy ridge line with soft shading;
    // farther layers are hazier (mixed towards the sky colour).
    struct Layer { float base, amp, freq, phase; Col col; float haze; };
    const Layer layers[] = {
        {300, 26, 0.008f, 1.3f, Col::rgb(92, 140, 104), 0.45f},
        {336, 30, 0.011f, 4.1f, Col::rgb(80, 138, 70), 0.22f},
        {380, 22, 0.015f, 2.2f, Col::rgb(96, 156, 64), 0.0f},
    };
    const Col haze = Col::rgb(200, 224, 236);
    uint32_t seed = 11;
    for (const Layer& L : layers) {
        const Col base = Col::mix(L.col, haze, L.haze);
        const uint32_t layerSeed = seed++;
        c.paint([&](int x, int y) {
            const float fx = static_cast<float>(x);
            const float ridge = L.base - L.amp * (0.6f * std::sin(fx * L.freq + L.phase) +
                                                  0.4f * std::sin(fx * L.freq * 2.3f + L.phase * 1.7f));
            const float d = static_cast<float>(y) + 0.5f - ridge;
            if (d < -1.0f) return Col{0, 0, 0, 0};
            const float cover = std::clamp(d + 0.5f, 0.0f, 1.0f);
            // Lighter on the crest, darker lower down, with a little texture.
            const float n = tileFbm(fx, static_cast<float>(y), 128, layerSeed, 3);
            const float shade = 1.08f - std::min(d, 90.0f) / 90.0f * 0.22f + (n - 0.5f) * 0.12f;
            return base.scaled(shade).withAlpha(cover);
        });
    }

    // Foreground meadow: textured grass with flowers.
    c.paint([&](int x, int y) {
        if (y < 404) return Col{0, 0, 0, 0};
        const float fx = static_cast<float>(x), fy = static_cast<float>(y);
        const float n = tileFbm(fx, fy * 1.6f, 64, 77, 4);
        const float depth = (fy - 404.0f) / 76.0f;
        const Col g = Col::mix(Col::rgb(116, 168, 70), Col::rgb(82, 132, 52), depth);
        return g.scaled(0.86f + n * 0.28f);
    });
    c.gradientRect(0, 403, W, 4, Col::rgb(150, 196, 96, 200), Col::rgb(150, 196, 96, 0));
    const Col petals[] = {Col::rgb(250, 250, 244), Col::rgb(248, 214, 92), Col::rgb(236, 150, 176)};
    for (int i = 0; i < 30; ++i) {
        const float x = static_cast<float>((i * 97 + 31) % kScreenWidth);
        const float y = 418.0f + static_cast<float>((i * 53) % 56);
        const float rr = 1.6f + (y - 418.0f) / 40.0f;
        for (int p = 0; p < 5; ++p) {
            const float a = static_cast<float>(p) * 1.2566f;
            c.fillCircle(x + std::cos(a) * rr, y + std::sin(a) * rr, rr * 0.8f, petals[i % 3]);
        }
        c.fillCircle(x, y, rr * 0.6f, Col::rgb(232, 160, 48));
    }

    if (SurfacePtr s = c.toSurface()) background_.reset(SDL_CreateTextureFromSurface(r, s.get()));

    // One fluffy cloud, reused at several sizes.
    Canvas cloud(160, 80);
    const float puffs[][3] = {{40, 52, 22}, {66, 38, 28}, {98, 34, 32}, {126, 50, 22}, {84, 56, 24}};
    for (const auto& p : puffs) cloud.softEllipse(p[0], p[1] + 6, p[2] + 6, p[2] * 0.6f, Col::rgb(90, 120, 160, 70));
    for (const auto& p : puffs) cloud.sphere(p[0], p[1], p[2], p[2], Col::rgb(250, 252, 255), 0.0f, 0.86f);
    cloud.fillRoundRect(36, 50, 94, 22, 11, Col::rgb(236, 242, 250));
    if (SurfacePtr s = cloud.toSurface()) {
        cloud_.reset(SDL_CreateTextureFromSurface(r, s.get()));
        if (cloud_) SDL_SetTextureBlendMode(cloud_.get(), SDL_BLENDMODE_BLEND);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
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

    // Drifting clouds (wrap around the screen), smaller ones farther away.
    if (cloud_) {
        for (int i = 0; i < 4; ++i) {
            const int span = kScreenWidth + 200;
            const float size = 0.55f + 0.15f * static_cast<float>(i % 3);
            const int w = static_cast<int>(160 * size), h = static_cast<int>(80 * size);
            const int x = static_cast<int>(std::fmod(cloudScroll_ * (1.0f + i * 0.3f) + i * 190.0f,
                                                     static_cast<float>(span))) - 120;
            const SDL_Rect dst{x, 20 + i * 40, w, h};
            SDL_RenderCopy(r, cloud_.get(), nullptr, &dst);
        }
    }

    const BitmapFont& font = game_.font();

    // Logo: each letter bobs on its own phase, with a thick outline.
    const char* title = "POCKET DASH";
    const int scale = 7;
    const int titleW = BitmapFont::textWidth(title, scale) + scale * 10;
    int x = (kScreenWidth - titleW) / 2;
    const SDL_Color letterColors[] = {ui::kYellow, ui::kPink, ui::kSky, ui::kMint};
    for (int i = 0; title[i]; ++i) {
        const char glyph[2] = {title[i], '\0'};
        const int y = 70 + static_cast<int>(std::lround(std::sin(time_ * 3.0f + i * 0.55f) * 6.0f));
        for (int ox = -1; ox <= 1; ++ox)
            for (int oy = -1; oy <= 2; ++oy)
                if (ox || oy) font.draw(r, x + ox * 3, y + oy * 3, glyph, scale, ui::kInk);
        font.draw(r, x, y, glyph, scale, letterColors[i % 4]);
        x += BitmapFont::textWidth(glyph, scale) + scale;
    }

    font.drawCentered(r, kScreenWidth / 2, 150, "A POCKET-SIZED ADVENTURE", 2, ui::kWhite);

    // Hero hopping on the hill.
    if (SDL_Texture* sheet = game_.sprites().heroLarge()) {
        const float hop = std::fabs(std::sin(time_ * 4.0f)) * 18.0f;
        constexpr int w = Sprites::kHeroLargeW;
        constexpr int h = Sprites::kHeroLargeH;
        const SDL_Rect src{static_cast<int>(time_ * 6.0f) % 2 * w, 0, w, h};
        draw::fillEllipse(r, kScreenWidth / 2, 392, 30 - static_cast<int>(hop / 3), 7, SDL_Color{0, 0, 0, 70});
        const SDL_Rect dst{kScreenWidth / 2 - w / 2, 396 - h - static_cast<int>(hop), w, h};
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
