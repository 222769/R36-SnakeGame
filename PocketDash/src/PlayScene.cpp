#include "PlayScene.h"

#include "BuiltinLevels.h"
#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "LevelLoader.h"
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

PlayScene::PlayScene(Game& game, std::string levelId)
    : Scene(game), levelId_(std::move(levelId)), camera_(kScreenWidth, kScreenHeight) {
    std::string error;
    bool loaded = false;
    if (levelId_ != "test") {
        loaded = levels::load(levelId_, level_, &error);
        if (!loaded) SDL_Log("[level] %s - playing the built-in test meadow instead", error.c_str());
    }
    if (!loaded) {
        std::string builtinError;
        if (!makeTestLevel(level_, &builtinError)) throw std::runtime_error("Built-in level is invalid: " + builtinError);
        if (levelId_ != "test") showToast("LEVEL FILE ERROR: SEE LOG.TXT");
        levelId_ = "test";
    }
    SDL_Log("[level] Playing %s \"%s\" (%dx%d, objective %s)", level_.id.c_str(), level_.name.c_str(),
            level_.width(), level_.height(), objectiveName(level_.objective));

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
    tiles_.prepare(session_->level()); // smashed blocks come back
    camera_.setShakeEnabled(game_.settings().screenShake);
    camera_.snapTo(session_->player().position());
    overlay_ = Overlay::None;
    hudHurt_ = 0.0f;
    signIndex_ = -1;
}

void PlayScene::showToast(const char* text) {
    std::snprintf(toast_, sizeof(toast_), "%s", text);
    toastTime_ = 2.2f;
}

void PlayScene::objectiveText(char* out, size_t size) const {
    if (!level_.hint.empty()) {
        std::snprintf(out, size, "%s", level_.hint.c_str());
        return;
    }
    int have = 0;
    int need = 0;
    session_->objectiveProgress(have, need);
    switch (level_.objective) {
    case Objective::ReachExit: std::snprintf(out, size, "REACH THE FLAG!"); break;
    case Objective::Coins: std::snprintf(out, size, "COLLECT %d COINS!", need); break;
    case Objective::Stars: std::snprintf(out, size, "FIND ALL %d STARS!", need); break;
    case Objective::Rescue: std::snprintf(out, size, "RESCUE %d FRIENDS WITH Y!", need); break;
    case Objective::DefeatAll: std::snprintf(out, size, "DEFEAT ALL %d FOES!", need); break;
    }
}

