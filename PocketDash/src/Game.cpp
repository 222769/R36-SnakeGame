#include "Game.h"

#include "Constants.h"
#include "Draw.h"
#include "CollectionScene.h"
#include "HighScoresScene.h"
#include "LevelSelectScene.h"
#include "PlayScene.h"
#include "Platform.h"
#include "SettingsScene.h"
#include "TitleScene.h"

#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace pd {

// ---------------------------------------------------------------------------
// SDL lifetime
// ---------------------------------------------------------------------------

Game::SdlSystem::SdlSystem() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0)
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

    // Optional subsystems: report failures but keep going.
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        SDL_Log("[sdl] Game controller subsystem unavailable: %s", SDL_GetError());
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0)
        SDL_Log("[sdl] SDL_image PNG support unavailable: %s", IMG_GetError());
    if (TTF_Init() != 0) SDL_Log("[sdl] SDL_ttf unavailable: %s", TTF_GetError());

    SDL_version v;
    SDL_GetVersion(&v);
    SDL_Log("[sdl] SDL %d.%d.%d, video driver: %s", v.major, v.minor, v.patch,
            SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?");
}

Game::SdlSystem::~SdlSystem() {
    if (TTF_WasInit()) TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

// ---------------------------------------------------------------------------

Game::Game(const GameOptions& options) : options_(options) {
    SDL_Log("[game] Pocket Dash %s on %s", POCKETDASH_VERSION, platform::name());
    SDL_Log("[game] Data root: %s", platform::dataRoot().c_str());

    createWindowAndRenderer();

    save_ = std::make_unique<SaveManager>(platform::saveDir());
    SDL_Log("[game] Save directory: %s", save_->saveDir().c_str());

    input_.loadConfig(platform::dataPath("config/controller.cfg"));
    // Buttons remapped in Settings (automated runs use the shipped config).
    if (!automated()) input_.loadOverrides(save_->controllerOverridePath());
    input_.openJoysticks();
    debug_ = options_.debug || input_.bindings().startWithDebug;

    // Automated runs ignore the saved options so they behave the same everywhere.
    settings_ = automated() ? Settings{} : save_->loadSettings();
    if (!options_.difficulty.empty()) settings_.difficulty = difficultyFromName(options_.difficulty);
    SDL_Log("[game] Difficulty: %s", difficultyName(settings_.difficulty));

    if (audio_.init(platform::dataPath("assets/audio/"))) {
        audio_.setMusicVolume(settings_.musicVolume);
        audio_.setSfxVolume(settings_.sfxVolume);
    }

    if (!font_.create(renderer_.get(), platform::dataPath("assets/fonts/DejaVuSans-Bold.ttf")))
        throw std::runtime_error("Failed to create the built-in font");
    draw::initSkin(renderer_.get());
    if (!sprites_.create(renderer_.get(), platform::dataPath("assets/sprites/")))
        throw std::runtime_error("Failed to create sprites");
    if (!backdrop_.build(renderer_.get())) SDL_Log("[game] Could not paint the menu backdrop");

    // Automated runs start from a clean slate so they behave the same on
    // every machine; --demo-progress fills in sample records for screenshots.
    if (options_.demoProgress) progress_ = demoProgress();
    else if (!automated()) progress_ = save_->loadProgress();
    applyOutfit();

    // The smoke test starts on the title screen so it also covers that scene.
    // The smoke test's script is written for the built-in test meadow.
    if (options_.smokeTest) startLevel_ = "test";
    else if (!options_.startLevel.empty()) startLevel_ = options_.startLevel;

    scene_ = makeStartScene();
}

std::unique_ptr<Scene> Game::makeStartScene() {
    if (options_.skipTitle && !options_.smokeTest && !options_.menuTest)
        return std::make_unique<PlayScene>(*this, playLevel());
    const std::string& s = options_.startScene;
    if (s == "menu") return std::make_unique<TitleScene>(*this, true);
    if (s == "levels") return std::make_unique<LevelSelectScene>(*this);
    if (s == "scores") return std::make_unique<HighScoresScene>(*this, std::string(), false);
    if (s == "settings") return std::make_unique<SettingsScene>(*this);
    if (s == "controls") return std::make_unique<SettingsScene>(*this, SettingsScene::Mode::ControllerTest);
    if (s == "collection") return std::make_unique<CollectionScene>(*this);
    if (!s.empty() && s != "title") SDL_Log("[game] Unknown --scene '%s', showing the title", s.c_str());
    return std::make_unique<TitleScene>(*this);
}

std::string Game::playLevel() const {
    if (!startLevel_.empty()) return startLevel_;
    const std::string id = progress_.continueLevel(1);
    return id.empty() ? std::string("1-1") : id;
}

void Game::saveProgress() {
    if (automated()) return;
    save_->saveProgress(progress_);
}

void Game::saveSettings() {
    if (automated()) return;
    save_->saveSettings(settings_);
}

void Game::applyOutfit() {
    if (!sprites_.setOutfit(renderer_.get(), progress_.selectedOutfit()))
        SDL_Log("[game] Could not repaint the hero: %s", SDL_GetError());
}

Game::~Game() {
    // Scenes may hold textures: release them before the renderer goes away.
    pendingScene_.reset();
    scene_.reset();
    draw::releaseSkin();
}

void Game::createWindowAndRenderer() {
    // Crisp pixels: nearest-neighbour scaling everywhere.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    fullscreen_ = options_.fullscreen < 0 ? platform::defaultFullscreen() : options_.fullscreen == 1;
    const int scale = options_.windowScale > 0 ? options_.windowScale : platform::defaultWindowScale();

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (fullscreen_)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    else
        flags |= SDL_WINDOW_RESIZABLE;

    window_.reset(SDL_CreateWindow("Pocket Dash", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   kScreenWidth * scale, kScreenHeight * scale, flags));
    if (!window_) throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

    if (!options_.softwareRenderer)
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!renderer_) {
        if (!options_.softwareRenderer)
            SDL_Log("[game] Accelerated renderer unavailable (%s), using software", SDL_GetError());
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_SOFTWARE));
    }
    if (!renderer_) throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());

    SDL_RendererInfo info{};
    SDL_GetRendererInfo(renderer_.get(), &info);
    vsync_ = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    std::snprintf(rendererName_, sizeof(rendererName_), "%s%s", info.name ? info.name : "?", vsync_ ? " VSYNC" : "");

    SDL_RenderSetLogicalSize(renderer_.get(), kScreenWidth, kScreenHeight);
    // Integer scaling keeps pixels square on desktop monitors; on the
    // 640x480 handheld it is a 1:1 mapping anyway.
    SDL_RenderSetIntegerScale(renderer_.get(), SDL_TRUE);
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);

    if (fullscreen_) SDL_ShowCursor(SDL_DISABLE);

    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer_.get(), &w, &h);
    SDL_Log("[game] Renderer: %s, output %dx%d, %s", rendererName_, w, h, fullscreen_ ? "fullscreen" : "windowed");
}

