#include "InputManager.h"

#include "SaveManager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace pd {

namespace {

constexpr const char* kConfigNames[kActionCount] = {
    "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT",
    "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "START", "SELECT",
};

constexpr const char* kKeyConfigNames[kActionCount] = {
    "KEY_UP", "KEY_DOWN", "KEY_LEFT", "KEY_RIGHT",
    "KEY_A", "KEY_B", "KEY_X", "KEY_Y", "KEY_L1", "KEY_R1", "KEY_L2", "KEY_R2",
    "KEY_START", "KEY_SELECT",
};

// Splits "Up, W" into scancodes. Names are SDL scancode names
// (see SDL_GetScancodeName), matched case-insensitively.
bool parseKeyList(const std::string& text, std::vector<SDL_Scancode>& out, std::string& bad) {
    out.clear();
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        if (comma == std::string::npos) comma = text.size();
        std::string name = text.substr(start, comma - start);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        if (!name.empty()) {
            const SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
            if (sc == SDL_SCANCODE_UNKNOWN) {
                bad = name;
                return false;
            }
            out.push_back(sc);
        }
        start = comma + 1;
    }
    return true;
}

} // namespace

const char* actionConfigName(Action a) { return kConfigNames[static_cast<int>(a)]; }
const char* actionKeyConfigName(Action a) { return kKeyConfigNames[static_cast<int>(a)]; }

InputBindings InputBindings::defaults() {
    InputBindings b;
    auto pad = [&](Action a, int button) { b.padButton[static_cast<int>(a)] = button; };
    auto key = [&](Action a, std::vector<SDL_Scancode> keys) { b.keys[static_cast<int>(a)] = std::move(keys); };

    // Default pad layout used by ArkOS on RK3326 handhelds ("GO-Super
    // Gamepad"): d-pad reported as buttons 8-11. Other firmwares differ —
    // config/controller.cfg overrides all of these.
    pad(Action::A, 1);
    pad(Action::B, 0);
    pad(Action::X, 2);
    pad(Action::Y, 3);
    pad(Action::L1, 4);
    pad(Action::R1, 5);
    pad(Action::L2, 6);
    pad(Action::R2, 7);
    pad(Action::Up, 8);
    pad(Action::Down, 9);
    pad(Action::Left, 10);
    pad(Action::Right, 11);
    pad(Action::Select, 12);
    pad(Action::Start, 13);

    // Keyboard (PC development). The spec's action keys Z/X/A/S clash with
    // WASD movement, so arrows are the default; a WASD layout can be chosen
    // in config/controller.cfg.
    key(Action::Up, {SDL_SCANCODE_UP});
    key(Action::Down, {SDL_SCANCODE_DOWN});
    key(Action::Left, {SDL_SCANCODE_LEFT});
    key(Action::Right, {SDL_SCANCODE_RIGHT});
    key(Action::A, {SDL_SCANCODE_Z, SDL_SCANCODE_SPACE});
    key(Action::B, {SDL_SCANCODE_X, SDL_SCANCODE_LSHIFT});
    key(Action::X, {SDL_SCANCODE_A});
    key(Action::Y, {SDL_SCANCODE_S});
    key(Action::L1, {SDL_SCANCODE_Q});
    key(Action::R1, {SDL_SCANCODE_W});
    key(Action::L2, {SDL_SCANCODE_1});
    key(Action::R2, {SDL_SCANCODE_2});
    key(Action::Start, {SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE});
    key(Action::Select, {SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB});
    return b;
}