void PlayScene::goToNextLevel() {
    const std::string next = levels::nextLevelId(levelId_);
    if (next.empty())
        game_.changeScene(std::make_unique<TitleScene>(game_));
    else
        game_.changeScene(std::make_unique<PlayScene>(game_, next));
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void PlayScene::update(float dt) {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();

    if (toastTime_ > 0.0f) toastTime_ -= dt;
    if (overlay_ == Overlay::Sign) {
        if (in.pressed(Action::Y) || in.pressed(Action::A) || in.pressed(Action::B) || in.pressed(Action::Start)) {
            overlay_ = Overlay::None;
            audio.play(Sfx::MenuMove);
        }
        return;
    }
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
    pin.usePressed = in.pressed(Action::X);
    pin.interactPressed = in.pressed(Action::Y);
    handleEvents(session_->update(pin, dt));
    if (session_->state() == LevelSession::State::TimeUp && session_->stateTime() >= kClearInputDelay) {
        if (in.pressed(Action::A) || in.pressed(Action::Start)) {
            audio.play(Sfx::MenuSelect);
            restart();
        } else if (in.pressed(Action::B)) {
            audio.play(Sfx::MenuSelect);
            game_.changeScene(std::make_unique<TitleScene>(game_));
        }
    }

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
    if (events & kSessionStar) audio.play(Sfx::Star);
    if (events & kSessionGem) audio.play(Sfx::Gem);
    if (events & kSessionPowerUpGet) audio.play(Sfx::PowerUp);
    if (events & kSessionPowerUpUse) audio.play(Sfx::PowerUpUse);
    if (events & kSessionShieldPop) {
        audio.play(Sfx::ShieldPop);
        camera_.shake(3.0f, 0.15f);
    }
    if (events & kSessionNoRoom) {
        audio.play(Sfx::Denied);
        showToast("NO ROOM TO GROW HERE!");
    }
    if (events & kSessionKey) {
        audio.play(Sfx::PowerUp);
        showToast("GOT A KEY!");
    }
    if (events & kSessionGateOpened) {
        audio.play(Sfx::Break);
        tiles_.prepare(session_->level());
        showToast("GATE OPENED!");
    }
    if (events & kSessionLocked) {
        audio.play(Sfx::Denied);
        showToast("LOCKED! FIND A KEY.");
    }
    if (events & kSessionRescue) {
        audio.play(Sfx::Star);
        char text[48];
        std::snprintf(text, sizeof(text), "FRIEND RESCUED! %d/%d", session_->friendsRescued(),
                      static_cast<int>(session_->friends().size()));
        showToast(text);
    }
    if (events & kSessionSecret) {
        audio.play(Sfx::Gem);
        showToast("SECRET FOUND!");
    }
    if (events & kSessionSign) {
        audio.play(Sfx::MenuSelect);
        signIndex_ = session_->readingSign();
        overlay_ = Overlay::Sign;
    }
    if (events & kSessionExitLocked) {
        audio.play(Sfx::Denied);
        char goal[48];
        objectiveText(goal, sizeof(goal));
        char text[64];
        std::snprintf(text, sizeof(text), "NOT YET! %s", goal);
        showToast(text);
    }
    if (events & kSessionTimeUp) audio.play(Sfx::PlayerHurt);
    if (events & kSessionBlockBroken) {
        audio.play(Sfx::Break);
        camera_.shake(3.0f, 0.12f);
        tiles_.prepare(session_->level()); // re-pick tile art around the hole
    }
}

void PlayScene::updateCleared() {
    if (session_->stateTime() < kClearInputDelay) return;
    InputManager& in = game_.input();
    if (in.pressed(Action::A) || in.pressed(Action::Start)) {
        game_.audio().play(Sfx::MenuSelect);
        goToNextLevel();
    } else if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuSelect);
        restart();
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
    if (state == LevelSession::State::Playing && overlay_ == Overlay::None) renderPrompt(r, cam);
    renderToast(r);
    if (state == LevelSession::State::Cleared) renderClearPanel(r);
    if (state == LevelSession::State::TimeUp) renderTimeUpPanel(r);
    if (overlay_ == Overlay::Sign) renderSignDialog(r);
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
            const int size = Sprites::kEnemyFrame;
            const SDL_Rect src{active ? Sprites::kEnemyFrame : 0, 0, Sprites::kEnemyFrame, Sprites::kEnemyFrame};
            const SDL_Rect dst{sx - 12, sy + 4 - size, size, size};
            SDL_RenderCopy(r, flag, &src, &dst);
        }
    }
    // Rafts float on the water, under everything else.
    for (const LevelSession::Raft& raft : session_->rafts()) {
        const RectF rr = raft.rect();
        const int x = static_cast<int>(rr.x - cam.x);
        const int y = static_cast<int>(rr.y - cam.y) + static_cast<int>(std::sin(animTime_ * 3.0f + rr.x) * 1.5f);
        const int w = static_cast<int>(rr.w);
        const int h = static_cast<int>(rr.h);
        draw::fillRect(r, x + 2, y + 4, w - 4, h - 4, SDL_Color{40, 90, 140, 120}); // shadow in the water
        draw::fillRect(r, x, y, w, h - 4, SDL_Color{100, 60, 32, 255});
        for (int py = 2; py < h - 6; py += 8) draw::fillRect(r, x + 2, y + py, w - 4, 6, SDL_Color{186, 124, 70, 255});
        draw::fillRect(r, x + 6, y, 3, h - 4, SDL_Color{100, 60, 32, 255});
        draw::fillRect(r, x + w - 9, y, 3, h - 4, SDL_Color{100, 60, 32, 255});
    }

    // Signs.
    if (SDL_Texture* props = sprites.checkpoint()) {
        for (const SignSpawn& sign : level_.signs) {
            const int size = Sprites::kEnemyFrame;
            const SDL_Rect src{Sprites::kPropSign * Sprites::kEnemyFrame, 0, Sprites::kEnemyFrame, Sprites::kEnemyFrame};
            const SDL_Rect dst{static_cast<int>(sign.pos.x - cam.x) - size / 2, static_cast<int>(sign.pos.y - cam.y) + 4 - size,
                               size, size};
            SDL_RenderCopy(r, props, &src, &dst);
        }
    }

    // Lost friends hop on the spot (with a "!" bubble); rescued ones hop away and fade.
    if (SDL_Texture* sheet = sprites.enemies()) {
        const auto& friends = session_->friends();
        for (size_t i = 0; i < friends.size(); ++i) {
            const LevelSession::Friend& f = friends[i];
            if (f.rescued && f.rescueTime > 1.0f) continue;
            const float t = animTime_ * 5.0f + static_cast<float>(i);
            const float lift = f.rescued ? f.rescueTime * 80.0f : std::fabs(std::sin(t)) * 6.0f;
            const int sx = static_cast<int>(f.pos.x - cam.x);
            const int sy = static_cast<int>(f.pos.y - cam.y);
            draw::fillEllipse(r, sx, sy + 2, 8, 3, SDL_Color{0, 0, 0, 70});
            const int size = Sprites::kEnemyFrame;
            const SDL_Rect src{0, Sprites::kRowFriend * Sprites::kEnemyFrame, Sprites::kEnemyFrame, Sprites::kEnemyFrame};
            const SDL_Rect dst{sx - size / 2, sy + 4 - size - static_cast<int>(lift), size, size};
            const Uint8 alpha = f.rescued ? static_cast<Uint8>(255.0f * std::max(0.0f, 1.0f - f.rescueTime)) : 255;
            SDL_SetTextureAlphaMod(sheet, alpha);
            SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr,
                             static_cast<int>(t * 0.3f) % 2 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
            SDL_SetTextureAlphaMod(sheet, 255);
            if (!f.rescued) game_.font().drawCentered(r, sx, sy - 44 - static_cast<int>(lift), "!", 2, ui::kPink);
        }
    }

    // Exit flag stays padlocked until the objective is complete.
    if (!session_->objectiveComplete()) {
        const int ex = static_cast<int>(level_.exit.x - cam.x);
        const int ey = static_cast<int>(level_.exit.y - cam.y) - 26 + static_cast<int>(std::sin(animTime_ * 3.0f) * 2.0f);
        draw::fillRect(r, ex - 5, ey - 7, 10, 2, SDL_Color{200, 140, 30, 255});
        draw::fillRect(r, ex - 6, ey - 6, 2, 6, SDL_Color{200, 140, 30, 255});
        draw::fillRect(r, ex + 4, ey - 6, 2, 6, SDL_Color{200, 140, 30, 255});
        draw::fillRect(r, ex - 8, ey, 16, 12, SDL_Color{200, 140, 30, 255});
        draw::fillRect(r, ex - 6, ey + 2, 12, 8, SDL_Color{255, 210, 63, 255});
        draw::fillRect(r, ex - 1, ey + 4, 2, 4, SDL_Color{40, 28, 60, 255});
    }

    session_->coins().render(r, sprites.coin(), cam, animTime_);
    session_->heartPickups().render(r, sprites.heartFull(), cam, animTime_);
    session_->items().render(r, sprites.items(), cam, animTime_);

    // Characters, back to front: enemies behind the player first.
    const Player& player = session_->player();
    const float py = player.position().y;
    for (const Enemy& e : session_->enemies())
        if (e.position().y <= py) e.render(r, sprites.enemies(), cam);
    player.render(r, sprites.player(), cam);
    if (session_->powers().active(PowerUpType::ShieldBubble)) {
        // Wobbling translucent bubble around the hero.
        const Vec2 c = player.position() - cam - Vec2{0.0f, 14.0f * player.modifiers().visualScale + player.height()};
        const int radius = static_cast<int>((20.0f + std::sin(animTime_ * 6.0f) * 1.5f) * player.modifiers().visualScale);
        draw::fillCircle(r, static_cast<int>(c.x), static_cast<int>(c.y), radius, SDL_Color{150, 220, 255, 60});
        draw::fillCircle(r, static_cast<int>(c.x) - radius / 3, static_cast<int>(c.y) - radius / 3, 3,
                         SDL_Color{255, 255, 255, 180});
    }
    for (const Enemy& e : session_->enemies())
        if (e.position().y > py) e.render(r, sprites.enemies(), cam);

    session_->effects().render(r, cam);
}