void Game::changeScene(std::unique_ptr<Scene> next) { pendingScene_ = std::move(next); }

void Game::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(window_.get(), fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(fullscreen_ ? SDL_DISABLE : SDL_ENABLE);
}

void Game::processEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_KEYDOWN:
            if (!e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_F1) debug_ = !debug_;
                if (e.key.keysym.scancode == SDL_SCANCODE_F11) toggleFullscreen();
            }
            break;
        default:
            break;
        }
        input_.handleEvent(e);
    }
}

void Game::step(float dt) {
    // Handheld hotkeys (there is no F1 or window close button on the R36S):
    //   SELECT + START held  -> quit
    //   SELECT + L1          -> toggle debug overlay
    if (input_.down(Action::Select) && input_.down(Action::Start)) {
        quitComboTimer_ += dt;
        if (quitComboTimer_ >= 0.6f) {
            SDL_Log("[game] SELECT+START held - quitting");
            running_ = false;
        }
    } else {
        quitComboTimer_ = 0.0f;
    }
    if (input_.down(Action::Select) && input_.pressed(Action::L1)) debug_ = !debug_;

    scene_->update(dt);
    input_.endUpdateStep();
    simTime_ += dt;

    if (pendingScene_) scene_ = std::move(pendingScene_);
}

int Game::run() {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    Uint64 last = SDL_GetPerformanceCounter();
    fpsWindowStart_ = last;
    double accumulator = 0.0;
    const int defaultFrames = options_.smokeTest ? 260 : options_.menuTest ? kMenuTestFrames : 0;
    const int maxFrames = options_.maxFrames > 0 ? options_.maxFrames : defaultFrames;

    while (running_) {
        const Uint64 frameStart = SDL_GetPerformanceCounter();
        double frameTime = static_cast<double>(frameStart - last) / freq;
        last = frameStart;
        frameTime = std::min(frameTime, 0.25); // avoid a spiral of death after a stall
        // With vsync at ~60 Hz, snap tiny timing jitter to exactly one step so
        // we never alternate between 0 and 2 updates per frame (visible stutter).
        if (std::fabs(frameTime - kFixedDt) < 0.0008) frameTime = kFixedDt;
        accumulator += frameTime;

        processEvents();

        if (options_.smokeTest || options_.menuTest) {
            accumulator = kFixedDt; // deterministic: exactly one step per frame
            if (options_.smokeTest) applySmokeTestInput();
            else applyMenuTestInput();
        }

        int steps = 0;
        while (accumulator >= kFixedDt && steps < 4 && running_) {
            step(kFixedDt);
            accumulator -= kFixedDt;
            ++steps;
        }
        if (steps == 4) accumulator = 0.0; // running too slow: drop time rather than lag forever

        if (options_.smokeTest) checkSmokeTest();
        if (options_.menuTest) checkMenuTest();

        ++frameCount_;
        const bool lastFrame = maxFrames > 0 && frameCount_ >= maxFrames;
        if (lastFrame) running_ = false;
        render();
        if (lastFrame && !options_.screenshotPath.empty()) saveScreenshot(options_.screenshotPath);
        SDL_RenderPresent(renderer_.get());

        // FPS counter (updated twice per second).
        ++fpsFrames_;
        const Uint64 now = SDL_GetPerformanceCounter();
        const double windowSec = static_cast<double>(now - fpsWindowStart_) / freq;
        if (windowSec >= 0.5) {
            fps_ = static_cast<float>(fpsFrames_ / windowSec);
            fpsFrames_ = 0;
            fpsWindowStart_ = now;
        }
        frameMs_ = static_cast<float>(static_cast<double>(now - frameStart) / freq * 1000.0);

        // Without vsync, sleep off the rest of the frame to save battery.
        if (!vsync_ && !options_.smokeTest && !options_.menuTest) {
            const double elapsed = static_cast<double>(SDL_GetPerformanceCounter() - frameStart) / freq;
            const double remaining = kFixedDt - elapsed;
            if (remaining > 0.002) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
        }
    }

    saveSettings();

    if (options_.menuTest) {
        if (!smokeFailed_ && std::strcmp(scene_->name(), "title") != 0) {
            SDL_Log("[menus] FAIL: expected to end on the title screen, got '%s'", scene_->name());
            smokeFailed_ = true;
        }
        SDL_Log("[menus] %s", smokeFailed_ ? "FAILED" : "PASSED");
        return smokeFailed_ ? 1 : 0;
    }
    if (options_.smokeTest) {
        if (!smokeFailed_ && std::strcmp(scene_->name(), "play") != 0) {
            SDL_Log("[smoke] FAIL: expected to end in the play scene, got '%s'", scene_->name());
            smokeFailed_ = true;
        }
        SDL_Log("[smoke] %s", smokeFailed_ ? "FAILED" : "PASSED");
        return smokeFailed_ ? 1 : 0;
    }
    return 0;
}

