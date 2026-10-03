#include "SettingsScene.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "Menu.h"
#include "Platform.h"
#include "TitleScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {

namespace {

enum Item { kMusic, kSfx, kShake, kDifficulty, kControllerTest, kRemap, kResetButtons, kErase, kBack, kItemCount };
constexpr const char* kLabels[kItemCount] = {"MUSIC VOLUME",  "SOUND EFFECTS", "SCREEN SHAKE",   "DIFFICULTY", "CONTROLLER TEST",
                                             "REMAP BUTTONS", "RESET BUTTONS", "ERASE SAVE DATA", "BACK"};

// Buttons asked for, in order, by the remap screen.
constexpr Action kRemapOrder[] = {Action::Up, Action::Down, Action::Left,  Action::Right, Action::A,
                                  Action::B,  Action::X,    Action::Y,     Action::L1,    Action::R1,
                                  Action::L2, Action::R2,   Action::Select, Action::Start};
constexpr int kRemapCount = static_cast<int>(sizeof(kRemapOrder) / sizeof(kRemapOrder[0]));
constexpr float kRemapStepTime = 5.0f; // no press in time = keep the current button
constexpr float kConfirmTime = 8.0f;   // no confirmation in time = revert
constexpr float kHoldToExit = 1.0f;
constexpr float kHoldToErase = 2.0f;

const char* actionLabel(Action a) {
    switch (a) {
    case Action::Up: return "D-PAD UP";
    case Action::Down: return "D-PAD DOWN";
    case Action::Left: return "D-PAD LEFT";
    case Action::Right: return "D-PAD RIGHT";
    case Action::A: return "A";
    case Action::B: return "B";
    case Action::X: return "X";
    case Action::Y: return "Y";
    case Action::L1: return "L1";
    case Action::R1: return "R1";
    case Action::L2: return "L2";
    case Action::R2: return "R2";
    case Action::Start: return "START";
    case Action::Select: return "SELECT";
    case Action::Count: break;
    }
    return "?";
}

const char* difficultyHelp(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "5 HEARTS, SLOWER FOES, 50% MORE TIME";
    case Difficulty::Challenge: return "FASTER FOES, FEWER CHECKPOINTS, SCORE x1.5";
    case Difficulty::Normal: break;
    }
    return "THE GAME AS DESIGNED";
}

// Ring of `progress` (0..1) drawn as dots, for hold-to-confirm prompts.
void drawHoldRing(SDL_Renderer* r, int cx, int cy, float progress, SDL_Color c) {
    constexpr int kDots = 20;
    for (int i = 0; i < kDots; ++i) {
        const float a = -1.5708f + 6.2832f * static_cast<float>(i) / kDots;
        const bool on = static_cast<float>(i) < progress * kDots;
        draw::fillCircle(r, cx + static_cast<int>(std::cos(a) * 16.0f), cy + static_cast<int>(std::sin(a) * 16.0f), 3,
                         on ? c : SDL_Color{255, 255, 255, 60});
    }
}

} // namespace

SettingsScene::SettingsScene(Game& game, Mode mode) : Scene(game), mode_(mode) {
    if (mode_ == Mode::Remap) startRemap();
}

void SettingsScene::leave() {
    game_.saveSettings();
    game_.changeScene(std::make_unique<TitleScene>(game_, true));
}

void SettingsScene::update(float dt) {
    time_ += dt;
    if (messageTime_ > 0.0f) messageTime_ -= dt;
    switch (mode_) {
    case Mode::Main: updateMain(); break;
    case Mode::ControllerTest: updateControllerTest(dt); break;
    case Mode::Remap: updateRemap(dt); break;
    case Mode::RemapConfirm: updateRemapConfirm(dt); break;
    case Mode::Erase: updateErase(dt); break;
    }
}

