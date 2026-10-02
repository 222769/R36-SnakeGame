#include "Game.h"

#include "Constants.h"
#include "Draw.h"
#include "PlayScene.h"
#include "Platform.h"
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

    input_.loadConfig(platform::dataPath("config/controller.cfg"));
    input_.openJoysticks();
    debug_ = options_.debug || input_.bindings().startWithDebug;

    save_ = std::make_unique<SaveManager>(platform::saveDir());
    SDL_Log("[game] Save directory: %s", save_->saveDir().c_str());
    settings_ = save_->loadSettings();

    if (audio_.init(platform::dataPath("assets/audio/"))) {
        audio_.setMusicVolume(settings_.musicVolume);
        audio_.setSfxVolume(settings_.sfxVolume);
    }

    if (!font_.create(renderer_.get())) throw std::runtime_error("Failed to create the built-in font");
    if (!sprites_.create(renderer_.get(), platform::dataPath("assets/sprites/")))
        throw std::runtime_error("Failed to create sprites");

    // The smoke test starts on the title screen so it also covers that scene.
    if (options_.skipTitle && !options_.smokeTest)
        scene_ = std::make_unique<PlayScene>(*this);
    else
        scene_ = std::make_unique<TitleScene>(*this);
}

Game::~Game() {
    // Scenes may hold textures: release them before the renderer goes away.
    pendingScene_.reset();
    scene_.reset();
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
    const int maxFrames = options_.maxFrames > 0 ? options_.maxFrames : (options_.smokeTest ? 220 : 0);

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

        if (options_.smokeTest) {
            accumulator = kFixedDt; // deterministic: exactly one step per frame
            applySmokeTestInput();
        }

        int steps = 0;
        while (accumulator >= kFixedDt && steps < 4 && running_) {
            step(kFixedDt);
            accumulator -= kFixedDt;
            ++steps;
        }
        if (steps == 4) accumulator = 0.0; // running too slow: drop time rather than lag forever

        if (options_.smokeTest) checkSmokeTest();

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
        if (!vsync_ && !options_.smokeTest) {
            const double elapsed = static_cast<double>(SDL_GetPerformanceCounter() - frameStart) / freq;
            const double remaining = kFixedDt - elapsed;
            if (remaining > 0.002) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
        }
    }

    save_->saveSettings(settings_);

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

    char lines[9][96];
    int n = 0;
    std::snprintf(lines[n++], 96, "FPS %.0f  %.1fMS  %s", fps_, frameMs_, rendererName_);
    std::snprintf(lines[n++], 96, "SCENE %s  LEVEL %s", scene_->name(), info.levelId);
    if (info.hasPlayer) {
        std::snprintf(lines[n++], 96, "PLAYER %.1f,%.1f Z %.1f", info.playerPos.x, info.playerPos.y, info.playerZ);
        std::snprintf(lines[n++], 96, "VEL %.0f,%.0f", info.playerVel.x, info.playerVel.y);
    }
    std::snprintf(lines[n++], 96, "ENEMIES %d", info.enemyCount);
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

void Game::applySmokeTestInput() {
    struct Window {
        Action action;
        long from;
        long to; // exclusive
    };
    static const Window kScript[] = {
        {Action::A, 5, 7},         // title -> gameplay
        {Action::Right, 20, 80},   // walk right for one second
        {Action::B, 90, 92},       // dash
        {Action::A, 110, 112},     // hop
        {Action::Up, 130, 160},    // diagonal walk
        {Action::Left, 130, 160},
        {Action::Start, 170, 172}, // pause
        {Action::Down, 175, 177},  // move the pause cursor (must not move the player)
        {Action::Start, 185, 187}, // resume
    };

    // An action is held if any of its windows covers this frame. Each action
    // is set exactly once per frame so separate windows can't fight.
    const long f = frameCount_;
    bool held[kActionCount] = {};
    for (const Window& w : kScript)
        if (f >= w.from && f < w.to) held[static_cast<int>(w.action)] = true;
    for (int i = 0; i < kActionCount; ++i) input_.setInjected(static_cast<Action>(i), held[i]);

    if (f == 100) debug_ = true; // exercise the debug overlay
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
    if (f >= 90 && f < 100) smokeMaxSpeed_ = std::max(smokeMaxSpeed_, info.playerVel.length());
    if (f == 100 && smokeMaxSpeed_ < 300.0f) return fail("B did not dash");
    if (f >= 110 && f < 140) smokeMaxZ_ = std::max(smokeMaxZ_, info.playerZ);
    if (f == 140 && smokeMaxZ_ < 10.0f) return fail("A did not hop");
    if (f == 140 && info.playerZ != 0.0f) return fail("player did not land after hopping");
    if (f == 160 && !(info.playerVel.x < 0.0f && info.playerVel.y < 0.0f)) return fail("diagonal input ignored");
    if (f == 172) smokeStartPos_ = info.playerPos;
    if (f == 183 && (info.playerPos.x != smokeStartPos_.x || info.playerPos.y != smokeStartPos_.y))
        return fail("player moved while paused");
    return true;
}

} // namespace pd