void PlayScene::renderHud(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const int maxHearts = session_->maxHearts();
    const int hearts = session_->hearts();

    // Hearts (drawn 1:1) on a soft backing; they wobble after damage.
    const int wobble = hudHurt_ > 0.0f ? static_cast<int>(std::sin(hudHurt_ * 40.0f) * 3.0f) : 0;
    ui::drawPanel(r, SDL_Rect{6, 8, maxHearts * 32 + 10, 38},
                  hudHurt_ > 0.0f ? SDL_Color{120, 20, 40, 170} : SDL_Color{20, 16, 40, 140}, SDL_Color{0, 0, 0, 0});
    for (int i = 0; i < maxHearts; ++i) {
        SDL_Texture* tex = i < hearts ? game_.sprites().heartFull() : game_.sprites().heartEmpty();
        const SDL_Rect dst{14 + i * 32 + wobble, 15, Sprites::kHeartW, Sprites::kHeartH};
        SDL_RenderCopy(r, tex, nullptr, &dst);
    }

    renderCollectionHud(r);
    renderPowerHud(r);

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
        const SDL_Rect dst{panelX + 10, 15, Sprites::kCoinSize, Sprites::kCoinSize};
        SDL_RenderCopy(r, coin, &src, &dst);
    }
    font.drawShadowed(r, panelX + 42, 16, coinText, 3, ui::kYellow);
    if (session_->powers().active(PowerUpType::DoubleCoins)) {
        // "x2" badge that pulses while Double Coins lasts.
        const int pulse = static_cast<int>(std::fabs(std::sin(animTime_ * 6.0f)) * 3.0f);
        font.drawShadowed(r, panelX - 44 - pulse, 16, "x2", 3, ui::kYellow);
    }

    // Clock, or a countdown on timed levels (red in the last 10 seconds).
    char clock[24];
    const bool timed = session_->timeLimit() > 0.0f;
    formatTime(clock, sizeof(clock), timed ? session_->timeLeft() : session_->time(), false);
    const bool hurry = timed && session_->timeLeft() < 10.0f;
    const int clockScale = timed ? 3 : 2;
    const SDL_Color clockColor = hurry && static_cast<int>(animTime_ * 4.0f) % 2 ? ui::kPink : ui::kWhite;
    font.drawShadowed(r, kScreenWidth - BitmapFont::textWidth(clock, clockScale) - 12, 52, clock, clockScale, clockColor);

    renderObjectiveHud(r);
}

