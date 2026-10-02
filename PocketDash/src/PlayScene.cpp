#include "PlayScene.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "TitleScene.h"
#include "World.h"

#include <cstdint>

namespace pd {

using draw::rgb;

namespace {

constexpr int kHudHeight = 64;
constexpr int kBorder = 32;
constexpr const char* kPauseItems[] = {"RESUME", "RESTART", "QUIT TO TITLE"};
constexpr int kPauseItemCount = 3;

// Tiny deterministic PRNG so the baked background is the same every run.
struct Lcg {
    uint32_t state;
    uint32_t next() {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }
    int range(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

} // namespace

PlayScene::PlayScene(Game& game)
    : Scene(game),
      arena_{static_cast<float>(kBorder), static_cast<float>(kHudHeight),
             static_cast<float>(kScreenWidth - kBorder * 2), static_cast<float>(kScreenHeight - kHudHeight - kBorder)} {
    buildBackground(game.renderer());
    restart();
    game.audio().playMusic(MusicTrack::Meadow);
}

void PlayScene::restart() {
    player_ = Player(arena_.center());
    hearts_ = maxHearts_;
    paused_ = false;
    infoOpen_ = false;
}

void PlayScene::buildBackground(SDL_Renderer* r) {
    const WorldTheme& theme = worldDef(WorldId::SunnyMeadows).theme;
    SurfacePtr s(SDL_CreateRGBSurfaceWithFormat(0, kScreenWidth, kScreenHeight, 32, SDL_PIXELFORMAT_RGBA32));
    if (!s) return;
    Lcg rng{1234u};

    // Checkered grass tiles with scattered blades.
    for (int ty = 0; ty < kScreenHeight / kTileSize; ++ty) {
        for (int tx = 0; tx < kScreenWidth / kTileSize; ++tx) {
            const SDL_Color base = ((tx + ty) % 2) ? theme.ground : theme.groundAlt;
            draw::surfaceFillRect(s.get(), tx * kTileSize, ty * kTileSize, kTileSize, kTileSize, base);
            for (int i = 0; i < 3; ++i) {
                const int bx = tx * kTileSize + rng.range(kTileSize - 4);
                const int by = ty * kTileSize + rng.range(kTileSize - 6);
                draw::surfaceFillRect(s.get(), bx, by, 2, 4, rgb(76, 160, 70));
                draw::surfaceFillRect(s.get(), bx + 2, by + 2, 2, 2, rgb(76, 160, 70));
            }
        }
    }

    // Flowers.
    const SDL_Color petals[] = {rgb(255, 255, 255), rgb(255, 214, 64), rgb(255, 130, 170), rgb(170, 140, 255)};
    for (int i = 0; i < 40; ++i) {
        const int x = kBorder + 8 + rng.range(kScreenWidth - kBorder * 2 - 16);
        const int y = kHudHeight + 8 + rng.range(kScreenHeight - kHudHeight - kBorder - 16);
        const SDL_Color c = petals[rng.range(4)];
        draw::surfaceFillRect(s.get(), x - 2, y, 6, 2, c);
        draw::surfaceFillRect(s.get(), x, y - 2, 2, 6, c);
        draw::surfaceFillRect(s.get(), x, y, 2, 2, rgb(255, 170, 40));
    }

    // Hedge border: a band of bushes all around the arena.
    auto hedgeBand = [&](int x, int y, int w, int h) {
        draw::surfaceFillRect(s.get(), x, y, w, h, theme.wallShade);
        for (int by = y + 8; by < y + h; by += 20)
            for (int bx = x + 8 + (by / 20 % 2) * 10; bx < x + w; bx += 20) {
                draw::surfaceFillCircle(s.get(), bx, by, 12, theme.wall);
                draw::surfaceFillCircle(s.get(), bx - 3, by - 4, 5, rgb(96, 176, 84));
            }
    };
    hedgeBand(0, 0, kScreenWidth, kHudHeight);
    hedgeBand(0, kScreenHeight - kBorder, kScreenWidth, kBorder);
    hedgeBand(0, kHudHeight, kBorder, kScreenHeight - kHudHeight - kBorder);
    hedgeBand(kScreenWidth - kBorder, kHudHeight, kBorder, kScreenHeight - kHudHeight - kBorder);

    // Soft shadow where the hedge meets the grass.
    draw::surfaceFillRect(s.get(), kBorder, kHudHeight, kScreenWidth - kBorder * 2, 4, rgb(80, 150, 66));

    background_.reset(SDL_CreateTextureFromSurface(r, s.get()));
}

void PlayScene::update(float dt) {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();

    if (infoOpen_) {
        if (in.pressed(Action::Select) || in.pressed(Action::B) || in.pressed(Action::A) || in.pressed(Action::Start)) {
            infoOpen_ = false;
            audio.play(Sfx::MenuMove);
        }
        return;
    }
    if (paused_) {
        updatePauseMenu();
        return;
    }
    if (in.pressed(Action::Start)) {
        paused_ = true;
        pauseIndex_ = 0;
        audio.play(Sfx::Pause);
        return;
    }
    if (in.pressed(Action::Select)) {
        infoOpen_ = true;
        audio.play(Sfx::MenuMove);
        return;
    }

    time_ += dt;

    PlayerInput pin;
    pin.move = in.moveVector();
    pin.hopPressed = in.pressed(Action::A);
    pin.dashPressed = in.pressed(Action::B);

    const unsigned events = player_.update(pin, dt);
    player_.constrainTo(arena_);

    if (events & kEventHopped) audio.play(Sfx::Jump);
    if (events & kEventDashed) audio.play(Sfx::Dash);
}

void PlayScene::updatePauseMenu() {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();

    if (in.pressed(Action::Start) || in.pressed(Action::B)) {
        paused_ = false;
        audio.play(Sfx::Pause);
        return;
    }
    if (in.pressed(Action::Up)) {
        pauseIndex_ = (pauseIndex_ + kPauseItemCount - 1) % kPauseItemCount;
        audio.play(Sfx::MenuMove);
    }
    if (in.pressed(Action::Down)) {
        pauseIndex_ = (pauseIndex_ + 1) % kPauseItemCount;
        audio.play(Sfx::MenuMove);
    }
    if (in.pressed(Action::A)) {
        audio.play(Sfx::MenuSelect);
        switch (pauseIndex_) {
        case 0: paused_ = false; break;
        case 1: restart(); break;
        case 2: game_.changeScene(std::make_unique<TitleScene>(game_)); break;
        default: break;
        }
    }
}

void PlayScene::render(SDL_Renderer* r) {
    if (background_) SDL_RenderCopy(r, background_.get(), nullptr, nullptr);
    player_.render(r, game_.sprites().player(), Vec2{});
    renderHud(r);
    if (infoOpen_) renderInfoPanel(r);
    if (paused_) renderPauseMenu(r);
}

void PlayScene::renderHud(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();

    // Hearts (9x8 art at 3x).
    for (int i = 0; i < maxHearts_; ++i) {
        SDL_Texture* tex = i < hearts_ ? game_.sprites().heartFull() : game_.sprites().heartEmpty();
        const SDL_Rect dst{14 + i * 32, 18, Sprites::kHeartW * 3, Sprites::kHeartH * 3};
        SDL_RenderCopy(r, tex, nullptr, &dst);
    }

    font.drawCentered(r, kScreenWidth / 2, 22, "1-0 SANDBOX", 3, ui::kYellow);

    // Button hints along the bottom hedge.
    font.drawCentered(r, kScreenWidth / 2, kScreenHeight - 24, "A HOP  B DASH  START PAUSE  SELECT INFO", 2,
                      ui::kWhite);
}

void PlayScene::renderPauseMenu(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    ui::dimScreen(r, 140);
    const SDL_Rect panel{kScreenWidth / 2 - 150, 130, 300, 210};
    ui::drawPanel(r, panel);
    font.drawCentered(r, kScreenWidth / 2, 152, "PAUSED", 4, ui::kYellow);
    for (int i = 0; i < kPauseItemCount; ++i) {
        const bool selected = i == pauseIndex_;
        const int y = 220 + i * 36;
        font.drawCentered(r, kScreenWidth / 2, y, kPauseItems[i], 3, selected ? ui::kWhite : ui::kGrey);
        if (selected) {
            const int w = BitmapFont::textWidth(kPauseItems[i], 3);
            font.draw(r, kScreenWidth / 2 - w / 2 - 30, y, ">", 3, ui::kPink);
        }
    }
}

void PlayScene::renderInfoPanel(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    ui::dimScreen(r, 120);
    const SDL_Rect panel{70, 90, kScreenWidth - 140, 300};
    ui::drawPanel(r, panel);
    font.drawCentered(r, kScreenWidth / 2, 108, "1-0 SANDBOX", 3, ui::kYellow);
    font.drawCentered(r, kScreenWidth / 2, 142, worldDef(WorldId::SunnyMeadows).name, 2, ui::kMint);

    const char* lines[] = {
        "GOAL: TRY OUT YOUR MOVES!",
        "",
        "D-PAD / STICK   MOVE",
        "A               HOP",
        "B               DASH",
        "START           PAUSE",
        "SELECT+START    QUIT",
    };
    int y = 180;
    for (const char* line : lines) {
        font.drawShadowed(r, 104, y, line, 2, ui::kWhite);
        y += 24;
    }
    font.drawCentered(r, kScreenWidth / 2, 360, "PRESS SELECT TO CLOSE", 2, ui::kGrey);
}

void PlayScene::fillDebugInfo(DebugInfo& info) const {
    info.levelId = "1-0";
    info.hasPlayer = true;
    info.playerPos = player_.position();
    info.playerVel = player_.velocity();
    info.playerZ = player_.height();
    info.enemyCount = 0;
}

void PlayScene::renderDebug(SDL_Renderer* r) const {
    draw::rectOutline(r, arena_, SDL_Color{255, 220, 0, 255});
    draw::rectOutline(r, player_.hitbox(), player_.isInvincible() ? SDL_Color{255, 80, 80, 255}
                                                                   : SDL_Color{0, 255, 120, 255});
    const Vec2 p = player_.position();
    draw::fillRect(r, static_cast<int>(p.x) - 1, static_cast<int>(p.y) - 1, 3, 3, SDL_Color{255, 0, 255, 255});
}

} // namespace pd