void SettingsScene::updateMain() {
    InputManager& in = game_.input();
    AudioManager& audio = game_.audio();
    Settings& s = game_.settings();
    menu::navigate(game_, index_, kItemCount);

    const int delta = menu::adjust(game_);
    const bool confirm = in.pressed(Action::A);
    switch (index_) {
    case kMusic:
        if (delta) {
            s.musicVolume = std::clamp(s.musicVolume + delta, 0, 10);
            audio.setMusicVolume(s.musicVolume);
            audio.play(Sfx::MenuMove);
        }
        break;
    case kSfx:
        if (delta) {
            s.sfxVolume = std::clamp(s.sfxVolume + delta, 0, 10);
            audio.setSfxVolume(s.sfxVolume);
            audio.play(Sfx::Coin); // hear the new level
        }
        break;
    case kShake:
        if (delta || confirm) {
            s.screenShake = !s.screenShake;
            audio.play(Sfx::MenuMove);
        }
        break;
    case kDifficulty:
        if (delta || confirm) {
            const int d = (static_cast<int>(s.difficulty) + (delta < 0 ? 2 : 1)) % 3;
            s.difficulty = static_cast<Difficulty>(d);
            audio.play(Sfx::MenuMove);
        }
        break;
    case kControllerTest:
        if (confirm) {
            audio.play(Sfx::MenuSelect);
            mode_ = Mode::ControllerTest;
            holdTime_ = 0.0f;
        }
        break;
    case kRemap:
        if (confirm) {
            audio.play(Sfx::MenuSelect);
            startRemap();
        }
        break;
    case kResetButtons:
        if (confirm) {
            audio.play(Sfx::MenuSelect);
            game_.input().loadConfig(platform::dataPath("config/controller.cfg"));
            if (!game_.automated()) std::remove(game_.save().controllerOverridePath().c_str());
            message_ = "BUTTONS RESET TO CONTROLLER.CFG";
            messageTime_ = 2.5f;
        }
        break;
    case kErase:
        if (confirm) {
            audio.play(Sfx::MenuSelect);
            mode_ = Mode::Erase;
            holdTime_ = 0.0f;
        }
        break;
    case kBack:
        if (confirm) {
            audio.play(Sfx::MenuMove);
            leave();
            return;
        }
        break;
    default: break;
    }
    if (in.pressed(Action::B)) {
        audio.play(Sfx::MenuMove);
        leave();
    }
}

void SettingsScene::updateControllerTest(float dt) {
    // Every button is being tested, so leaving needs a deliberate hold.
    holdTime_ = game_.input().down(Action::B) ? holdTime_ + dt : 0.0f;
    if (holdTime_ >= kHoldToExit) {
        game_.audio().play(Sfx::MenuMove);
        mode_ = Mode::Main;
        holdTime_ = 0.0f;
    }
}

void SettingsScene::startRemap() {
    InputManager& in = game_.input();
    if (!in.hasController()) {
        mode_ = Mode::Main;
        message_ = "NO CONTROLLER FOUND";
        messageTime_ = 2.5f;
        game_.audio().play(Sfx::Denied);
        return;
    }
    oldBindings_ = in.bindings();
    newBindings_ = in.bindings();
    assigned_.clear();
    remapStep_ = 0;
    timer_ = kRemapStepTime;
    duplicateFlash_ = false;
    in.takeRawButtonPress(); // ignore the press that opened this screen
    mode_ = Mode::Remap;
}

void SettingsScene::updateRemap(float dt) {
    InputManager& in = game_.input();
    timer_ -= dt;
    const int button = in.takeRawButtonPress();
    bool advance = timer_ <= 0.0f; // no press: keep the current button
    if (button >= 0) {
        if (std::find(assigned_.begin(), assigned_.end(), button) != assigned_.end()) {
            duplicateFlash_ = true;
            game_.audio().play(Sfx::Denied);
        } else {
            newBindings_.padButton[static_cast<size_t>(kRemapOrder[remapStep_])] = button;
            assigned_.push_back(button);
            duplicateFlash_ = false;
            game_.audio().play(Sfx::MenuMove);
            advance = true;
        }
    }
    if (!advance) return;
    ++remapStep_;
    timer_ = kRemapStepTime;
    if (remapStep_ < kRemapCount) return;
    // Try the new layout; it must be confirmed with the new A button.
    in.setBindings(newBindings_);
    mode_ = Mode::RemapConfirm;
    timer_ = kConfirmTime;
}

void SettingsScene::updateRemapConfirm(float dt) {
    InputManager& in = game_.input();
    timer_ -= dt;
    if (in.pressed(Action::A)) {
        game_.audio().play(Sfx::MenuSelect);
        const bool saved = game_.automated() || in.saveButtonBindings(game_.save().controllerOverridePath());
        message_ = saved ? "NEW BUTTONS SAVED" : "COULD NOT SAVE THE BUTTONS";
        messageTime_ = 2.5f;
        mode_ = Mode::Main;
    } else if (timer_ <= 0.0f || in.pressed(Action::B)) {
        in.setBindings(oldBindings_);
        game_.audio().play(Sfx::Denied);
        message_ = "KEPT THE OLD BUTTONS";
        messageTime_ = 2.5f;
        mode_ = Mode::Main;
    }
}

void SettingsScene::updateErase(float dt) {
    InputManager& in = game_.input();
    if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuMove);
        mode_ = Mode::Main;
        return;
    }
    holdTime_ = in.down(Action::A) ? holdTime_ + dt : 0.0f;
    if (holdTime_ >= kHoldToErase) {
        game_.progress().reset();
        game_.saveProgress();
        game_.applyOutfit();
        game_.audio().play(Sfx::Break);
        message_ = "SAVE DATA ERASED";
        messageTime_ = 2.5f;
        mode_ = Mode::Main;
        holdTime_ = 0.0f;
    }
}