void InputBindings::apply(const KeyValueStore& kv, std::vector<std::string>* warnings) {
    auto warn = [&](const std::string& msg) {
        if (warnings) warnings->push_back(msg);
    };

    for (int i = 0; i < kActionCount; ++i) {
        if (kv.has(kConfigNames[i])) {
            const int v = kv.getInt(kConfigNames[i], -2);
            if (v < -1 || v >= 64)
                warn(std::string("Invalid button number for ") + kConfigNames[i]);
            else
                padButton[i] = v;
        }
        if (kv.has(kKeyConfigNames[i])) {
            std::vector<SDL_Scancode> parsed;
            std::string bad;
            if (parseKeyList(kv.getString(kKeyConfigNames[i]), parsed, bad))
                keys[i] = std::move(parsed);
            else
                warn(std::string("Unknown key name '") + bad + "' for " + kKeyConfigNames[i]);
        }
    }

    axisX = kv.getInt("AXIS_X", axisX);
    axisY = kv.getInt("AXIS_Y", axisY);
    deadzone = std::clamp(kv.getInt("DEADZONE", deadzone), 0, 32000);
    invertX = kv.getBool("INVERT_X", invertX);
    invertY = kv.getBool("INVERT_Y", invertY);
    useHat = kv.getBool("USE_HAT", useHat);
    startWithDebug = kv.getBool("DEBUG_OVERLAY", startWithDebug);
}

// ---------------------------------------------------------------------------

InputManager::InputManager() : bindings_(InputBindings::defaults()) {}

InputManager::~InputManager() = default;

void InputManager::loadConfig(const std::string& path) {
    bindings_ = InputBindings::defaults();
    KeyValueStore kv;
    if (!kv.loadFromFile(path)) {
        SDL_Log("[input] %s not found, using default bindings", path.c_str());
        return;
    }
    std::vector<std::string> warnings;
    bindings_.apply(kv, &warnings);
    for (const auto& w : warnings) SDL_Log("[input] Config warning: %s", w.c_str());
    SDL_Log("[input] Loaded bindings from %s", path.c_str());
}

void InputManager::openJoysticks() {
    const int count = SDL_NumJoysticks();
    SDL_Log("[input] %d joystick(s) detected", count);
    for (int i = 0; i < count; ++i) openJoystick(i);
}

void InputManager::openJoystick(int deviceIndex) {
    const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(deviceIndex);
    for (const auto& j : joysticks_)
        if (j.id == id) return; // already open

    JoystickPtr js(SDL_JoystickOpen(deviceIndex));
    if (!js) {
        SDL_Log("[input] Failed to open joystick %d: %s", deviceIndex, SDL_GetError());
        return;
    }

    char guid[64];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js.get()), guid, sizeof(guid));
    const char* name = SDL_JoystickName(js.get());
    SDL_Log("[input] Opened joystick %d: \"%s\"", deviceIndex, name ? name : "?");
    SDL_Log("[input]   GUID %s, %d buttons, %d axes, %d hats", guid,
            SDL_JoystickNumButtons(js.get()), SDL_JoystickNumAxes(js.get()),
            SDL_JoystickNumHats(js.get()));
    if (SDL_IsGameController(deviceIndex)) {
        // Informational only: SDL's mapping can help users fill in
        // controller.cfg, but the game always uses the raw button numbers.
        if (char* mapping = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUID(js.get()))) {
            SDL_Log("[input]   SDL mapping: %s", mapping);
            SDL_free(mapping);
        }
    }

    joysticks_.push_back({id, std::move(js)});
    refreshControllerName();
}

void InputManager::closeJoystick(SDL_JoystickID id) {
    auto it = std::find_if(joysticks_.begin(), joysticks_.end(),
                           [id](const OpenJoystick& j) { return j.id == id; });
    if (it == joysticks_.end()) return;
    SDL_Log("[input] Joystick disconnected: \"%s\"", SDL_JoystickName(it->handle.get()));
    joysticks_.erase(it);

    // Release everything the pad was holding so nothing gets stuck.
    for (int i = 0; i < kActionCount; ++i) {
        padDown_[i] = 0;
        hatDown_[i] = 0;
        axisDown_[i] = 0;
    }
    rawButtons_.fill(false);
    axes_.fill(0);
    hat_ = 0;
    refreshControllerName();
}

