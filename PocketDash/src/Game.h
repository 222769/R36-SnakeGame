#pragma once

#include "AudioManager.h"
#include "InputManager.h"
#include "SaveManager.h"
#include "Scene.h"
#include "SdlPtr.h"
#include "Settings.h"
#include "Sprites.h"
#include "UI.h"

#include <memory>
#include <string>

namespace pd {

struct GameOptions {
    int fullscreen = -1;        // -1 = platform default, 0 = windowed, 1 = fullscreen
    int windowScale = 0;        // 0 = platform default
    bool debug = false;
    bool softwareRenderer = false;
    bool smokeTest = false;     // scripted headless run with assertions
    bool skipTitle = false;     // start straight in gameplay
    std::string startLevel;     // level the title screen starts ("" = 1-1, "test" = built-in meadow)
    std::string difficulty;     // overrides (and saves) the difficulty setting
    int maxFrames = 0;          // quit after N frames (0 = run forever)
    std::string screenshotPath; // save the final frame as PNG
};

// Owns SDL, the window, every engine service and the active scene, and
// runs the fixed-timestep main loop.
class Game {
public:
    explicit Game(const GameOptions& options);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Runs until quit. Returns the process exit code.
    int run();

    // --- Services available to scenes --------------------------------------
    SDL_Renderer* renderer() const { return renderer_.get(); }
    InputManager& input() { return input_; }
    AudioManager& audio() { return audio_; }
    SaveManager& save() { return *save_; }
    Settings& settings() { return settings_; }
    const BitmapFont& font() const { return font_; }
    const Sprites& sprites() const { return sprites_; }

    // Swaps scenes after the current update step finishes.
    void changeScene(std::unique_ptr<Scene> next);
    void quit() { running_ = false; }

    bool debugEnabled() const { return debug_; }
    // Level that "press A" on the title screen starts.
    const std::string& startLevel() const { return startLevel_; }
    // Seconds of simulated time since start (stops while minimised).
    double time() const { return simTime_; }

private:
    // Initialises SDL and its satellite libraries; shuts them down last.
    struct SdlSystem {
        SdlSystem();
        ~SdlSystem();
    };

    void createWindowAndRenderer();
    void processEvents();
    void step(float dt);
    void render();
    void renderDebugOverlay();
    void toggleFullscreen();
    bool saveScreenshot(const std::string& path);
    void applySmokeTestInput();
    bool checkSmokeTest();

    GameOptions options_;
    SdlSystem sdl_; // must be the first member: destroyed last
    WindowPtr window_;
    RendererPtr renderer_;
    InputManager input_;
    AudioManager audio_;
    std::unique_ptr<SaveManager> save_;
    Settings settings_;
    BitmapFont font_;
    Sprites sprites_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> pendingScene_;

    std::string startLevel_ = "1-1";
    bool running_ = true;
    bool debug_ = false;
    bool vsync_ = false;
    bool fullscreen_ = false;
    double simTime_ = 0.0;
    long frameCount_ = 0;
    float quitComboTimer_ = 0.0f;

    // FPS measurement.
    Uint64 fpsWindowStart_ = 0;
    int fpsFrames_ = 0;
    float fps_ = 0.0f;
    float frameMs_ = 0.0f;
    char rendererName_[32] = "?";

    // Smoke test bookkeeping.
    bool smokeFailed_ = false;
    Vec2 smokeStartPos_;
    float smokeMaxZ_ = 0.0f;
    float smokeMaxSpeed_ = 0.0f;
};

} // namespace pd