// ---------------------------------------------------------------------------

void SettingsScene::render(SDL_Renderer* r) {
    game_.backdrop().render(r, time_);
    ui::dimScreen(r, 60);
    switch (mode_) {
    case Mode::Main: renderMain(r); break;
    case Mode::ControllerTest: renderControllerTest(r); break;
    case Mode::Remap:
    case Mode::RemapConfirm: renderRemap(r); break;
    case Mode::Erase:
        renderMain(r);
        renderErase(r);
        break;
    }
}

void SettingsScene::renderMain(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const Settings& s = game_.settings();
    menu::drawHeader(r, font, "SETTINGS");
    for (int i = 0; i < kItemCount; ++i) {
        const SDL_Rect row{110, 76 + i * 37, kScreenWidth - 220, 32};
        const bool sel = i == index_;
        switch (i) {
        case kMusic: menu::drawBarRow(r, font, row, kLabels[i], s.musicVolume, 10, sel); break;
        case kSfx: menu::drawBarRow(r, font, row, kLabels[i], s.sfxVolume, 10, sel); break;
        case kShake: menu::drawRow(r, font, row, kLabels[i], s.screenShake ? "ON" : "OFF", sel); break;
        case kDifficulty: menu::drawRow(r, font, row, kLabels[i], difficultyName(s.difficulty), sel); break;
        default: menu::drawRow(r, font, row, kLabels[i], {}, sel); break;
        }
    }
    const char* help = nullptr;
    if (messageTime_ > 0.0f && message_) help = message_;
    else if (index_ == kDifficulty) help = difficultyHelp(s.difficulty);
    else if (index_ == kRemap) help = "PRESS EACH BUTTON WHEN ASKED";
    else if (index_ == kErase) help = "CLEARS RECORDS, SCORES, STARS AND GEMS";
    if (help) font.drawCentered(r, kScreenWidth / 2, 412, help, 2, messageTime_ > 0.0f ? ui::kMint : ui::kWhite);
    menu::drawHints(r, font, "A SELECT    < > CHANGE    B BACK");
}

void SettingsScene::renderControllerTest(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const InputManager& in = game_.input();
    menu::drawHeader(r, font, "CONTROLLER TEST");
    const SDL_Rect box{40, 76, kScreenWidth - 80, 352};
    draw::roundedRect(r, box, SDL_Color{24, 30, 50, 225}, SDL_Color{255, 255, 255, 160}, SDL_Color{0, 0, 0, 100});

    // One lamp per action, lit while held.
    constexpr Action kShown[] = {Action::Up, Action::Down, Action::Left, Action::Right, Action::A,  Action::B,     Action::X,
                                 Action::Y,  Action::L1,   Action::R1,   Action::L2,    Action::R2, Action::Select, Action::Start};
    for (int i = 0; i < 14; ++i) {
        const Action a = kShown[i];
        const int col = i % 4, rowN = i / 4;
        const SDL_Rect pill{box.x + 18 + col * 138, box.y + 18 + rowN * 44, 128, 36};
        const bool on = in.down(a);
        draw::roundedRect(r, pill, on ? SDL_Color{246, 196, 62, 255} : SDL_Color{255, 255, 255, 30},
                          on ? SDL_Color{255, 255, 255, 255} : SDL_Color{255, 255, 255, 60});
        char label[24];
        const int b = in.bindings().padButton[static_cast<size_t>(a)];
        // Short names so "RIGHT (11)" fits the pill.
        const char* name = actionLabel(a);
        if (a == Action::Up || a == Action::Down || a == Action::Left || a == Action::Right) name += 6; // skip "D-PAD "
        std::snprintf(label, sizeof(label), b >= 0 ? "%s (%d)" : "%s", name, b);
        font.drawCentered(r, pill.x + pill.w / 2, pill.y + 10, label, 2, on ? ui::kInk : ui::kWhite, false);
    }

    char line[96];
    int y = box.y + 206;
    std::snprintf(line, sizeof(line), "CONTROLLER: %s", in.controllerName().c_str());
    font.draw(r, box.x + 20, y, line, 2, ui::kWhite);
    y += 26;
    char held[48];
    in.describeHeldButtons(held, sizeof(held));
    std::snprintf(line, sizeof(line), "RAW BUTTONS HELD: %s", held[0] ? held : "-");
    font.draw(r, box.x + 20, y, line, 2, ui::kYellow);
    y += 26;
    std::snprintf(line, sizeof(line), "HAT: %d    STICK: %d, %d    LAST BUTTON: %d", in.hatValue(), in.axisValue(0),
                  in.axisValue(1), in.lastPressedButton());
    font.draw(r, box.x + 20, y, line, 2, ui::kWhite);
    y += 26;
    font.draw(r, box.x + 20, y, "NUMBERS IN ( ) COME FROM CONFIG/CONTROLLER.CFG", 2, ui::kGrey);

    drawHoldRing(r, box.x + box.w - 40, box.y + box.h - 34, holdTime_ / kHoldToExit, ui::kYellow);
    menu::drawHints(r, font, "HOLD B TO EXIT");
}

