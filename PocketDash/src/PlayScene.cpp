#include "PlayScene.h"

#include "BuiltinLevels.h"
#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "TitleScene.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace pd {

namespace {

constexpr const char* kPauseItems[] = {"RESUME", "RESTART", "QUIT TO TITLE"};
constexpr int kPauseItemCount = 3;
constexpr float kBannerTime = 2.6f;
// Height of the HUD strip; the camera may scroll this far above the map.
constexpr float kHudMargin = 40.0f;
constexpr SDL_Color kDustColor{245, 240, 225, 200};

// "1:05.3" style clock.
void formatTime(char* out, size_t size, float seconds, bool tenths) {
    seconds = std::clamp(seconds, 0.0f, 99.0f * 60.0f + 59.9f); // keep the HUD short
    const int total = static_cast<int>(seconds);
    if (tenths)
        std::snprintf(out, size, "%d:%02d.%d", total / 60, total % 60, static_cast<int>(seconds * 10.0f) % 10);
    else
        std::snprintf(out, size, "%d:%02d", total / 60, total % 60);
}

} // namespace

PlayScene::PlayScene(Game& game) : Scene(game), camera_(kScreenWidth, kScreenHeight) {
    std::string error;
    if (!makeTestLevel(level_, &error)) throw std::runtime_error("Built-in level is invalid: " + error);

    const WorldDef& world = worldDef(level_.world);
    if (!tiles_.build(game.renderer(), world.theme)) throw std::runtime_error("Failed to build tile set");
    tiles_.prepare(level_);
    camera_.setBounds(level_.pixelWidth(), level_.pixelHeight(), kHudMargin);

    restart();
    game.audio().playMusic(MusicTrack::Meadow);
}

void PlayScene::restart() {
    player_ = Player(level_.spawn);
    coins_.reset(level_.coins);
    effects_.clear();
    camera_.setShakeEnabled(game_.settings().screenShake);
    camera_.snapTo(player_.position());
    hearts_ = maxHearts_;
    levelTime_ = 0.0f;
    stateTime_ = 0.0f;
    state_ = State::Playing;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void PlayScene::update(float dt) {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();
    stateTime_ += dt;

    switch (state_) {
    case State::Info:
        if (in.pressed(Action::Select) || in.pressed(Action::B) || in.pressed(Action::A) || in.pressed(Action::Start)) {
            state_ = State::Playing;
            audio.play(Sfx::MenuMove);
        }
        return;
    case State::Paused:
        updatePauseMenu();
        return;
    case State::Clear:
        updateClear(dt);
        return;
    case State::Playing:
        break;
    }

    if (in.pressed(Action::Start)) {
        state_ = State::Paused;
        pauseIndex_ = 0;
        audio.play(Sfx::Pause);
        return;
    }
    if (in.pressed(Action::Select)) {
        state_ = State::Info;
        audio.play(Sfx::MenuMove);
        return;
    }
    if (game_.debugEnabled() && in.pressed(Action::R1)) debugWarpToExit();
    updatePlaying(dt);
}

void PlayScene::updatePlaying(float dt) {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();
    levelTime_ += dt;
    animTime_ += dt;

    PlayerInput pin;
    pin.move = in.moveVector();
    pin.hopPressed = in.pressed(Action::A);
    pin.dashPressed = in.pressed(Action::B);

    const unsigned events = player_.update(pin, dt, &level_);
    if (events & kEventHopped) audio.play(Sfx::Jump);
    if (events & kEventDashed) {
        audio.play(Sfx::Dash);
        effects_.spawn(Effects::Type::Dust, player_.position(), kDustColor);
    }
    if (events & kEventLanded) effects_.spawn(Effects::Type::Dust, player_.position(), kDustColor);

    // Coins are collected around the body, not the feet.
    if (coins_.collect(player_.position() - Vec2{0.0f, 10.0f}, &effects_) > 0) audio.play(Sfx::Coin);

    effects_.update(dt);
    camera_.follow(player_.position() - Vec2{0.0f, 12.0f}, player_.velocity() * 0.18f, dt);

    if (level_.overlapsTile(player_.hitbox(), Tile::Exit)) {
        state_ = State::Clear;
        stateTime_ = 0.0f;
        player_.stop();
        audio.play(Sfx::LevelComplete);
        for (int i = 0; i < 3; ++i)
            effects_.spawn(Effects::Type::Sparkle, level_.exit + Vec2{(i - 1) * 14.0f, -10.0f - i * 6.0f},
                           SDL_Color{255, 214, 64, 255});
    }
}

void PlayScene::debugWarpToExit() {
    // Debug tool: drop the player on the first free tile near the exit.
    const int ex = static_cast<int>(level_.exit.x) / kTileSize;
    const int ey = static_cast<int>(level_.exit.y) / kTileSize;
    const int offsets[][2] = {{0, 2}, {0, 1}, {-2, 0}, {2, 0}, {-1, 0}, {1, 0}, {0, -1}};
    for (const auto& o : offsets) {
        Player probe(Level::tileCenter(ex + o[0], ey + o[1]) + Vec2{0.0f, 6.0f});
        if (level_.overlapsSolid(probe.hitbox()) || level_.overlapsTile(probe.hitbox(), Tile::Exit)) continue;
        player_.setPosition(probe.position());
        player_.stop();
        camera_.snapTo(player_.position());
        SDL_Log("[debug] Warped next to the exit");
        return;
    }
}

void PlayScene::updateClear(float dt) {
    animTime_ += dt;
    effects_.update(dt);

    // Victory hops while the results are shown.
    PlayerInput celebrate;
    celebrate.hopPressed = !player_.isAirborne();
    player_.update(celebrate, dt, &level_);
    // Frame the hero in the upper half, above the results panel.
    camera_.follow(player_.position() + Vec2{0.0f, 110.0f}, {}, dt);

    if (stateTime_ < 0.8f) return; // ignore inputs that were meant for gameplay
    InputManager& in = game_.input();
    if (in.pressed(Action::A) || in.pressed(Action::Start)) {
        game_.audio().play(Sfx::MenuSelect);
        restart();
    } else if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuSelect);
        game_.changeScene(std::make_unique<TitleScene>(game_));
    }
}