void PlayScene::renderObjectiveHud(SDL_Renderer* r) const {
    // Objective progress and keys, right-aligned under the clock.
    const BitmapFont& font = game_.font();
    int y = session_->timeLimit() > 0.0f ? 82 : 74;
    int have = 0;
    int need = 0;
    session_->objectiveProgress(have, need);
    if (need > 0) {
        const char* label = level_.objective == Objective::Coins     ? "COINS"
                            : level_.objective == Objective::Stars   ? "STARS"
                            : level_.objective == Objective::Rescue  ? "FRIENDS"
                                                                     : "FOES";
        char text[32];
        std::snprintf(text, sizeof(text), "%s %d/%d", label, std::min(have, need), need);
        font.drawShadowed(r, kScreenWidth - BitmapFont::textWidth(text, 2) - 12, y, text, 2,
                          have >= need ? ui::kMint : ui::kYellow);
        y += 22;
    }
    if (session_->keysHeld() > 0) {
        if (SDL_Texture* items = game_.sprites().items()) {
            char text[16];
            std::snprintf(text, sizeof(text), "x%d", session_->keysHeld());
            const int tw = BitmapFont::textWidth(text, 2);
            const SDL_Rect src{Sprites::kItemKey * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
            const SDL_Rect dst{kScreenWidth - tw - 12 - 28, y - 4, 24, 24};
            SDL_RenderCopy(r, items, &src, &dst);
            font.drawShadowed(r, kScreenWidth - tw - 12, y, text, 2, ui::kYellow);
        }
    }
}

void PlayScene::renderPrompt(SDL_Renderer* r, Vec2 cam) const {
    // A bobbing "Y" bubble over whatever Y would interact with.
    Vec2 where;
    if (session_->interactTarget(&where) == LevelSession::InteractKind::None) return;
    const int x = static_cast<int>(where.x - cam.x);
    const int y = static_cast<int>(where.y - cam.y) - 62 + static_cast<int>(std::sin(animTime_ * 5.0f) * 3.0f);
    draw::fillCircle(r, x, y, 11, SDL_Color{30, 22, 48, 220});
    draw::fillCircle(r, x, y, 9, SDL_Color{255, 214, 64, 255});
    game_.font().draw(r, x - 5, y - 7, "Y", 2, ui::kInk);
}

void PlayScene::renderToast(SDL_Renderer* r) const {
    if (toastTime_ <= 0.0f || !toast_[0]) return;
    const BitmapFont& font = game_.font();
    const int w = BitmapFont::textWidth(toast_, 2) + 32;
    // Slide up from the bottom edge, then fade out.
    const float in = std::min(1.0f, (2.2f - toastTime_) / 0.15f);
    const int y = kScreenHeight - 64 + static_cast<int>((1.0f - in) * 40.0f);
    const Uint8 a = static_cast<Uint8>(255.0f * std::min(1.0f, toastTime_ / 0.3f));
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - w / 2, y, w, 36}, SDL_Color{34, 28, 60, static_cast<Uint8>(a * 0.85f)},
                  SDL_Color{255, 255, 255, a});
    font.draw(r, kScreenWidth / 2 - w / 2 + 16, y + 11, toast_, 2, SDL_Color{255, 255, 255, a});
}

