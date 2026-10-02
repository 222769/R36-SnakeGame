#pragma once

#include "Math.h"
#include "SdlPtr.h"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pd {

class KeyValueStore;

// Logical game actions. Gameplay code only ever asks about these, never
// about physical keys or button numbers.
enum class Action : int {
    Up, Down, Left, Right,
    A,      // jump / action / confirm
    B,      // dash / back
    X,      // use power-up
    Y,      // interact
    L1, R1, L2, R2,
    Start,  // pause
    Select, // map / level info
    Count
};

constexpr int kActionCount = static_cast<int>(Action::Count);

// Name used in config files ("DPAD_UP", "A", "START", ...).
const char* actionConfigName(Action a);
// Name used for keyboard bindings in config files ("KEY_UP", "KEY_A", ...).
const char* actionKeyConfigName(Action a);

// Physical bindings. Loaded from config/controller.cfg.
//
// The R36S exposes its controls as a plain SDL joystick, and the button
// numbers differ between firmware images, so we deliberately use the raw
// joystick API and let the config file decide what each number means.
struct InputBindings {
    std::array<int, kActionCount> padButton{};                 // -1 = unbound
    std::array<std::vector<SDL_Scancode>, kActionCount> keys;  // any of these keys

    int axisX = 0;           // left stick horizontal axis (-1 = disabled)
    int axisY = 1;           // left stick vertical axis   (-1 = disabled)
    int deadzone = 8000;     // 0..32767
    bool invertX = false;
    bool invertY = false;
    bool useHat = true;      // treat joystick hat 0 as a d-pad
    bool startWithDebug = false;

    static InputBindings defaults();

    // Overrides defaults with any keys present in `kv`. Unknown or malformed
    // values are reported in `warnings` and ignored.
    void apply(const KeyValueStore& kv, std::vector<std::string>* warnings = nullptr);
};

class InputManager {
public:
    InputManager();
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Loads bindings; missing file = built-in defaults.
    void loadConfig(const std::string& path);
    const InputBindings& bindings() const { return bindings_; }

    // Opens every joystick currently connected (hot-plug is handled through
    // events afterwards). Requires SDL_INIT_JOYSTICK.
    void openJoysticks();

    void handleEvent(const SDL_Event& e);

    // Call after each fixed simulation step: presses are latched until one
    // step has seen them, so taps shorter than a frame are never lost and a
    // press is never seen twice when a frame runs multiple steps.
    void endUpdateStep();

    bool down(Action a) const;
    bool pressed(Action a) const { return latched_[idx(a)]; }

    // Combined movement vector (length <= 1). Digital inputs win over the
    // analogue stick; the stick keeps its magnitude for gentle walking.
    Vec2 moveVector() const;

    // Scripted input for automated tests / attract mode.
    void setInjected(Action a, bool isDown);
    void clearInjected();

    // Diagnostics for the debug overlay and logs.
    bool hasController() const { return !joysticks_.empty(); }
    const std::string& controllerName() const { return controllerName_; }
    // Writes held raw button numbers, e.g. "1 5 12", into `out`.
    void describeHeldButtons(char* out, size_t size) const;
    int hatValue() const { return hat_; }
    int axisValue(int axis) const;
    int lastPressedButton() const { return lastButton_; }

private:
    static constexpr int idx(Action a) { return static_cast<int>(a); }
    static constexpr int kMaxButtons = 64;
    static constexpr int kMaxAxes = 8;

    using SourceArray = std::array<uint8_t, kActionCount>;
    void setSource(SourceArray& src, Action a, bool value);
    void openJoystick(int deviceIndex);
    void closeJoystick(SDL_JoystickID id);
    void updateHat(int value);
    void updateAxis(int axis, int value);
    void refreshControllerName();

    InputBindings bindings_;

    SourceArray keyDown_{};
    SourceArray padDown_{};
    SourceArray hatDown_{};
    SourceArray axisDown_{};
    SourceArray injected_{};
    std::array<bool, kActionCount> latched_{};

    struct OpenJoystick {
        SDL_JoystickID id;
        JoystickPtr handle;
    };
    std::vector<OpenJoystick> joysticks_;
    std::string controllerName_ = "NONE";

    std::array<bool, kMaxButtons> rawButtons_{};
    std::array<int, kMaxAxes> axes_{};
    int hat_ = 0;
    int lastButton_ = -1;
};

} // namespace pd