void PlayScene::updatePauseMenu() {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();

    if (in.pressed(Action::Start) || in.pressed(Action::B)) {
        state_ = State::Playing;
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
        case 0: state_ = State::Playing; break;
        case 1: restart(); break;
        case 2: game_.changeScene(std::make_unique<TitleScene>(game_)); break;
        default: break;
        }
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void PlayScene::render(SDL_Renderer* r) {
    const Vec2 cam = camera_.position();
    tiles_.render(r, cam, animTime_);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    coins_.render(r, game_.sprites().coin(), cam, animTime_);
    player_.render(r, game_.sprites().player(), cam);
    effects_.render(r, cam);

    renderHud(r);
    if (levelTime_ < kBannerTime && state_ == State::Playing) renderBanner(r);

    switch (state_) {
    case State::Info: renderInfoPanel(r); break;
    case State::Paused: renderPauseMenu(r); break;
    case State::Clear: renderClearPanel(r); break;
    case State::Playing: break;
    }
}

void PlayScene::renderHud(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();

    // Hearts (9x8 art at 3x) on a soft backing so they read on any tile.
    ui::drawPanel(r, SDL_Rect{6, 8, maxHearts_ * 32 + 10, 38}, SDL_Color{20, 16, 40, 140}, SDL_Color{0, 0, 0, 0});
    for (int i = 0; i < maxHearts_; ++i) {
        SDL_Texture* tex = i < hearts_ ? game_.sprites().heartFull() : game_.sprites().heartEmpty();
        const SDL_Rect dst{14 + i * 32, 15, Sprites::kHeartW * 3, Sprites::kHeartH * 3};
        SDL_RenderCopy(r, tex, nullptr, &dst);
    }

    // Coin counter and clock, top right.
    char coinText[16];
    std::snprintf(coinText, sizeof(coinText), "%d/%d", coins_.collected(), coins_.total());
    const int textW = BitmapFont::textWidth(coinText, 3);
    const int panelW = textW + 52;
    const int panelX = kScreenWidth - panelW - 6;
    ui::drawPanel(r, SDL_Rect{panelX, 8, panelW, 38}, SDL_Color{20, 16, 40, 140}, SDL_Color{0, 0, 0, 0});
    if (SDL_Texture* coin = game_.sprites().coin()) {
        const SDL_Rect src{0, 0, Sprites::kCoinSize, Sprites::kCoinSize};
        const SDL_Rect dst{panelX + 10, 15, Sprites::kCoinSize * 2, Sprites::kCoinSize * 2};
        SDL_RenderCopy(r, coin, &src, &dst);
    }
    font.drawShadowed(r, panelX + 42, 16, coinText, 3, ui::kYellow);

    char clock[24];
    formatTime(clock, sizeof(clock), levelTime_, false);
    font.drawShadowed(r, kScreenWidth - BitmapFont::textWidth(clock, 2) - 12, 52, clock, 2, ui::kWhite);
}

void PlayScene::renderBanner(SDL_Renderer* r) const {
    // Shown on whichever half of the screen the hero is not in, so it never
    // hides them. Slides in from that edge, holds, then slides back out.
    const float heroScreenY = player_.position().y - camera_.position().y;
    const bool atBottom = heroScreenY < kScreenHeight * 0.5f;
    constexpr int kH = 62;
    const float rest = atBottom ? static_cast<float>(kScreenHeight - kH - 20) : 64.0f;
    const float hidden = atBottom ? static_cast<float>(kScreenHeight + 4) : -static_cast<float>(kH + 4);

    const float t = levelTime_;
    float slide = 1.0f; // 1 = fully shown
    if (t < 0.3f) slide = t / 0.3f;
    else if (t > kBannerTime - 0.3f) slide = (kBannerTime - t) / 0.3f;
    const int iy = static_cast<int>(hidden + (rest - hidden) * std::clamp(slide, 0.0f, 1.0f));

    char title[64];
    std::snprintf(title, sizeof(title), "%s  %s", level_.id.c_str(), level_.name.c_str());
    const BitmapFont& font = game_.font();
    const int w = std::max(BitmapFont::textWidth(title, 3), BitmapFont::textWidth("REACH THE FLAG!", 2)) + 40;
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - w / 2, iy, w, kH});
    font.drawCentered(r, kScreenWidth / 2, iy + 10, title, 3, ui::kYellow);
    font.drawCentered(r, kScreenWidth / 2, iy + 38, "REACH THE FLAG!", 2, ui::kWhite);
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
    const SDL_Rect panel{70, 70, kScreenWidth - 140, 340};
    ui::drawPanel(r, panel);

    char title[64];
    std::snprintf(title, sizeof(title), "%s  %s", level_.id.c_str(), level_.name.c_str());
    font.drawCentered(r, kScreenWidth / 2, 88, title, 3, ui::kYellow);
    font.drawCentered(r, kScreenWidth / 2, 122, worldDef(level_.world).name, 2, ui::kMint);

    char coins[48];
    std::snprintf(coins, sizeof(coins), "COINS: %d / %d", coins_.collected(), coins_.total());
    char clock[24];
    formatTime(clock, sizeof(clock), levelTime_, false);
    char time[32];
    std::snprintf(time, sizeof(time), "TIME:  %s", clock);

    const char* lines[] = {
        "GOAL: REACH THE FLAG!",
        coins,
        time,
        "",
        "D-PAD / STICK   MOVE",
        "A               HOP",
        "B               DASH",
        "START           PAUSE",
        "SELECT+START    QUIT",
    };
    int y = 158;
    for (const char* line : lines) {
        font.drawShadowed(r, 104, y, line, 2, ui::kWhite);
        y += 24;
    }
    font.drawCentered(r, kScreenWidth / 2, 382, "PRESS SELECT TO CLOSE", 2, ui::kGrey);
}