void InputManager::refreshControllerName() {
    if (joysticks_.empty()) {
        controllerName_ = "NONE";
        return;
    }
    const char* name = SDL_JoystickName(joysticks_.front().handle.get());
    controllerName_ = name ? name : "UNKNOWN";
}

void InputManager::setSource(SourceArray& src, Action a, bool value) {
    const bool before = down(a);
    src[idx(a)] = value ? 1 : 0;
    if (!before && down(a)) latched_[idx(a)] = true;
}

void InputManager::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        if (e.key.repeat) break;
        const bool isDown = e.type == SDL_KEYDOWN;
        for (int i = 0; i < kActionCount; ++i) {
            for (SDL_Scancode sc : bindings_.keys[i]) {
                if (sc == e.key.keysym.scancode) {
                    // Recompute from the keyboard state so two keys bound to
                    // the same action don't release each other.
                    const Uint8* state = SDL_GetKeyboardState(nullptr);
                    bool any = isDown;
                    for (SDL_Scancode other : bindings_.keys[i])
                        if (state[other]) any = true;
                    setSource(keyDown_, static_cast<Action>(i), any);
                    break;
                }
            }
        }
        break;
    }
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP: {
        const int button = e.jbutton.button;
        const bool isDown = e.jbutton.state == SDL_PRESSED;
        if (button < kMaxButtons) rawButtons_[button] = isDown;
        if (isDown) {
            lastButton_ = button;
            rawPress_ = button;
        }
        for (int i = 0; i < kActionCount; ++i)
            if (bindings_.padButton[i] == button) setSource(padDown_, static_cast<Action>(i), isDown);
        break;
    }
    case SDL_JOYHATMOTION:
        if (e.jhat.hat == 0) updateHat(e.jhat.value);
        break;
    case SDL_JOYAXISMOTION:
        updateAxis(e.jaxis.axis, e.jaxis.value);
        break;
    case SDL_JOYDEVICEADDED:
        openJoystick(e.jdevice.which);
        break;
    case SDL_JOYDEVICEREMOVED:
        closeJoystick(e.jdevice.which);
        break;
    default:
        break;
    }
}

void InputManager::updateHat(int value) {
    hat_ = value;
    if (!bindings_.useHat) return;
    setSource(hatDown_, Action::Up, (value & SDL_HAT_UP) != 0);
    setSource(hatDown_, Action::Down, (value & SDL_HAT_DOWN) != 0);
    setSource(hatDown_, Action::Left, (value & SDL_HAT_LEFT) != 0);
    setSource(hatDown_, Action::Right, (value & SDL_HAT_RIGHT) != 0);
}

void InputManager::updateAxis(int axis, int value) {
    if (axis >= 0 && axis < kMaxAxes) axes_[axis] = value;

    // Digital view of the stick, used for menus. Uses a higher threshold
    // than the analogue deadzone so menus don't scroll on a slight tilt.
    const int threshold = std::clamp(bindings_.deadzone * 2, 16000, 28000);
    if (axis == bindings_.axisX) {
        const int v = bindings_.invertX ? -value : value;
        setSource(axisDown_, Action::Left, v < -threshold);
        setSource(axisDown_, Action::Right, v > threshold);
    } else if (axis == bindings_.axisY) {
        const int v = bindings_.invertY ? -value : value;
        setSource(axisDown_, Action::Up, v < -threshold);
        setSource(axisDown_, Action::Down, v > threshold);
    }
}

void InputManager::endUpdateStep() {
    latched_.fill(false);
    for (int i = 0; i < kActionCount; ++i) heldSteps_[i] = down(static_cast<Action>(i)) ? heldSteps_[i] + 1 : 0;
}

bool InputManager::repeated(Action a) const {
    if (pressed(a)) return true;
    const int held = heldSteps_[idx(a)];
    return held >= kRepeatDelay && (held - kRepeatDelay) % kRepeatInterval == 0;
}

