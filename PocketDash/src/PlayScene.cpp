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
// Results-screen inputs are ignored briefly so gameplay presses don't skip it.
constexpr float kClearInputDelay = 0.8f;

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

    session_ = std::make_unique<LevelSession>(level_, game.settings().difficulty);
    restart();
    game.audio().playMusic(MusicTrack::Meadow);
}

void PlayScene::restart() {
    session_->restart();
    camera_.setShakeEnabled(game_.settings().screenShake);
    camera_.snapTo(session_->player().position());
    overlay_ = Overlay::None;
    hudHurt_ = 0.0f;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void PlayScene::update(float dt) {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();

    if (overlay_ == Overlay::Info) {
        if (in.pressed(Action::Select) || in.pressed(Action::B) || in.pressed(Action::A) || in.pressed(Action::Start)) {
            overlay_ = Overlay::None;
            audio.play(Sfx::MenuMove);
        }
        return;
    }
    if (overlay_ == Overlay::Paused) {
        updatePauseMenu();
        return;
    }

    animTime_ += dt;
    if (hudHurt_ > 0.0f) hudHurt_ -= dt;
    const LevelSession::State state = session_->state();

    if (state == LevelSession::State::Playing) {
        if (in.pressed(Action::Start)) {
            overlay_ = Overlay::Paused;
            pauseIndex_ = 0;
            audio.play(Sfx::Pause);
            return;
        }
        if (in.pressed(Action::Select)) {
            overlay_ = Overlay::Info;
            audio.play(Sfx::MenuMove);
            return;
        }
        // Debug warps (overlay on): R1 = next to the exit, L1 = next enemy.
        // Select+L1 toggles the overlay itself, so plain L1 only.
        if (game_.debugEnabled() && in.pressed(Action::R1) && session_->warpNear(level_.exit)) {
            camera_.snapTo(session_->player().position());
            SDL_Log("[debug] Warped next to the exit");
        }
        if (game_.debugEnabled() && in.pressed(Action::L1) && !in.down(Action::Select)) {
            const auto& enemies = session_->enemies();
            for (size_t tries = 0; tries < enemies.size(); ++tries) {
                const Enemy& e = enemies[static_cast<size_t>(debugEnemyIndex_++) % enemies.size()];
                if (e.alive() && session_->warpNear(e.position() + Vec2{0.0f, -64.0f})) {
                    camera_.snapTo(session_->player().position());
                    SDL_Log("[debug] Warped near a %s", enemyTypeName(e.type()));
                    break;
                }
            }
        }
    }

    PlayerInput pin;
    pin.move = in.moveVector();
    pin.hopPressed = in.pressed(Action::A);
    pin.dashPressed = in.pressed(Action::B);
    handleEvents(session_->update(pin, dt));

    const Player& player = session_->player();
    if (session_->state() == LevelSession::State::Cleared) {
        // Frame the hero in the upper half, above the results panel.
        camera_.follow(player.position() + Vec2{0.0f, 110.0f}, {}, dt);
        updateCleared();
    } else {
        camera_.follow(player.position() - Vec2{0.0f, 12.0f}, player.velocity() * 0.18f, dt);
    }
}

void PlayScene::handleEvents(unsigned events) {
    AudioManager& audio = game_.audio();
    if (events & kSessionHopped) audio.play(Sfx::Jump);
    if (events & kSessionDashed) audio.play(Sfx::Dash);
    if (events & kSessionCoin) audio.play(Sfx::Coin);
    if (events & kSessionHeart) audio.play(Sfx::Heart);
    if (events & kSessionCheckpoint) audio.play(Sfx::Checkpoint);
    if (events & kSessionStomp) {
        audio.play(Sfx::EnemyHit);
        camera_.shake(2.0f, 0.12f);
    }
    if (events & kSessionSplash) {
        audio.play(Sfx::Splash);
        camera_.snapTo(session_->player().position()); // rescued: jump the view back with them
    }
    if (events & kSessionHurt) {
        audio.play(Sfx::PlayerHurt);
        camera_.shake(6.0f, 0.3f);
        hudHurt_ = 0.5f;
    }
    if (events & kSessionCleared) audio.play(Sfx::LevelComplete);
}

void PlayScene::updateCleared() {
    if (session_->stateTime() < kClearInputDelay) return;
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
        overlay_ = Overlay::None;
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
        case 0: overlay_ = Overlay::None; break;
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
    renderWorld(r, cam);
    renderHud(r);

    const LevelSession::State state = session_->state();
    if (state == LevelSession::State::KnockedOut) renderKnockOut(r, cam);
    if (session_->time() < kBannerTime && state == LevelSession::State::Playing && overlay_ == Overlay::None)
        renderBanner(r);
    if (state == LevelSession::State::Cleared) renderClearPanel(r);
    if (overlay_ == Overlay::Info) renderInfoPanel(r);
    if (overlay_ == Overlay::Paused) renderPauseMenu(r);
}

void PlayScene::renderWorld(SDL_Renderer* r, Vec2 cam) const {
    const Sprites& sprites = game_.sprites();
    tiles_.render(r, cam, animTime_);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Ground items first.
    if (SDL_Texture* flag = sprites.checkpoint()) {
        const auto& cps = session_->checkpoints();
        for (size_t i = 0; i < cps.size(); ++i) {
            const bool active = cps[i].reached; // coloured once touched
            const int sx = static_cast<int>(cps[i].pos.x - cam.x);
            const int sy = static_cast<int>(cps[i].pos.y - cam.y);
            const int size = Sprites::kEnemyFrame * kPixelScale;
            const SDL_Rect src{active ? Sprites::kEnemyFrame : 0, 0, Sprites::kEnemyFrame, Sprites::kEnemyFrame};
            const SDL_Rect dst{sx - 12, sy + 4 - size, size, size};
            SDL_RenderCopy(r, flag, &src, &dst);
        }
    }
    session_->coins().render(r, sprites.coin(), cam, animTime_);
    session_->heartPickups().render(r, sprites.heartFull(), cam, animTime_);

    // Characters, back to front: enemies behind the player first.
    const Player& player = session_->player();
    const float py = player.position().y;
    for (const Enemy& e : session_->enemies())
        if (e.position().y <= py) e.render(r, sprites.enemies(), cam);
    player.render(r, sprites.player(), cam);
    for (const Enemy& e : session_->enemies())
        if (e.position().y > py) e.render(r, sprites.enemies(), cam);

    session_->effects().render(r, cam);
}

void PlayScene::renderHud(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const int maxHearts = session_->maxHearts();
    const int hearts = session_->hearts();

    // Hearts (9x8 art at 3x) on a soft backing; they wobble after damage.
    const int wobble = hudHurt_ > 0.0f ? static_cast<int>(std::sin(hudHurt_ * 40.0f) * 3.0f) : 0;
    ui::drawPanel(r, SDL_Rect{6, 8, maxHearts * 32 + 10, 38},
                  hudHurt_ > 0.0f ? SDL_Color{120, 20, 40, 170} : SDL_Color{20, 16, 40, 140}, SDL_Color{0, 0, 0, 0});
    for (int i = 0; i < maxHearts; ++i) {
        SDL_Texture* tex = i < hearts ? game_.sprites().heartFull() : game_.sprites().heartEmpty();
        const SDL_Rect dst{14 + i * 32 + wobble, 15, Sprites::kHeartW * 3, Sprites::kHeartH * 3};
        SDL_RenderCopy(r, tex, nullptr, &dst);
    }

    // Coin counter and clock, top right.
    const CoinField& coins = session_->coins();
    char coinText[16];
    std::snprintf(coinText, sizeof(coinText), "%d/%d", coins.collected(), coins.total());
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
    formatTime(clock, sizeof(clock), session_->time(), false);
    font.drawShadowed(r, kScreenWidth - BitmapFont::textWidth(clock, 2) - 12, 52, clock, 2, ui::kWhite);
}

void PlayScene::renderBanner(SDL_Renderer* r) const {
    // Shown on whichever half of the screen the hero is not in, so it never
    // hides them. Slides in from that edge, holds, then slides back out.
    const float heroScreenY = session_->player().position().y - camera_.position().y;
    const bool atBottom = heroScreenY < kScreenHeight * 0.5f;
    constexpr int kH = 62;
    const float rest = atBottom ? static_cast<float>(kScreenHeight - kH - 20) : 64.0f;
    const float hidden = atBottom ? static_cast<float>(kScreenHeight + 4) : -static_cast<float>(kH + 4);

    const float t = session_->time();
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

void PlayScene::renderKnockOut(SDL_Renderer* r, Vec2 cam) const {
    // Dizzy stars circling the hero's head, and a friendly "OOPS!".
    const Vec2 p = session_->player().position() - cam;
    const float t = session_->stateTime();
    for (int i = 0; i < 3; ++i) {
        const float a = t * 6.0f + static_cast<float>(i) * 2.094f;
        const int x = static_cast<int>(p.x + std::cos(a) * 14.0f);
        const int y = static_cast<int>(p.y - 38.0f + std::sin(a) * 5.0f);
        draw::fillRect(r, x - 3, y - 1, 6, 2, ui::kYellow);
        draw::fillRect(r, x - 1, y - 3, 2, 6, ui::kYellow);
    }
    const int bounce = static_cast<int>(std::fabs(std::sin(t * 5.0f)) * -8.0f);
    game_.font().drawCentered(r, static_cast<int>(p.x), static_cast<int>(p.y) - 80 + bounce, "OOPS!", 4, ui::kPink);
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
    ui::drawPanel(r, SDL_Rect{70, 70, kScreenWidth - 140, 340});

    char title[64];
    std::snprintf(title, sizeof(title), "%s  %s", level_.id.c_str(), level_.name.c_str());
    font.drawCentered(r, kScreenWidth / 2, 88, title, 3, ui::kYellow);
    font.drawCentered(r, kScreenWidth / 2, 122, worldDef(level_.world).name, 2, ui::kMint);

    char coins[48];
    std::snprintf(coins, sizeof(coins), "COINS: %d / %d", session_->coins().collected(), session_->coins().total());
    char clock[24];
    formatTime(clock, sizeof(clock), session_->time(), false);
    char time[40];
    std::snprintf(time, sizeof(time), "TIME:  %s", clock);
    char mode[40];
    std::snprintf(mode, sizeof(mode), "MODE:  %s", difficultyName(session_->difficulty()));

    const char* lines[] = {
        "GOAL: REACH THE FLAG!",
        coins,
        time,
        mode,
        "",
        "D-PAD / STICK   MOVE",
        "A               HOP / STOMP",
        "B               DASH",
        "START           PAUSE",
        "SELECT+START    QUIT",
    };
    int y = 154;
    for (const char* line : lines) {
        font.drawShadowed(r, 104, y, line, 2, ui::kWhite);
        y += 22;
    }
    font.drawCentered(r, kScreenWidth / 2, 384, "PRESS SELECT TO CLOSE", 2, ui::kGrey);
}

void PlayScene::renderClearPanel(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    // The panel sits in the lower part of the screen so the hero's victory
    // hops (upper half of the view) stay visible. It grows open over 0.25 s.
    constexpr int kTop = 250;
    constexpr int kHeight = 216;
    const float st = session_->stateTime();
    const float grow = std::min(1.0f, st / 0.25f);
    const int h = static_cast<int>(kHeight * grow);
    ui::dimScreen(r, static_cast<Uint8>(60 * grow));
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - 190, kTop + (kHeight - h) / 2, 380, h});
    if (grow < 1.0f) return;

    const int bounce = static_cast<int>(std::lround(std::fabs(std::sin(st * 4.0f)) * -6.0f));
    font.drawCentered(r, kScreenWidth / 2, kTop + 16 + bounce, "LEVEL CLEAR!", 4, ui::kYellow);

    const CoinField& coinField = session_->coins();
    char coins[32];
    std::snprintf(coins, sizeof(coins), "COINS  %d/%d", coinField.collected(), coinField.total());
    font.drawCentered(r, kScreenWidth / 2, kTop + 60, coins, 3, ui::kWhite);
    char clock[24];
    formatTime(clock, sizeof(clock), session_->time(), true);
    char time[40];
    std::snprintf(time, sizeof(time), "TIME  %s", clock);
    font.drawCentered(r, kScreenWidth / 2, kTop + 90, time, 3, ui::kWhite);
    char foes[40];
    std::snprintf(foes, sizeof(foes), "FOES DEFEATED  %d", session_->stats().enemiesDefeated);
    font.drawCentered(r, kScreenWidth / 2, kTop + 122, foes, 2, ui::kWhite);
    if (coinField.collected() == coinField.total())
        font.drawCentered(r, kScreenWidth / 2, kTop + 144, "ALL COINS!", 2, ui::kMint);

    if (st >= kClearInputDelay) {
        font.drawCentered(r, kScreenWidth / 2, kTop + 168, "A  PLAY AGAIN", 2, ui::kWhite);
        font.drawCentered(r, kScreenWidth / 2, kTop + 190, "B  TITLE", 2, ui::kGrey);
    }
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void PlayScene::fillDebugInfo(DebugInfo& info) const {
    const Player& player = session_->player();
    info.levelId = level_.id.c_str();
    info.hasPlayer = true;
    info.playerPos = player.position();
    info.playerVel = player.velocity();
    info.playerZ = player.height();
    info.enemyCount = session_->enemiesAlive();
    info.coins = session_->coins().collected();
    info.coinsTotal = session_->coins().total();
    info.hearts = session_->hearts();
    info.maxHearts = session_->maxHearts();
    info.hasCamera = true;
    info.camera = camera_.rawPosition();
    info.levelClear = session_->state() == LevelSession::State::Cleared;
}

void PlayScene::renderDebug(SDL_Renderer* r) const {
    const Vec2 cam = camera_.position();
    const Player& player = session_->player();
    auto toScreen = [&](RectF box) {
        box.x -= cam.x;
        box.y -= cam.y;
        return box;
    };

    // Solid and dangerous tiles around the player (what collision tests against).
    const int ptx = static_cast<int>(std::floor(player.position().x / kTileSize));
    const int pty = static_cast<int>(std::floor(player.position().y / kTileSize));
    for (int ty = pty - 2; ty <= pty + 2; ++ty) {
        for (int tx = ptx - 2; tx <= ptx + 2; ++tx) {
            const Tile t = level_.tileAt(tx, ty);
            const RectF box{static_cast<float>(tx * kTileSize), static_cast<float>(ty * kTileSize),
                            static_cast<float>(kTileSize), static_cast<float>(kTileSize)};
            if (Level::isSolid(t)) draw::rectOutline(r, toScreen(box), SDL_Color{255, 60, 60, 255});
            else if (Level::isDanger(t)) draw::rectOutline(r, toScreen(box), SDL_Color{255, 0, 255, 255});
            else if (t == Tile::Exit) draw::rectOutline(r, toScreen(box), SDL_Color{0, 220, 255, 255});
        }
    }

    for (const Enemy& e : session_->enemies())
        if (e.alive()) draw::rectOutline(r, toScreen(e.hitbox()), SDL_Color{255, 150, 0, 255});

    // Player hitbox (red while invincible), feet point and last safe spot.
    draw::rectOutline(r, toScreen(player.hitbox()),
                      player.isInvincible() ? SDL_Color{255, 80, 80, 255} : SDL_Color{0, 255, 120, 255});
    const Vec2 p = player.position() - cam;
    draw::fillRect(r, static_cast<int>(p.x) - 1, static_cast<int>(p.y) - 1, 3, 3, SDL_Color{255, 0, 255, 255});
    const Vec2 safe = session_->lastSafePosition() - cam;
    draw::rectOutline(r, RectF{safe.x - 3, safe.y - 3, 6, 6}, SDL_Color{120, 200, 255, 255});

    // Camera dead-zone (centre of the view).
    draw::rectOutline(r,
                      RectF{(kScreenWidth - Camera::kDeadZoneW) * 0.5f, (kScreenHeight - Camera::kDeadZoneH) * 0.5f,
                            Camera::kDeadZoneW, Camera::kDeadZoneH},
                      SDL_Color{255, 220, 0, 160});
}

} // namespace pd
