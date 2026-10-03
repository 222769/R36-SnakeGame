#include "Backdrop.h"

#include "Canvas.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>

namespace pd {

bool Backdrop::build(SDL_Renderer* r) {
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
    return background_ != nullptr;
}

void Backdrop::render(SDL_Renderer* r, float time) const {
    if (background_) SDL_RenderCopy(r, background_.get(), nullptr, nullptr);

    // Drifting clouds (wrap around the screen), smaller ones farther away.
    if (!cloud_) return;
    const float scroll = time * 12.0f;
    for (int i = 0; i < 4; ++i) {
        const int span = kScreenWidth + 200;
        const float size = 0.55f + 0.15f * static_cast<float>(i % 3);
        const int w = static_cast<int>(160 * size), h = static_cast<int>(80 * size);
        const int x = static_cast<int>(std::fmod(scroll * (1.0f + i * 0.3f) + i * 190.0f, static_cast<float>(span))) - 120;
        const SDL_Rect dst{x, 20 + i * 40, w, h};
        SDL_RenderCopy(r, cloud_.get(), nullptr, &dst);
    }
}

} // namespace pd