int InputManager::takeRawButtonPress() {
    const int b = rawPress_;
    rawPress_ = -1;
    return b;
}

void InputManager::loadOverrides(const std::string& path) {
    KeyValueStore kv;
    if (!kv.loadFromFile(path)) return;
    std::vector<std::string> warnings;
    bindings_.apply(kv, &warnings);
    for (const auto& w : warnings) SDL_Log("[input] Override warning: %s", w.c_str());
    SDL_Log("[input] Applied button overrides from %s", path.c_str());
}

void InputManager::setBindings(const InputBindings& bindings) {
    for (int i = 0; i < kActionCount; ++i) setSource(padDown_, static_cast<Action>(i), false);
    bindings_ = bindings;
    // Re-press anything still physically held under the new numbers.
    for (int i = 0; i < kActionCount; ++i) {
        const int b = bindings_.padButton[i];
        if (b >= 0 && b < kMaxButtons && rawButtons_[b]) padDown_[i] = 1;
    }
}

bool InputManager::saveButtonBindings(const std::string& path) const {
    KeyValueStore kv;
    for (int i = 0; i < kActionCount; ++i) kv.set(kConfigNames[i], bindings_.padButton[i]);
    return kv.saveToFile(path);
}

bool InputManager::down(Action a) const {
    const int i = idx(a);
    return keyDown_[i] || padDown_[i] || hatDown_[i] || axisDown_[i] || injected_[i];
}

Vec2 InputManager::moveVector() const {
    // Digital directions first: d-pad / keys give crisp 8-way movement.
    const auto isDigital = [this](Action a) {
        const int i = idx(a);
        return keyDown_[i] || padDown_[i] || hatDown_[i] || injected_[i];
    };
    Vec2 digital{
        static_cast<float>((isDigital(Action::Right) ? 1 : 0) - (isDigital(Action::Left) ? 1 : 0)),
        static_cast<float>((isDigital(Action::Down) ? 1 : 0) - (isDigital(Action::Up) ? 1 : 0))};
    if (digital.lengthSq() > 0.0f) return digital.normalized();

    // Analogue stick with a radial deadzone, rescaled so movement starts
    // smoothly from zero just outside the deadzone.
    const float ax = (bindings_.axisX >= 0 && bindings_.axisX < kMaxAxes) ? axes_[bindings_.axisX] : 0;
    const float ay = (bindings_.axisY >= 0 && bindings_.axisY < kMaxAxes) ? axes_[bindings_.axisY] : 0;
    Vec2 stick{(bindings_.invertX ? -ax : ax) / 32767.0f, (bindings_.invertY ? -ay : ay) / 32767.0f};
    const float len = stick.length();
    const float dz = bindings_.deadzone / 32767.0f;
    if (len <= dz || dz >= 1.0f) return {};
    const float scaled = std::min(1.0f, (len - dz) / (1.0f - dz));
    return stick * (scaled / len);
}

void InputManager::setInjected(Action a, bool isDown) { setSource(injected_, a, isDown); }

void InputManager::clearInjected() {
    for (int i = 0; i < kActionCount; ++i) setSource(injected_, static_cast<Action>(i), false);
}

void InputManager::describeHeldButtons(char* out, size_t size) const {
    if (size == 0) return;
    out[0] = '\0';
    size_t used = 0;
    for (int b = 0; b < kMaxButtons && used + 4 < size; ++b) {
        if (!rawButtons_[b]) continue;
        const int n = std::snprintf(out + used, size - used, used ? " %d" : "%d", b);
        if (n < 0) break;
        used += static_cast<size_t>(n);
    }
    if (used == 0) std::snprintf(out, size, "-");
}

int InputManager::axisValue(int axis) const {
    return (axis >= 0 && axis < kMaxAxes) ? axes_[axis] : 0;
}

} // namespace pd