void Game::render() {
    SDL_Renderer* r = renderer_.get();
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    scene_->render(r);
    if (debug_) {
        scene_->renderDebug(r);
        renderDebugOverlay();
    }
}

void Game::renderDebugOverlay() {
    SDL_Renderer* r = renderer_.get();
    DebugInfo info;
    scene_->fillDebugInfo(info);

    char lines[10][96];
    int n = 0;
    std::snprintf(lines[n++], 96, "FPS %.0f  %.1fMS  %s", fps_, frameMs_, rendererName_);
    std::snprintf(lines[n++], 96, "SCENE %s  LEVEL %s", scene_->name(), info.levelId);
    if (info.hasPlayer) {
        std::snprintf(lines[n++], 96, "PLAYER %.1f,%.1f Z %.1f", info.playerPos.x, info.playerPos.y, info.playerZ);
        std::snprintf(lines[n++], 96, "VEL %.0f,%.0f", info.playerVel.x, info.playerVel.y);
    }
    std::snprintf(lines[n++], 96, "ENEMIES %d  COINS %d/%d  HP %d/%d", info.enemyCount, info.coins, info.coinsTotal,
                  info.hearts, info.maxHearts);
    if (info.hasCamera) std::snprintf(lines[n++], 96, "CAMERA %.0f,%.0f", info.camera.x, info.camera.y);
    std::snprintf(lines[n++], 96, "PAD %.40s", input_.controllerName().c_str());
    char held[48];
    input_.describeHeldButtons(held, sizeof(held));
    std::snprintf(lines[n++], 96, "BTN %s  LAST %d  HAT %d", held, input_.lastPressedButton(), input_.hatValue());
    std::snprintf(lines[n++], 96, "AXES %d %d %d %d", input_.axisValue(0), input_.axisValue(1), input_.axisValue(2),
                  input_.axisValue(3));
    const Vec2 mv = input_.moveVector();
    std::snprintf(lines[n++], 96, "MOVE %.2f,%.2f", mv.x, mv.y);

    int width = 0;
    for (int i = 0; i < n; ++i) width = std::max(width, BitmapFont::textWidth(lines[i], 2));
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const int lineH = BitmapFont::lineHeight(2) + 2;
    draw::fillRect(r, 4, 52, width + 12, n * lineH + 8, SDL_Color{0, 0, 0, 170});
    for (int i = 0; i < n; ++i) font_.draw(r, 10, 56 + i * lineH, lines[i], 2, ui::kMint);
}