void PlayScene::renderClearPanel(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    // The panel sits in the lower part of the screen so the hero's victory
    // hops (centre of the view) stay visible. It grows open over 0.25 s.
    constexpr int kTop = 250;
    constexpr int kHeight = 216;
    const float grow = std::min(1.0f, stateTime_ / 0.25f);
    const int h = static_cast<int>(kHeight * grow);
    ui::dimScreen(r, static_cast<Uint8>(60 * grow));
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - 190, kTop + (kHeight - h) / 2, 380, h});
    if (grow < 1.0f) return;

    const int bounce = static_cast<int>(std::lround(std::fabs(std::sin(stateTime_ * 4.0f)) * -6.0f));
    font.drawCentered(r, kScreenWidth / 2, kTop + 18 + bounce, "LEVEL CLEAR!", 4, ui::kYellow);

    char coins[32];
    std::snprintf(coins, sizeof(coins), "COINS  %d/%d", coins_.collected(), coins_.total());
    font.drawCentered(r, kScreenWidth / 2, kTop + 70, coins, 3, ui::kWhite);
    char clock[24];
    formatTime(clock, sizeof(clock), levelTime_, true);
    char time[40];
    std::snprintf(time, sizeof(time), "TIME  %s", clock);
    font.drawCentered(r, kScreenWidth / 2, kTop + 102, time, 3, ui::kWhite);
    if (coins_.collected() == coins_.total())
        font.drawCentered(r, kScreenWidth / 2, kTop + 134, "ALL COINS!", 2, ui::kMint);

    if (stateTime_ >= 0.8f) {
        font.drawCentered(r, kScreenWidth / 2, kTop + 160, "A  PLAY AGAIN", 2, ui::kWhite);
        font.drawCentered(r, kScreenWidth / 2, kTop + 184, "B  TITLE", 2, ui::kGrey);
    }
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void PlayScene::fillDebugInfo(DebugInfo& info) const {
    info.levelId = level_.id.c_str();
    info.hasPlayer = true;
    info.playerPos = player_.position();
    info.playerVel = player_.velocity();
    info.playerZ = player_.height();
    info.enemyCount = 0;
    info.coins = coins_.collected();
    info.coinsTotal = coins_.total();
    info.hasCamera = true;
    info.camera = camera_.rawPosition();
    info.levelClear = state_ == State::Clear;
}

