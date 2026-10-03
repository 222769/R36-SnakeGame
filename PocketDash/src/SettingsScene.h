#pragma once

#include "InputManager.h"
#include "Scene.h"

#include <vector>

namespace pd {

// Options: volumes, screen shake, difficulty, a controller test, button
// remapping and erasing the save. Settings apply immediately and are saved
// when leaving the screen.
class SettingsScene : public Scene {
public:
    enum class Mode { Main, ControllerTest, Remap, RemapConfirm, Erase };

    explicit SettingsScene(Game& game, Mode mode = Mode::Main);

    const char* name() const override { return "settings"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    void updateMain();
    void updateControllerTest(float dt);
    void updateRemap(float dt);
    void updateRemapConfirm(float dt);
    void updateErase(float dt);
    void startRemap();
    void leave();

    void renderMain(SDL_Renderer* r) const;
    void renderControllerTest(SDL_Renderer* r) const;
    void renderRemap(SDL_Renderer* r) const;
    void renderErase(SDL_Renderer* r) const;

    Mode mode_ = Mode::Main;
    int index_ = 0;
    float time_ = 0.0f;
    float holdTime_ = 0.0f;  // hold-to-exit / hold-to-erase progress
    float timer_ = 0.0f;     // remap step / confirmation countdown
    int remapStep_ = 0;
    InputBindings oldBindings_;
    InputBindings newBindings_;
    std::vector<int> assigned_; // buttons picked in this remap
    bool duplicateFlash_ = false;
    const char* message_ = nullptr; // short result line under the menu
    float messageTime_ = 0.0f;
};

} // namespace pd