void PlayScene::renderSignDialog(SDL_Renderer* r) const {
    if (signIndex_ < 0 || signIndex_ >= static_cast<int>(level_.signs.size())) return;
    const BitmapFont& font = game_.font();
    const SDL_Rect box{40, 300, kScreenWidth - 80, 150};
    ui::drawPanel(r, box, SDL_Color{250, 240, 220, 240}, SDL_Color{100, 60, 32, 255});

    // Word-wrap the sign text to the box width.
    const std::string& text = level_.signs[static_cast<size_t>(signIndex_)].text;
    const int maxW = box.w - 40;
    int y = box.y + 20;
    size_t pos = 0;
    while (pos < text.size() && y < box.y + box.h - 40) {
        // Take whole words while they fit; a single over-long word is cut.
        size_t len = 0;
        for (size_t end = pos; end <= text.size(); ++end) {
            if (end != text.size() && text[end] != ' ') continue;
            if (BitmapFont::textWidth(std::string_view(text).substr(pos, end - pos), 2) > maxW) break;
            len = end - pos;
        }
        if (len == 0) {
            len = 1;
            while (pos + len < text.size() &&
                   BitmapFont::textWidth(std::string_view(text).substr(pos, len + 1), 2) <= maxW)
                ++len;
        }
        font.draw(r, box.x + 20, y, std::string_view(text).substr(pos, len), 2, ui::kInk);
        y += 22;
        pos += len;
        while (pos < text.size() && text[pos] == ' ') ++pos;
    }
    font.draw(r, box.x + box.w - BitmapFont::textWidth("Y  OK", 2) - 16, box.y + box.h - 26, "Y  OK", 2,
              SDL_Color{100, 60, 32, 255});
}

void PlayScene::renderTimeUpPanel(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    ui::dimScreen(r, 120);
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - 170, 160, 340, 160});
    const int bounce = static_cast<int>(std::fabs(std::sin(session_->stateTime() * 4.0f)) * -6.0f);
    font.drawCentered(r, kScreenWidth / 2, 180 + bounce, "TIME UP!", 4, ui::kPink);
    font.drawCentered(r, kScreenWidth / 2, 228, "SO CLOSE! TRY AGAIN?", 2, ui::kWhite);
    if (session_->stateTime() >= kClearInputDelay) {
        font.drawCentered(r, kScreenWidth / 2, 262, "A  RETRY", 2, ui::kWhite);
        font.drawCentered(r, kScreenWidth / 2, 286, "B  TITLE", 2, ui::kGrey);
    }
}