void PlayScene::renderDebug(SDL_Renderer* r) const {
    const Vec2 cam = camera_.position();
    // Solid tiles around the player (what collision is testing against).
    const int ptx = static_cast<int>(std::floor(player_.position().x / kTileSize));
    const int pty = static_cast<int>(std::floor(player_.position().y / kTileSize));
    for (int ty = pty - 2; ty <= pty + 2; ++ty) {
        for (int tx = ptx - 2; tx <= ptx + 2; ++tx) {
            const Tile t = level_.tileAt(tx, ty);
            const RectF box{static_cast<float>(tx * kTileSize) - cam.x, static_cast<float>(ty * kTileSize) - cam.y,
                            static_cast<float>(kTileSize), static_cast<float>(kTileSize)};
            if (Level::isSolid(t)) draw::rectOutline(r, box, SDL_Color{255, 60, 60, 255});
            else if (t == Tile::Exit) draw::rectOutline(r, box, SDL_Color{0, 220, 255, 255});
        }
    }
    RectF hb = player_.hitbox();
    hb.x -= cam.x;
    hb.y -= cam.y;
    draw::rectOutline(r, hb, player_.isInvincible() ? SDL_Color{255, 80, 80, 255} : SDL_Color{0, 255, 120, 255});
    const Vec2 p = player_.position() - cam;
    draw::fillRect(r, static_cast<int>(p.x) - 1, static_cast<int>(p.y) - 1, 3, 3, SDL_Color{255, 0, 255, 255});

    // Camera dead-zone (centre of the view).
    draw::rectOutline(r,
                      RectF{(kScreenWidth - Camera::kDeadZoneW) * 0.5f, (kScreenHeight - Camera::kDeadZoneH) * 0.5f,
                            Camera::kDeadZoneW, Camera::kDeadZoneH},
                      SDL_Color{255, 220, 0, 160});
}

} // namespace pd
