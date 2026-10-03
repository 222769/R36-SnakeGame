#pragma once

#include "AudioManager.h"
#include "Backdrop.h"
#include "InputManager.h"
#include "Progress.h"
#include "SaveManager.h"
#include "Scene.h"
#include "SdlPtr.h"
#include "Settings.h"
#include "Sprites.h"
#include "UI.h"

#include <memory>
#include <string>
#include <vector>

namespace pd {

struct GameOptions {
    int fullscreen = -1;        // -1 = platform default, 0 = windowed, 1 = fullscreen
    int windowScale = 0;        // 0 = platform default
    bool debug = false;
    bool softwareRenderer = false;
    bool smokeTest = false;     // scripted headless run with assertions
    bool menuTest = false;      // scripted headless walk through every menu screen
    bool demoProgress = false;  // sample progress (not saved), for screenshots
    std::string startScene;     // title | menu | levels | scores | settings | collection | controls
    bool skipTitle = false;     // start straight in gameplay
    std::string startLevel;     // level the title screen starts ("" = 1-1, "test" = built-in meadow)
    std::string difficulty;     // overrides (and saves) the difficulty setting
    int maxFrames = 0;          // quit after N frames (0 = run forever)
    std::string screenshotPath; // save the final frame as PNG
};

// Length of the --menu-test script, in frames.
constexpr int kMenuTestFrames = 560;

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
    // Level "Play" starts: the --level override, else the first unfinished one.
    std::string playLevel() const;

    Progress& progress() { return progress_; }
    // Writes progress.ini (skipped in automated runs so tests never touch
    // the real save).
    void saveProgress();
    void saveSettings();
    // Repaints the hero in the selected outfit.
    void applyOutfit();
    // Scripted / frame-limited runs: nothing is written to the save folder.
    bool automated() const { return options_.smokeTest || options_.menuTest || options_.maxFrames > 0; }
    const Backdrop& backdrop() const { return backdrop_; }
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
    void renderTransition();
    void toggleFullscreen();
    bool saveScreenshot(const std::string& path);
    void applySmokeTestInput();
    bool checkSmokeTest();
    void applyMenuTestInput();
    bool checkMenuTest();
    std::unique_ptr<Scene> makeStartScene();

    GameOptions options_;
    SdlSystem sdl_; // must be the first member: destroyed last
    WindowPtr window_;
    RendererPtr renderer_;
    InputManager input_;
    AudioManager audio_;
    std::unique_ptr<SaveManager> save_;
    Settings settings_;
    Progress progress_;
    Backdrop backdrop_;
    BitmapFont font_;
    Sprites sprites_;
    std::unique_ptr<Scene> scene_;
    std::unique_ptr<Scene> pendingScene_;

    // Iris wipe between scenes: the old scene closes into a circle, then the
    // new one opens out of it. Off in scripted tests (they count frames).
    enum class Transition { None, Closing, Opening };
    static constexpr float kIrisTime = 0.32f;
    bool transitionsEnabled_ = true;
    Transition transition_ = Transition::None;
    float transitionTime_ = 0.0f;
    Vec2 irisCenter_;
    std::vector<SDL_Rect> irisRects_;

    std::string startLevel_; // --level override ("" = continue from progress)
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