void PlayScene::renderCollectionHud(SDL_Renderer* r) const {
    // Star slots (top centre), plus a gem counter when the level has gems.
    SDL_Texture* items = game_.sprites().items();
    if (!items) return;
    const ItemField& it = session_->items();
    const int stars = it.starsTotal();
    const bool gems = it.gemsTotal() > 0;
    if (stars == 0 && !gems) return;

    constexpr int icon = Sprites::kItemSize;
    const int width = stars * (icon + 4) + (gems ? icon + 40 : 0) + 12;
    const int x0 = kScreenWidth / 2 - width / 2;
    ui::drawPanel(r, SDL_Rect{x0, 8, width, 38}, SDL_Color{20, 16, 40, 140}, SDL_Color{0, 0, 0, 0});
    int x = x0 + 8;
    for (int i = 0; i < stars; ++i) {
        const int frame = it.starCollected(i) ? Sprites::kItemStar : Sprites::kItemStarEmpty;
        const SDL_Rect src{frame * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
        const SDL_Rect dst{x, 15, icon, icon};
        SDL_RenderCopy(r, items, &src, &dst);
        x += icon + 4;
    }
    if (gems) {
        const SDL_Rect src{Sprites::kItemGem * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
        const SDL_Rect dst{x + 2, 15, icon, icon};
        SDL_RenderCopy(r, items, &src, &dst);
        char text[8];
        std::snprintf(text, sizeof(text), "%d", it.gemsCollected());
        game_.font().drawShadowed(r, x + icon + 6, 19, text, 2, ui::kMint);
    }
}

void PlayScene::renderPowerHud(SDL_Renderer* r) const {
    // Below the hearts: the stored power-up (press X), then active ones with
    // a shrinking timer bar each.
    SDL_Texture* items = game_.sprites().items();
    if (!items) return;
    const PowerUpState& powers = session_->powers();
    const BitmapFont& font = game_.font();
    constexpr int icon = Sprites::kItemSize;
    int x = 6;
    const int y = 50;

    auto drawIcon = [&](PowerUpType t, int ix, int iy) {
        const SDL_Rect src{Sprites::itemFrame(t) * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
        const SDL_Rect dst{ix, iy, icon, icon};
        if (t == PowerUpType::RainbowStar)
            SDL_SetTextureColorMod(items, 255, static_cast<Uint8>(160.0f + std::sin(animTime_ * 6.0f) * 90.0f), 200);
        SDL_RenderCopy(r, items, &src, &dst);
        if (t == PowerUpType::RainbowStar) SDL_SetTextureColorMod(items, 255, 255, 255);
    };

    if (powers.stored() != PowerUpType::None) {
        const SDL_Color c = powerUpInfo(powers.stored()).color;
        ui::drawPanel(r, SDL_Rect{x, y, 48, 40}, SDL_Color{20, 16, 40, 160}, c);
        drawIcon(powers.stored(), x + 6, y + 8);
        font.drawShadowed(r, x + 36, y + 26, "X", 1, ui::kWhite);
        x += 54;
    }
    for (int i = 1; i < kPowerUpCount; ++i) {
        const auto t = static_cast<PowerUpType>(i);
        if (!powers.active(t)) continue;
        drawIcon(t, x + 4, y + 4);
        const float duration = powerUpInfo(t).duration;
        if (duration > 0.0f) {
            const float frac = std::clamp(powers.timeLeft(t) / duration, 0.0f, 1.0f);
            draw::fillRect(r, x + 4, y + 31, icon, 4, SDL_Color{20, 16, 40, 180});
            // Blink in the last two seconds so the end is no surprise.
            const bool ending = powers.timeLeft(t) < 2.0f && static_cast<int>(animTime_ * 8.0f) % 2 == 0;
            if (!ending)
                draw::fillRect(r, x + 4, y + 31, static_cast<int>(icon * frac), 4, powerUpInfo(t).color);
        }
        x += icon + 8;
    }
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
    char goal[64];
    objectiveText(goal, sizeof(goal));
    const BitmapFont& font = game_.font();
    const int w = std::max(BitmapFont::textWidth(title, 3), BitmapFont::textWidth(goal, 2)) + 40;
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - w / 2, iy, w, kH});
    font.drawCentered(r, kScreenWidth / 2, iy + 10, title, 3, ui::kYellow);
    font.drawCentered(r, kScreenWidth / 2, iy + 38, goal, 2, ui::kWhite);
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

    const ItemField& items = session_->items();
    char found[64];
    std::snprintf(found, sizeof(found), "COINS %d/%d  STARS %d/%d", session_->coins().collected(),
                  session_->coins().total(), items.starsCollected(), items.starsTotal());
    char clock[24];
    formatTime(clock, sizeof(clock), session_->time(), false);
    char time[64];
    std::snprintf(time, sizeof(time), "TIME %s  MODE %s", clock, difficultyName(session_->difficulty()));

    char goal[64];
    objectiveText(goal, sizeof(goal));
    const char* lines[] = {
        goal,
        found,
        time,
        "",
        "D-PAD / STICK   MOVE",
        "A               HOP / STOMP",
        "B               DASH / SMASH",
        "X / Y           POWER-UP / TALK",
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
    constexpr int kTop = 232;
    constexpr int kHeight = 240;
    const float st = session_->stateTime();
    const float grow = std::min(1.0f, st / 0.25f);
    const int h = static_cast<int>(kHeight * grow);
    ui::dimScreen(r, static_cast<Uint8>(60 * grow));
    ui::drawPanel(r, SDL_Rect{kScreenWidth / 2 - 200, kTop + (kHeight - h) / 2, 400, h});
    if (grow < 1.0f) return;

    const int bounce = static_cast<int>(std::lround(std::fabs(std::sin(st * 4.0f)) * -6.0f));
    font.drawCentered(r, kScreenWidth / 2, kTop + 14 + bounce, "LEVEL CLEAR!", 4, ui::kYellow);

    // Stars found, popping in one by one.
    const ItemField& items = session_->items();
    if (SDL_Texture* tex = game_.sprites().items(); tex && items.starsTotal() > 0) {
        constexpr int icon = Sprites::kItemSize * 3 / 2;
        const int total = items.starsTotal();
        const int x0 = kScreenWidth / 2 - (total * (icon + 6) - 6) / 2;
        for (int i = 0; i < total; ++i) {
            if (st < 0.3f + 0.15f * static_cast<float>(i)) continue;
            const int frame = items.starCollected(i) ? Sprites::kItemStar : Sprites::kItemStarEmpty;
            const SDL_Rect src{frame * Sprites::kItemSize, 0, Sprites::kItemSize, Sprites::kItemSize};
            const SDL_Rect dst{x0 + i * (icon + 6), kTop + 52, icon, icon};
            SDL_RenderCopy(r, tex, &src, &dst);
        }
    }

    const CoinField& coinField = session_->coins();
    char line[64];
    std::snprintf(line, sizeof(line), "COINS  %d/%d", coinField.collected(), coinField.total());
    font.drawCentered(r, kScreenWidth / 2, kTop + 98, line, 3, ui::kWhite);
    char clock[24];
    formatTime(clock, sizeof(clock), session_->time(), true);
    std::snprintf(line, sizeof(line), "TIME  %s", clock);
    font.drawCentered(r, kScreenWidth / 2, kTop + 126, line, 3, ui::kWhite);
    if (items.gemsTotal() > 0)
        std::snprintf(line, sizeof(line), "FOES %d   GEMS %d/%d", session_->stats().enemiesDefeated, items.gemsCollected(),
                      items.gemsTotal());
    else
        std::snprintf(line, sizeof(line), "FOES DEFEATED  %d", session_->stats().enemiesDefeated);
    font.drawCentered(r, kScreenWidth / 2, kTop + 156, line, 2, ui::kWhite);

    if (session_->goldenStar()) {
        // The big reward: everything found in one run.
        const Uint8 g = static_cast<Uint8>(200.0f + 55.0f * std::sin(st * 8.0f));
        font.drawCentered(r, kScreenWidth / 2, kTop + 178, "GOLDEN STAR!", 2, SDL_Color{255, g, 60, 255});
    } else if (coinField.collected() == coinField.total()) {
        font.drawCentered(r, kScreenWidth / 2, kTop + 178, "ALL COINS!", 2, ui::kMint);
    }

    if (st >= kClearInputDelay) {
        const bool last = levels::nextLevelId(levelId_).empty();
        font.drawCentered(r, kScreenWidth / 2 - 96, kTop + 206, last ? "A  TITLE" : "A  NEXT LEVEL", 2, ui::kWhite);
        font.drawCentered(r, kScreenWidth / 2 + 96, kTop + 206, "B  PLAY AGAIN", 2, ui::kGrey);
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
            const Tile t = session_->level().tileAt(tx, ty);
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