void SettingsScene::renderRemap(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    menu::drawHeader(r, font, "REMAP BUTTONS");
    const SDL_Rect box{70, 90, kScreenWidth - 140, 300};
    draw::roundedRect(r, box, SDL_Color{24, 30, 50, 230}, SDL_Color{255, 255, 255, 170}, SDL_Color{0, 0, 0, 100});
    char line[64];
    if (mode_ == Mode::Remap) {
        std::snprintf(line, sizeof(line), "STEP %d OF %d", remapStep_ + 1, kRemapCount);
        font.drawCentered(r, kScreenWidth / 2, box.y + 20, line, 2, ui::kGrey);
        font.drawCentered(r, kScreenWidth / 2, box.y + 60, "PRESS THE BUTTON FOR", 2, ui::kWhite);
        const int pulse = static_cast<int>(std::lround(std::fabs(std::sin(time_ * 4.0f)) * 4.0f));
        font.drawCentered(r, kScreenWidth / 2, box.y + 96 - pulse, actionLabel(kRemapOrder[remapStep_]), 5, ui::kYellow);
        if (duplicateFlash_)
            font.drawCentered(r, kScreenWidth / 2, box.y + 170, "ALREADY USED - TRY ANOTHER", 2, ui::kPink);
        const int current = oldBindings_.padButton[static_cast<size_t>(kRemapOrder[remapStep_])];
        std::snprintf(line, sizeof(line), "WAIT %d S TO KEEP BUTTON %d", static_cast<int>(std::ceil(timer_)), current);
        font.drawCentered(r, kScreenWidth / 2, box.y + 210, line, 2, ui::kWhite);
        // Countdown bar.
        const int w = static_cast<int>((box.w - 80) * std::max(0.0f, timer_) / kRemapStepTime);
        draw::roundedRect(r, SDL_Rect{box.x + 40, box.y + 250, box.w - 80, 12}, SDL_Color{255, 255, 255, 40},
                          SDL_Color{0, 0, 0, 0});
        if (w > 12)
            draw::roundedRect(r, SDL_Rect{box.x + 40, box.y + 250, w, 12}, ui::kYellow, SDL_Color{0, 0, 0, 0});
        menu::drawHints(r, font, "THE D-PAD MAY BE A HAT: JUST WAIT TO SKIP");
    } else {
        font.drawCentered(r, kScreenWidth / 2, box.y + 50, "ALL DONE!", 4, ui::kYellow);
        font.drawCentered(r, kScreenWidth / 2, box.y + 120, "PRESS YOUR NEW A BUTTON TO KEEP", 2, ui::kWhite);
        font.drawCentered(r, kScreenWidth / 2, box.y + 150, "THIS LAYOUT", 2, ui::kWhite);
        std::snprintf(line, sizeof(line), "OLD BUTTONS RETURN IN %d", static_cast<int>(std::ceil(timer_)));
        font.drawCentered(r, kScreenWidth / 2, box.y + 210, line, 2, ui::kPink);
        menu::drawHints(r, font, "A KEEP    B UNDO");
    }
}

void SettingsScene::renderErase(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    ui::dimScreen(r, 140);
    const SDL_Rect box{kScreenWidth / 2 - 220, 150, 440, 180};
    draw::roundedRect(r, box, SDL_Color{60, 24, 36, 240}, SDL_Color{255, 160, 170, 220}, SDL_Color{0, 0, 0, 120});
    font.drawCentered(r, kScreenWidth / 2, box.y + 20, "ERASE ALL PROGRESS?", 3, ui::kPink);
    font.drawCentered(r, kScreenWidth / 2, box.y + 62, "RECORDS, HIGH SCORES, STARS, GEMS", 2, ui::kWhite);
    font.drawCentered(r, kScreenWidth / 2, box.y + 84, "AND OUTFITS WILL BE LOST.", 2, ui::kWhite);
    font.drawCentered(r, kScreenWidth / 2, box.y + 124, "HOLD A TO ERASE    B CANCEL", 2, ui::kYellow);
    drawHoldRing(r, box.x + box.w - 34, box.y + 132, holdTime_ / kHoldToErase, ui::kPink);
}

} // namespace pd