bool Game::saveScreenshot(const std::string& path) {
    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer_.get(), &w, &h);
    SurfacePtr shot(SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32));
    if (!shot || SDL_RenderReadPixels(renderer_.get(), nullptr, SDL_PIXELFORMAT_RGBA32, shot->pixels, shot->pitch) != 0) {
        SDL_Log("[game] Screenshot failed: %s", SDL_GetError());
        return false;
    }
    if (IMG_SavePNG(shot.get(), path.c_str()) != 0) {
        SDL_Log("[game] Could not write %s: %s", path.c_str(), IMG_GetError());
        return false;
    }
    SDL_Log("[game] Screenshot saved to %s", path.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Smoke test: drives the real game loop with scripted input and checks the
// player actually responds. Runs headless via SDL_VIDEODRIVER=dummy in CTest.
// ---------------------------------------------------------------------------

namespace {

// Scripted menu walk (--menu-test). Each entry holds one action for a few
// frames; checkMenuTest() asserts which screen is showing at key frames.
struct MenuStep {
    Action action;
    long from;
    long to; // exclusive
};
const MenuStep kMenuScript[] = {
    {Action::A, 5, 7},                                                    // title -> menu
    {Action::Down, 12, 14}, {Action::A, 18, 20},                          // LEVEL SELECT
    {Action::Right, 30, 32}, {Action::A, 40, 42},                         // locked level: refused
    {Action::X, 50, 52},                                                  // high scores for it
    {Action::Right, 62, 64}, {Action::B, 70, 72},                         // next level, back to the map
    {Action::B, 82, 84},                                                  // back to the menu
    {Action::Down, 95, 97}, {Action::Down, 100, 102}, {Action::A, 106, 108}, // COLLECTION
    {Action::Right, 118, 120}, {Action::A, 124, 126}, {Action::B, 130, 132}, // locked outfit, back
    {Action::Down, 142, 144}, {Action::Down, 147, 149}, {Action::Down, 152, 154}, {Action::Down, 157, 159},
    {Action::A, 163, 165},                                                // SETTINGS
    {Action::Right, 174, 176},                                            // music 7 -> 8
    {Action::Down, 184, 186}, {Action::Down, 189, 191}, {Action::Down, 194, 196}, {Action::Down, 199, 201},
    {Action::A, 205, 207},                                                // CONTROLLER TEST
    {Action::B, 210, 280},                                                // hold B to leave it
    {Action::Down, 285, 287}, {Action::A, 291, 293},                      // REMAP: no controller here
    {Action::Down, 296, 298}, {Action::Down, 301, 303}, {Action::A, 307, 309}, // ERASE SAVE DATA
    {Action::A, 312, 440},                                                // hold A to erase
    {Action::B, 445, 447},                                                // leave settings
    {Action::Down, 460, 462}, {Action::Down, 465, 467}, {Action::Down, 470, 472}, {Action::A, 476, 478}, // HIGH SCORES
    {Action::B, 488, 490},
    {Action::A, 500, 502},                                                // PLAY
    {Action::Start, 515, 517}, {Action::Down, 520, 522}, {Action::Down, 525, 527}, {Action::A, 531, 533}, // pause -> map
    {Action::B, 545, 547},                                                // map -> menu
};

struct MenuCheck {
    long frame;
    const char* scene;
};
const MenuCheck kMenuChecks[] = {
    {25, "levels"},  {45, "levels"},  {58, "scores"}, {78, "levels"},   {90, "title"},
    {114, "collection"}, {128, "collection"}, {138, "title"}, {170, "settings"}, {290, "settings"},
    {455, "title"},  {484, "scores"}, {496, "title"}, {510, "play"},   {540, "levels"},
};

} // namespace

void Game::applyMenuTestInput() {
    const long f = frameCount_;
    bool held[kActionCount] = {};
    for (const MenuStep& w : kMenuScript)
        if (f >= w.from && f < w.to) held[static_cast<int>(w.action)] = true;
    for (int i = 0; i < kActionCount; ++i) input_.setInjected(static_cast<Action>(i), held[i]);
}

bool Game::checkMenuTest() {
    const long f = frameCount_;
    auto fail = [&](const std::string& what) {
        SDL_Log("[menus] FAIL at frame %ld: %s", f, what.c_str());
        smokeFailed_ = true;
        running_ = false;
        return false;
    };
    for (const MenuCheck& c : kMenuChecks)
        if (f == c.frame && std::strcmp(scene_->name(), c.scene) != 0)
            return fail(std::string("expected '") + c.scene + "', showing '" + scene_->name() + "'");
    if (f == 180 && settings_.musicVolume != 8) return fail("RIGHT did not raise the music volume");
    return true;
}

void Game::applySmokeTestInput() {
    struct Window {
        Action action;
        long from;
        long to; // exclusive
    };
    static const Window kScript[] = {
        {Action::A, 5, 7},         // title -> main menu
        {Action::A, 10, 12},       // PLAY
        {Action::Right, 20, 80},   // walk right for one second
        {Action::B, 90, 92},       // dash
        {Action::A, 110, 112},     // hop
        {Action::Down, 130, 160},  // diagonal walk (the spawn is in the top row, so go down)
        {Action::Left, 130, 160},
        {Action::Start, 170, 172}, // pause
        {Action::Down, 175, 177},  // move the pause cursor (must not move the player)
        {Action::Start, 185, 187}, // resume
        {Action::R1, 190, 192},    // debug warp next to the exit (debug overlay is on)
        {Action::Up, 195, 240},    // walk into the flag
    };

    // An action is held if any of its windows covers this frame. Each action
    // is set exactly once per frame so separate windows can't fight.
    const long f = frameCount_;
    bool held[kActionCount] = {};
    for (const Window& w : kScript)
        if (f >= w.from && f < w.to) held[static_cast<int>(w.action)] = true;
    for (int i = 0; i < kActionCount; ++i) input_.setInjected(static_cast<Action>(i), held[i]);

    if (f == 100) debug_ = true;  // exercise the debug overlay (also enables the warp)
    if (f == 193) debug_ = false; // clean final frames for screenshots
}

bool Game::checkSmokeTest() {
    DebugInfo info;
    scene_->fillDebugInfo(info);
    const long f = frameCount_;
    auto fail = [&](const char* what) {
        SDL_Log("[smoke] FAIL at frame %ld: %s", f, what);
        smokeFailed_ = true;
        running_ = false;
        return false;
    };

    if (f == 15 && std::strcmp(scene_->name(), "play") != 0) return fail("A on the title screen did not start the game");
    if (!info.hasPlayer) return true;

    if (f == 19) smokeStartPos_ = info.playerPos;
    if (f == 80 && info.playerPos.x < smokeStartPos_.x + 80.0f) return fail("holding RIGHT did not move the player");
    if (f == 80 && info.coins < 2) return fail("walking over the coin row did not collect coins");
    if (f == 80 && info.coinsTotal < 10) return fail("level has suspiciously few coins");
    if (f >= 90 && f < 100) smokeMaxSpeed_ = std::max(smokeMaxSpeed_, info.playerVel.length());
    if (f == 100 && smokeMaxSpeed_ < 300.0f) return fail("B did not dash");
    if (f >= 110 && f < 140) smokeMaxZ_ = std::max(smokeMaxZ_, info.playerZ);
    if (f == 140 && smokeMaxZ_ < 10.0f) return fail("A did not hop");
    if (f == 140 && info.playerZ != 0.0f) return fail("player did not land after hopping");
    if (f == 160 && !(info.playerVel.x < 0.0f && info.playerVel.y > 0.0f)) return fail("diagonal input ignored");
    if (f == 172) smokeStartPos_ = info.playerPos;
    if (f == 183 && (info.playerPos.x != smokeStartPos_.x || info.playerPos.y != smokeStartPos_.y))
        return fail("player moved while paused");
    if (f == 240 && !info.levelClear) return fail("walking into the exit did not clear the level");
    // The scripted route avoids every hazard and enemy: any lost heart means
    // the level layout (or enemy AI) changed in a way that ambushes the start.
    if ((f == 160 || f == 240) && info.hearts != info.maxHearts) return fail("took unexpected damage on a safe route");
    if (f == 30 && info.enemyCount == 0) return fail("level has no enemies");
    return true;
}

} // namespace pd
