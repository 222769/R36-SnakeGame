// Pocket Dash unit tests. Dependency-free: no SDL window or audio needed.

#include "BuiltinLevels.h"
#include "Boss.h"
#include "Camera.h"
#include "Collectibles.h"
#include "Effects.h"
#include "Enemy.h"
#include "InputManager.h"
#include "LevelLoader.h"
#include "LevelSession.h"
#include "Level.h"
#include "Player.h"
#include "AudioManager.h"
#include "PowerUp.h"
#include "Progress.h"
#include "SaveManager.h"
#include "Score.h"
#include "Sprites.h"
#include "Synth.h"
#include "UI.h"
#include "World.h"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <functional>
#include <string>
#include <vector>

using namespace pd;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failures;                                                              \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
        }                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

constexpr float kDt = 1.0f / 60.0f;

// Runs the player for `steps` fixed steps with the same input.
void simulate(Player& p, PlayerInput in, int steps) {
    for (int i = 0; i < steps; ++i) {
        p.update(in, kDt);
        in.hopPressed = false; // presses are one-shot
        in.dashPressed = false;
    }
}

// --- KeyValueStore -----------------------------------------------------------

void testKeyValueParsing() {
    KeyValueStore kv;
    kv.parse("# comment\n"
             "; also comment\n"
             "  A = 1 \n"
             "NAME=Pocket Dash\n"
             "FLAG=yes\n"
             "BAD=12abc\n"
             "[section]\n"
             "inner=4\n"
             "noequals\n"
             "A=7\n");
    CHECK(kv.getInt("A", -1) == 1);
    CHECK(kv.getString("NAME") == "Pocket Dash");
    CHECK(kv.getBool("FLAG", false));
    CHECK(kv.getInt("BAD", 99) == 99);
    CHECK(kv.getInt("section.inner", 0) == 4);
    CHECK(kv.getInt("section.A", 0) == 7);
    CHECK(!kv.has("noequals"));
    CHECK(kv.getInt("missing", 5) == 5);
}

void testKeyValueRoundTrip() {
    const std::string path = (std::filesystem::temp_directory_path() / "pocketdash_test.ini").string();
    KeyValueStore out;
    out.set("music_volume", 3);
    out.set("difficulty", std::string("CHALLENGE"));
    out.set("screen_shake", false);
    CHECK(out.saveToFile(path));

    KeyValueStore in;
    CHECK(in.loadFromFile(path));
    CHECK(in.getInt("music_volume", 0) == 3);
    CHECK(in.getString("difficulty") == "CHALLENGE");
    CHECK(!in.getBool("screen_shake", true));
    std::filesystem::remove(path);
}

void testSettingsRoundTrip() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pocketdash_save_test";
    std::filesystem::create_directories(dir);
    SaveManager save(dir.string() + "/");

    Settings s;
    s.musicVolume = 2;
    s.sfxVolume = 10;
    s.screenShake = false;
    s.difficulty = Difficulty::Relaxed;
    CHECK(save.saveSettings(s));

    const Settings loaded = save.loadSettings();
    CHECK(loaded.musicVolume == 2);
    CHECK(loaded.sfxVolume == 10);
    CHECK(!loaded.screenShake);
    CHECK(loaded.difficulty == Difficulty::Relaxed);
    std::filesystem::remove_all(dir);
}

// --- Input bindings ----------------------------------------------------------

void testBindingsFromConfig() {
    KeyValueStore kv;
    kv.parse("A=0\nB=1\nSTART=6\nSELECT=7\nDPAD_UP=-1\nAXIS_Y=3\nINVERT_Y=1\nDEADZONE=99999\n"
             "KEY_A=J, Space\nKEY_B=NotAKey\nX=100\n");
    InputBindings b = InputBindings::defaults();
    std::vector<std::string> warnings;
    b.apply(kv, &warnings);

    CHECK(b.padButton[static_cast<int>(Action::A)] == 0);
    CHECK(b.padButton[static_cast<int>(Action::B)] == 1);
    CHECK(b.padButton[static_cast<int>(Action::Start)] == 6);
    CHECK(b.padButton[static_cast<int>(Action::Select)] == 7);
    CHECK(b.padButton[static_cast<int>(Action::Up)] == -1);
    CHECK(b.padButton[static_cast<int>(Action::X)] == 2); // invalid 100 ignored
    CHECK(b.axisY == 3);
    CHECK(b.invertY);
    CHECK(b.deadzone == 32000); // clamped
    const auto& keysA = b.keys[static_cast<int>(Action::A)];
    CHECK(keysA.size() == 2 && keysA[0] == SDL_SCANCODE_J && keysA[1] == SDL_SCANCODE_SPACE);
    CHECK(!b.keys[static_cast<int>(Action::B)].empty()); // bad key name keeps defaults
    CHECK(warnings.size() == 2);
}

void testInputLatching() {
    InputManager input;
    CHECK(!input.down(Action::A));
    input.setInjected(Action::A, true);
    CHECK(input.down(Action::A));
    CHECK(input.pressed(Action::A));
    input.endUpdateStep();
    CHECK(input.down(Action::A));
    CHECK(!input.pressed(Action::A)); // a press is seen by exactly one step

    // A tap shorter than a frame is still delivered.
    input.setInjected(Action::B, true);
    input.setInjected(Action::B, false);
    CHECK(!input.down(Action::B));
    CHECK(input.pressed(Action::B));
    input.endUpdateStep();

    input.clearInjected();
    input.setInjected(Action::Right, true);
    input.setInjected(Action::Down, true);
    const Vec2 mv = input.moveVector();
    CHECK_NEAR(mv.length(), 1.0f, 1e-4f);
    CHECK(mv.x > 0.0f && mv.y > 0.0f);
}

void testJoystickEvents() {
    InputManager input; // default bindings: A = button 1, d-pad up = button 8
    SDL_Event e{};
    e.type = SDL_JOYBUTTONDOWN;
    e.jbutton.button = 1;
    e.jbutton.state = SDL_PRESSED;
    input.handleEvent(e);
    CHECK(input.down(Action::A));
    CHECK(input.pressed(Action::A));
    CHECK(input.lastPressedButton() == 1);

    char held[32];
    input.describeHeldButtons(held, sizeof(held));
    CHECK(std::string(held) == "1");

    e.type = SDL_JOYBUTTONUP;
    e.jbutton.state = SDL_RELEASED;
    input.handleEvent(e);
    CHECK(!input.down(Action::A));

    // Hat d-pad.
    SDL_Event hat{};
    hat.type = SDL_JOYHATMOTION;
    hat.jhat.hat = 0;
    hat.jhat.value = SDL_HAT_LEFTUP;
    input.handleEvent(hat);
    CHECK(input.down(Action::Left) && input.down(Action::Up));

    // Analogue stick: below deadzone = no movement, full tilt = length 1.
    hat.jhat.value = SDL_HAT_CENTERED;
    input.handleEvent(hat);
    SDL_Event axis{};
    axis.type = SDL_JOYAXISMOTION;
    axis.jaxis.axis = 0;
    axis.jaxis.value = 4000;
    input.handleEvent(axis);
    CHECK(input.moveVector().lengthSq() == 0.0f);
    axis.jaxis.value = 32767;
    input.handleEvent(axis);
    CHECK_NEAR(input.moveVector().x, 1.0f, 1e-3f);
    CHECK(input.down(Action::Right)); // digital view for menus
}

// --- Player ------------------------------------------------------------------

void testPlayerAcceleratesToMaxSpeed() {
    Player p({100, 100});
    PlayerInput in;
    in.move = {1, 0};
    simulate(p, in, 60);
    CHECK_NEAR(p.velocity().x, p.tuning().maxSpeed, 0.5f);
    CHECK(p.position().x > 200.0f);
    CHECK(p.facing() == Facing::Right);

    // Releasing the stick stops the player quickly (< 0.2 s).
    simulate(p, PlayerInput{}, 12);
    CHECK_NEAR(p.velocity().length(), 0.0f, 0.01f);
}

void testPlayerDiagonalNotFaster() {
    Player p({0, 0});
    PlayerInput in;
    in.move = {1, 1}; // un-normalised input must be clamped
    simulate(p, in, 60);
    CHECK(p.velocity().length() <= p.tuning().maxSpeed + 0.5f);
}

void testPlayerDash() {
    Player p({0, 0});
    PlayerInput in;
    in.dashPressed = true; // idle dash goes the way the player faces (down)
    const unsigned events = p.update(in, kDt);
    CHECK(events & kEventDashed);
    CHECK(p.isDashing());
    CHECK_NEAR(p.velocity().y, p.tuning().dashSpeed, 0.01f);

    // Dash ends, then cooldown blocks an immediate second dash.
    simulate(p, PlayerInput{}, 10);
    CHECK(!p.isDashing());
    in.dashPressed = true;
    CHECK((p.update(in, kDt) & kEventDashed) == 0);
    simulate(p, PlayerInput{}, 30);
    CHECK((p.update(in, kDt) & kEventDashed) != 0);

    // Dash distance is meaningful: ~ dashSpeed * dashTime.
    Player q({0, 0});
    PlayerInput right;
    right.move = {1, 0};
    right.dashPressed = true;
    simulate(q, right, 9);
    CHECK(q.position().x > 60.0f);
}

void testPlayerHop() {
    Player p({0, 0});
    PlayerInput in;
    in.hopPressed = true;
    CHECK(p.update(in, kDt) & kEventHopped);
    CHECK(p.isAirborne());

    // Cannot double-hop in the air.
    CHECK((p.update(in, kDt) & kEventHopped) == 0);

    float peak = 0.0f;
    bool landed = false;
    for (int i = 0; i < 60 && !landed; ++i) {
        landed = (p.update(PlayerInput{}, kDt) & kEventLanded) != 0;
        peak = std::max(peak, p.height());
    }
    CHECK(landed);
    CHECK(peak > 12.0f && peak < 30.0f);
    CHECK(p.height() == 0.0f);
}

void testPlayerKnockbackAndInvincibility() {
    Player p({100, 100});
    CHECK(p.takeHit({90, 100})); // hit from the left
    CHECK(p.velocity().x > 0.0f);
    CHECK(p.isInvincible());
    CHECK(p.isStunned());
    CHECK(!p.takeHit({90, 100})); // no double hits

    // Stunned: input is ignored briefly.
    PlayerInput left;
    left.move = {-1, 0};
    p.update(left, kDt);
    CHECK(p.velocity().x > 0.0f);

    simulate(p, PlayerInput{}, 120);
    CHECK(!p.isInvincible());
}

void testPlayerConstrain() {
    Player p({5, 5});
    p.constrainTo(RectF{0, 0, 100, 100});
    const RectF box = p.hitbox();
    CHECK(box.left() >= 0.0f && box.top() >= 0.0f);
    p.setPosition({500, 500});
    p.constrainTo(RectF{0, 0, 100, 100});
    CHECK(p.hitbox().right() <= 100.0f && p.hitbox().bottom() <= 100.0f);
}

// --- Data tables -------------------------------------------------------------

void testArtAndFontData() {
    std::string error;
    const bool artOk = Sprites::validateBuiltinArt(&error);
    if (!artOk) std::printf("  art error: %s\n", error.c_str());
    CHECK(artOk);
    error.clear();
    const bool fontOk = BitmapFont::validateGlyphData(&error);
    if (!fontOk) std::printf("  font error: %s\n", error.c_str());
    CHECK(fontOk);
    CHECK(BitmapFont::textWidth("AB", 2) == 22);
}

void testPowerUps() {
    PowerUpState s;
    s.store(PowerUpType::SpeedShoes);
    CHECK(s.activateStored() == PowerUpType::SpeedShoes);
    CHECK(s.active(PowerUpType::SpeedShoes));
    CHECK(s.stored() == PowerUpType::None);
    s.activate(PowerUpType::ShieldBubble);
    for (int i = 0; i < 60 * 9; ++i) s.update(kDt);
    CHECK(!s.active(PowerUpType::SpeedShoes)); // timed out
    CHECK(s.active(PowerUpType::ShieldBubble)); // lasts until consumed
    s.consume(PowerUpType::ShieldBubble);
    CHECK(!s.active(PowerUpType::ShieldBubble));
    s.activate(PowerUpType::TinyMode);
    s.activate(PowerUpType::GiantMode);
    CHECK(!s.active(PowerUpType::TinyMode) && s.active(PowerUpType::GiantMode));
}

void testWorldsAndLevel() {
    CHECK(worldDef(WorldId::SunnyMeadows).implemented);
    CHECK(worldDef(0).levelCount >= 8);
    CHECK(std::string(worldDef(WorldId::CosmicZone).name) == "COSMIC ZONE");
    CHECK(worldDef(WorldId::FrozenPeaks).rules.frictionScale < 1.0f);

    Level level(4, 3);
    CHECK(level.tileAt(0, 0) == Tile::Ground);
    level.setTile(1, 1, Tile::Wall);
    CHECK(Level::isSolid(level.tileAt(1, 1)));
    CHECK(level.tileAt(-1, 0) == Tile::Wall); // outside counts as wall
    CHECK(level.tileAt(4, 0) == Tile::Wall);
}

// --- Phase 2: levels, collision, camera, coins ---------------------------------

Level parseOrDie(const std::vector<std::string>& rows) {
    Level level;
    std::string error;
    const bool ok = Level::fromAscii(rows, level, &error);
    if (!ok) std::printf("  map error: %s\n", error.c_str());
    CHECK(ok);
    return level;
}

void testAsciiParsing() {
    const Level level = parseOrDie({
        "#####",
        "#Pc.#",
        "#~=E#",
        "#####",
    });
    CHECK(level.width() == 5 && level.height() == 4);
    CHECK(level.tileAt(1, 1) == Tile::Ground); // spawn marker is ground
    CHECK(level.tileAt(2, 1) == Tile::Ground); // coin marker is ground
    CHECK(level.tileAt(1, 2) == Tile::Water);
    CHECK(level.tileAt(2, 2) == Tile::Bridge);
    CHECK(level.tileAt(3, 2) == Tile::Exit);
    CHECK(level.coins.size() == 1);
    CHECK_NEAR(level.spawn.x, 48.0f, 0.01f); // centre of tile 1
    CHECK_NEAR(level.exit.x, 112.0f, 0.01f);

    Level bad;
    std::string error;
    CHECK(!Level::fromAscii({"###", "#P", "###"}, bad, &error));        // ragged
    CHECK(error.find("row 2") != std::string::npos);
    CHECK(!Level::fromAscii({"#P#", "#?E"}, bad, &error));              // unknown char
    CHECK(error.find("'?'") != std::string::npos);
    CHECK(!Level::fromAscii({"#..", "#.E"}, bad, &error));              // no spawn
    CHECK(!Level::fromAscii({"PP.", "#.E"}, bad, &error));              // two spawns
    CHECK(!Level::fromAscii({"P..", "#.."}, bad, &error));              // no exit
}

void testCollisionStopsFlush() {
    const Level level = parseOrDie({
        "######",
        "#P..##",
        "#...E#",
        "######",
    });
    // Box in tile column 2 moving right into the wall at column 4 (x = 128).
    const RectF box{70.0f, 40.0f, 18.0f, 12.0f};
    CollisionResult hit;
    const Vec2 moved = moveAndCollide(level, box, {100.0f, 0.0f}, 0.0f, &hit);
    CHECK(hit.hitX && !hit.hitY);
    CHECK_NEAR(box.x + moved.x + box.w, 128.0f, 0.01f);

    // Moving diagonally into the wall slides along it.
    const Vec2 slid = moveAndCollide(level, box, {100.0f, 20.0f}, 0.0f, &hit);
    CHECK(hit.hitX);
    CHECK_NEAR(slid.y, 20.0f, 0.01f);

    // Outside the map counts as solid.
    const Vec2 up = moveAndCollide(level, RectF{40.0f, 40.0f, 18.0f, 12.0f}, {0.0f, -500.0f}, 0.0f, &hit);
    CHECK(hit.hitY);
    CHECK_NEAR(40.0f + up.y, 32.0f, 0.01f);
}

void testNoTunnelling() {
    // A single wall tile between two open areas; dash at it many times.
    const Level level = parseOrDie({
        "#######",
        "#P.#.E#",
        "#######",
    });
    Player p(level.spawn);
    for (int i = 0; i < 120; ++i) {
        PlayerInput in;
        in.move = {1.0f, 0.0f};
        in.dashPressed = (i % 20) == 0;
        p.update(in, kDt, &level);
        CHECK(p.hitbox().right() <= 96.0f + 0.01f);
    }
    CHECK(p.velocity().x == 0.0f); // pinned against the wall
    // Even a single absurd step cannot pass through.
    const Vec2 moved = moveAndCollide(level, p.hitbox(), {1000.0f, 0.0f});
    CHECK(moved.x <= 0.01f);
}

void testCornerNudge() {
    const Level level = parseOrDie({
        "#######",
        "#P.#..#",
        "#.....#",
        "#..#.E#",
        "#######",
    });
    // Feet 5px too low for the one-tile gap in column 3 (rows 64..96).
    const RectF box{40.0f, 89.0f, 18.0f, 12.0f};
    CollisionResult hit;
    const Vec2 stuck = moveAndCollide(level, box, {80.0f, 0.0f}, 0.0f, &hit);
    CHECK(hit.hitX);
    CHECK(box.x + stuck.x + box.w <= 96.01f);

    Player p({50.0f, 99.0f});
    PlayerInput right;
    right.move = {1.0f, 0.0f};
    for (int i = 0; i < 60; ++i) p.update(right, kDt, &level);
    CHECK(p.position().x > 140.0f); // slipped through the gap
    CHECK(!level.overlapsSolid(p.hitbox()));
}

void testCamera() {
    Camera cam(640.0f, 480.0f);
    cam.setBounds(1280.0f, 768.0f);
    cam.snapTo({10.0f, 10.0f});
    CHECK(cam.rawPosition().x == 0.0f && cam.rawPosition().y == 0.0f); // clamped to the top-left
    cam.snapTo({1270.0f, 760.0f});
    CHECK_NEAR(cam.rawPosition().x, 640.0f, 0.01f);
    CHECK_NEAR(cam.rawPosition().y, 288.0f, 0.01f);

    // Small movements inside the dead-zone don't scroll.
    cam.snapTo({640.0f, 384.0f});
    const Vec2 before = cam.rawPosition();
    for (int i = 0; i < 30; ++i) cam.follow({650.0f, 390.0f}, {}, kDt);
    CHECK_NEAR(cam.rawPosition().x, before.x, 0.01f);
    // Large movements do, and the camera converges.
    for (int i = 0; i < 120; ++i) cam.follow({900.0f, 384.0f}, {}, kDt);
    CHECK(cam.rawPosition().x > before.x + 200.0f);

    // A top margin lets the view scroll above the world (room for the HUD).
    Camera hud(640.0f, 480.0f);
    hud.setBounds(1280.0f, 768.0f, 40.0f);
    hud.snapTo({0.0f, 0.0f});
    CHECK_NEAR(hud.rawPosition().y, -40.0f, 0.01f);

    // A world smaller than the screen is centred.
    Camera small(640.0f, 480.0f);
    small.setBounds(320.0f, 240.0f);
    small.snapTo({0.0f, 0.0f});
    CHECK_NEAR(small.rawPosition().x, -160.0f, 0.01f);

    // Disabled shake leaves the position untouched.
    cam.setShakeEnabled(false);
    cam.shake(10.0f, 1.0f);
    CHECK(cam.position().x == std::round(cam.rawPosition().x));
}

void testBuiltinLevel() {
    Level level;
    std::string error;
    const bool ok = makeTestLevel(level, &error);
    if (!ok) std::printf("  level error: %s\n", error.c_str());
    CHECK(ok);
    CHECK(!level.id.empty() && !level.name.empty());

    Player p(level.spawn);
    CHECK(!level.overlapsSolid(p.hitbox())); // spawn is not inside a wall

    // Tile flood fill from the spawn. With `abilities`, crates, boulders and
    // tiny gaps count as passable (the level's power-ups can open them).
    auto reachable = [&](bool abilities) {
        const int sx = static_cast<int>(level.spawn.x) / 32;
        const int sy = static_cast<int>(level.spawn.y) / 32;
        std::set<std::pair<int, int>> seen{{sx, sy}};
        std::vector<std::pair<int, int>> stack{{sx, sy}};
        while (!stack.empty()) {
            const auto [x, y] = stack.back();
            stack.pop_back();
            const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : dirs) {
                const int nx = x + d[0];
                const int ny = y + d[1];
                if (!level.inBounds(nx, ny)) continue;
                const Tile t = level.tileAt(nx, ny);
                const bool open = !Level::isSolid(t, abilities ? kPassTinyGaps : kPassNone) ||
                                  (abilities && Level::isBreakable(t, true));
                if (open && !Level::isDanger(t) && seen.insert({nx, ny}).second) stack.push_back({nx, ny});
            }
        }
        return seen;
    };
    auto tileOf = [](Vec2 p) { return std::make_pair(static_cast<int>(p.x) / 32, static_cast<int>(p.y) / 32); };

    // Coins and the exit need no special abilities.
    const auto plain = reachable(false);
    int unreachable = 0;
    for (const Vec2& c : level.coins)
        if (!plain.count(tileOf(c))) ++unreachable;
    CHECK(unreachable == 0);
    CHECK(plain.count(tileOf(level.exit)) == 1);

    // Everything else is reachable once crates, boulders and tiny gaps open...
    const auto full = reachable(true);
    std::vector<Vec2> extras = level.stars;
    extras.insert(extras.end(), level.gems.begin(), level.gems.end());
    extras.insert(extras.end(), level.hearts.begin(), level.hearts.end());
    for (const PowerUpSpawn& pu : level.powerUps) extras.push_back(pu.pos);
    for (const CheckpointSpawn& cp : level.checkpoints) extras.push_back(cp.pos);
    unreachable = 0;
    for (const Vec2& e : extras)
        if (!full.count(tileOf(e))) ++unreachable;
    CHECK(unreachable == 0);
    CHECK(level.stars.size() == 3);

    // ...and the abilities needed to open them must be in the level, reachable
    // without themselves needing those abilities.
    bool hasBoulder = false;
    bool hasTinyGap = false;
    for (int y = 0; y < level.height(); ++y)
        for (int x = 0; x < level.width(); ++x) {
            hasBoulder |= level.tileAt(x, y) == Tile::Boulder;
            hasTinyGap |= level.tileAt(x, y) == Tile::TinyGap;
        }
    auto hasReachablePower = [&](PowerUpType t) {
        for (const PowerUpSpawn& pu : level.powerUps)
            if (pu.type == t && plain.count(tileOf(pu.pos))) return true;
        return false;
    };
    if (hasBoulder) CHECK(hasReachablePower(PowerUpType::GiantMode));
    if (hasTinyGap) CHECK(hasReachablePower(PowerUpType::TinyMode));
}

void testCoins() {
    CoinField coins;
    coins.reset({{100.0f, 100.0f}, {130.0f, 100.0f}, {400.0f, 400.0f}});
    Effects fx;
    CHECK(coins.total() == 3);
    CHECK(coins.collect({100.0f, 105.0f}, &fx) == 1);
    CHECK(coins.collect({100.0f, 105.0f}, &fx) == 0); // can't collect twice
    CHECK(coins.collect({120.0f, 100.0f}, &fx) == 1);
    CHECK(coins.collected() == 2);
    CHECK(fx.activeCount() == 2);
    coins.reset({{0.0f, 0.0f}});
    CHECK(coins.collected() == 0 && coins.total() == 1);
}

void testEffectsPool() {
    Effects fx;
    for (int i = 0; i < 200; ++i) fx.spawn(Effects::Type::Dust, {0.0f, 0.0f}, SDL_Color{255, 255, 255, 255});
    CHECK(fx.activeCount() == 96); // fixed pool, recycles the oldest
    // Every effect type finishes within its lifetime.
    for (int i = 0; i < 6; ++i) fx.spawn(static_cast<Effects::Type>(i), {0.0f, 0.0f}, SDL_Color{255, 255, 255, 255});
    float longest = 0.0f;
    for (int i = 0; i < 6; ++i) longest = std::max(longest, Effects::lifetime(static_cast<Effects::Type>(i)));
    CHECK(longest < 1.5f);
    for (int i = 0; i < static_cast<int>(longest / kDt) + 2; ++i) fx.update(kDt);
    CHECK(fx.activeCount() == 0);
}

// --- Phase 3: enemies, damage, hazards, checkpoints, difficulty ---------------

// Runs a session for `steps` steps with the given held input; `hopAt`/`dashAt`
// press A/B on one step. Returns all events OR-ed together.
unsigned run(LevelSession& s, Vec2 move, int steps, int hopAt = -1, int dashAt = -1) {
    unsigned all = 0;
    for (int i = 0; i < steps; ++i) {
        PlayerInput in;
        in.move = move;
        in.hopPressed = i == hopAt;
        in.dashPressed = i == dashAt;
        all |= s.update(in, kDt);
    }
    return all;
}

// Holds RIGHT until the player's feet reach `x`; returns steps taken.
int walkRightTo(LevelSession& s, float x) {
    int steps = 0;
    while (s.player().position().x < x && steps < 600) {
        PlayerInput in;
        in.move = {1.0f, 0.0f};
        s.update(in, kDt);
        ++steps;
    }
    return steps;
}

void testThornsHurtAndKnockBack() {
    const Level level = parseOrDie({
        "#########",
        "#P..^..E#",
        "#########",
    });
    LevelSession s(level, Difficulty::Normal);
    const unsigned ev = run(s, {1.0f, 0.0f}, 40);
    CHECK(ev & kSessionHurt);
    CHECK(s.hearts() == 2);
    CHECK(s.player().isInvincible());
    CHECK(s.player().position().x < 128.0f); // pushed back out of the thorns
    CHECK(s.stats().heartsLost == 1);
}

void testHopOverThorns() {
    const Level level = parseOrDie({
        "#########",
        "#P..^..E#",
        "#########",
    });
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 116.0f); // thorns span x 128..160
    run(s, {1.0f, 0.0f}, 40, 0);
    CHECK(s.hearts() == 3);
    CHECK(s.player().position().x > 170.0f);
}

void testWaterFallAndDashSkim() {
    const Level level = parseOrDie({
        "#########",
        "#P..~..E#",
        "#########",
    });
    // Walking in: splash, lose a heart, rescued to the bank.
    LevelSession s(level, Difficulty::Normal);
    const unsigned ev = run(s, {1.0f, 0.0f}, 60);
    CHECK(ev & kSessionSplash);
    CHECK(s.hearts() == 2);
    CHECK(s.player().position().x < 128.0f);
    CHECK(s.stats().splashes >= 1);

    // Dashing across a one-tile stream skims over it.
    LevelSession d(level, Difficulty::Normal);
    walkRightTo(d, 112.0f);
    const unsigned dev = run(d, {1.0f, 0.0f}, 30, -1, 0);
    CHECK((dev & kSessionSplash) == 0);
    CHECK(d.hearts() == 3);
    CHECK(d.player().position().x > 160.0f);
}

void testStompAndDashDefeatEnemies() {
    const Level level = parseOrDie({
        "##########",
        "#P...m..E#",
        "##########",
    });
    // Touching the mushroom on foot hurts.
    LevelSession walk(level, Difficulty::Normal);
    const unsigned wev = run(walk, {1.0f, 0.0f}, 50);
    CHECK(wev & kSessionHurt);
    CHECK(walk.enemiesAlive() == 1);

    // Hopping onto it defeats it and bounces the hero.
    LevelSession hop(level, Difficulty::Normal);
    walkRightTo(hop, 128.0f); // mushroom centre x = 176
    const unsigned hev = run(hop, {1.0f, 0.0f}, 25, 0);
    CHECK(hev & kSessionStomp);
    CHECK(hop.enemiesAlive() == 0);
    CHECK(hop.hearts() == 3);
    CHECK(hop.stats().enemiesDefeated == 1);

    // Dashing into it also defeats it, without damage.
    LevelSession dash(level, Difficulty::Normal);
    walkRightTo(dash, 120.0f);
    const unsigned dev = run(dash, {1.0f, 0.0f}, 20, -1, 0);
    CHECK(dev & kSessionStomp);
    CHECK((dev & kSessionHurt) == 0);
    CHECK(dash.enemiesAlive() == 0);
}

void testKnockOutReturnsToCheckpoint() {
    const Level level = parseOrDie({
        "############",
        "#P.C..~~~.E#",
        "############",
    });
    LevelSession s(level, Difficulty::Normal);
    unsigned ev = 0;
    int steps = 0;
    // Keep walking into the water: each fall (outside invincibility) costs a heart.
    while (!(ev & kSessionKnockedOut) && steps < 60 * 15) {
        PlayerInput in;
        in.move = {1.0f, 0.0f};
        ev |= s.update(in, kDt);
        ++steps;
    }
    CHECK(ev & kSessionCheckpoint);
    CHECK(ev & kSessionKnockedOut);
    CHECK(s.state() == LevelSession::State::KnockedOut);
    CHECK(s.activeCheckpoint() == 0);

    // No game over: after the "oops" pause we are back at the checkpoint, healed.
    const unsigned rev = run(s, {}, static_cast<int>(LevelSession::kKnockOutTime * 60.0f) + 2);
    CHECK(rev & kSessionRespawned);
    CHECK(s.state() == LevelSession::State::Playing);
    CHECK(s.hearts() == s.maxHearts());
    CHECK_NEAR(s.player().position().x, s.checkpoints()[0].pos.x, 0.01f);
    CHECK(s.player().isInvincible());
}

void testHeartPickup() {
    const Level level = parseOrDie({
        "#########",
        "#P^.h..E#",
        "#########",
    });
    LevelSession s(level, Difficulty::Normal);
    // Step on the thorns first (lose a heart), then walk on through the
    // pickup while still invincible.
    run(s, {1.0f, 0.0f}, 70);
    CHECK(s.stats().heartsLost == 1);
    CHECK(s.hearts() == 3); // healed back by the pickup
    CHECK(s.heartPickups().remaining() == 0);

    // At full health the heart is left for later.
    const Level spare = parseOrDie({
        "#######",
        "#P.h.E#",
        "#######",
    });
    LevelSession f(spare, Difficulty::Normal);
    run(f, {1.0f, 0.0f}, 30);
    CHECK(f.heartPickups().remaining() == 1);
}

void testDifficultyRules() {
    const Level level = parseOrDie({
        "#########",
        "#PCkr..E#",
        "#########",
    });
    LevelSession relaxed(level, Difficulty::Relaxed);
    LevelSession normal(level, Difficulty::Normal);
    LevelSession challenge(level, Difficulty::Challenge);
    CHECK(relaxed.maxHearts() == 5 && relaxed.hearts() == 5);
    CHECK(normal.maxHearts() == 3);
    CHECK(challenge.maxHearts() == 3);
    CHECK(relaxed.checkpoints().size() == 3);
    CHECK(normal.checkpoints().size() == 2);
    CHECK(challenge.checkpoints().size() == 1);
    CHECK(relaxed.rules().enemySpeed < 1.0f && challenge.rules().enemySpeed > 1.0f);
    CHECK(challenge.rules().scoreMultiplier > normal.rules().scoreMultiplier);
    CHECK(difficultyFromName("relaxed") == Difficulty::Relaxed);
    CHECK(difficultyFromName("Challenge") == Difficulty::Challenge);
    CHECK(difficultyFromName("???") == Difficulty::Normal);
}

void testSlimeTelegraphsThenChases() {
    const Level level = parseOrDie({
        "############",
        "#P.........#",
        "#..........#",
        "#.......s.E#",
        "############",
    });
    EnemySpawn spawn = level.enemies.at(0);
    Enemy slime(spawn);
    const Vec2 target{60.0f, 60.0f};
    const Vec2 start = slime.position();
    float firstMove = -1.0f;
    for (int i = 0; i < 180; ++i) {
        slime.update(kDt, level, target);
        if (firstMove < 0.0f && (slime.position() - start).lengthSq() > 0.01f) firstMove = i * kDt;
    }
    CHECK(firstMove >= 0.3f); // it rests and squishes before the first hop
    CHECK((slime.position() - target).length() < (start - target).length() - 20.0f);
}

void testEnemiesAvoidDanger() {
    const Level level = parseOrDie({
        "###########",
        "#P.~~~^^^.#",
        "#.b.....B.#",
        "#.~s.^...m#",
        "#..~~..^.E#",
        "###########",
    });
    std::vector<Enemy> enemies;
    for (const EnemySpawn& sp : level.enemies) enemies.emplace_back(sp);
    CHECK(enemies.size() == 4);
    bool touched = false;
    for (int i = 0; i < 60 * 20; ++i) {
        for (Enemy& e : enemies) {
            e.update(kDt, level, {300.0f, 300.0f});
            const RectF box = e.hitbox();
            if (level.overlapsSolid(box) || level.overlapsTile(box, Tile::Water) ||
                level.overlapsTile(box, Tile::Hazard) || level.overlapsTile(box, Tile::Exit))
                touched = true;
        }
    }
    CHECK(!touched);
}

void testMushroomShockwave() {
    EnemySpawn spawn;
    spawn.type = EnemyType::Mushroom;
    spawn.pos = {200.0f, 200.0f};
    Enemy m(spawn);
    const Level open = parseOrDie({"P.E"});
    bool sawRing = false;
    bool hitAtRadius = false;
    for (int i = 0; i < 60 * 5 && !hitAtRadius; ++i) {
        m.update(kDt, open, {});
        if (m.ringActive()) {
            sawRing = true;
            const float rr = m.ringRadius();
            hitAtRadius = m.ringHits({200.0f + rr, 200.0f});
            CHECK(!m.ringHits({200.0f, 200.0f}) || rr < Enemy::kRingThickness);
        }
    }
    CHECK(sawRing);
    CHECK(hitAtRadius);
    m.defeat();
    CHECK(!m.alive() && !m.ringActive());
}

void testDebugWarp() {
    const Level level = parseOrDie({
        "##########",
        "#P.......#",
        "#........#",
        "#....m...#",
        "#...#E#..#",
        "#........#",
        "##########",
    });
    LevelSession s(level, Difficulty::Normal);
    // Warping to a free tile lands exactly there, not next to it.
    CHECK(s.warpNear(Level::tileCenter(5, 1)));
    CHECK(static_cast<int>(s.player().position().x) / 32 == 5);
    CHECK(static_cast<int>(s.player().position().y) / 32 == 1);
    CHECK(!s.player().isInvincible()); // debug warps don't blink the hero
    // Warping to the exit picks a free tile nearby (never the exit itself).
    CHECK(s.warpNear(level.exit));
    CHECK(!level.overlapsTile(s.player().hitbox(), Tile::Exit));
    CHECK(!level.overlapsSolid(s.player().hitbox()));
    CHECK(s.state() == LevelSession::State::Playing);
}

// --- Phase 4: stars, gems, power-ups, smashing --------------------------------

// One step with explicit buttons.
unsigned stepWith(LevelSession& s, Vec2 move, bool hop = false, bool dash = false, bool use = false) {
    PlayerInput in;
    in.move = move;
    in.hopPressed = hop;
    in.dashPressed = dash;
    in.usePressed = use;
    return s.update(in, kDt);
}

void testStarsGemsAndGoldenStar() {
    const Level all = parseOrDie({
        "##########",
        "#P*c*g*.E#",
        "##########",
    });
    LevelSession s(all, Difficulty::Normal);
    const unsigned ev = run(s, {1.0f, 0.0f}, 120);
    CHECK(ev & kSessionStar);
    CHECK(ev & kSessionGem);
    CHECK(s.items().starsCollected() == 3 && s.items().starsTotal() == 3);
    CHECK(s.items().starCollected(0) && s.items().starCollected(2));
    CHECK(s.items().gemsCollected() == 1);
    CHECK(s.state() == LevelSession::State::Cleared);
    CHECK(ev & kSessionGoldenStar);
    CHECK(s.goldenStar());

    // Missing one star (in the row below) means no Golden Star.
    const Level missed = parseOrDie({
        "#########",
        "#P*c*.E.#",
        "#.....*.#",
        "#########",
    });
    LevelSession m(missed, Difficulty::Normal);
    const unsigned mev = run(m, {1.0f, 0.0f}, 120);
    CHECK(m.state() == LevelSession::State::Cleared);
    CHECK((mev & kSessionGoldenStar) == 0);
    CHECK(!m.goldenStar());
    CHECK(m.items().starsCollected() == 2);
}

void testPowerUpSlot() {
    const Level level = parseOrDie({
        "##########",
        "#P12....E#",
        "##########",
    });
    LevelSession s(level, Difficulty::Normal);
    // Walk over both bubbles and out of reach of the second (x > 112 + 18).
    const unsigned ev = run(s, {1.0f, 0.0f}, 40);
    CHECK(ev & kSessionPowerUpGet);
    CHECK(s.powers().stored() == PowerUpType::SpeedShoes); // the shield waits on the ground
    CHECK(s.player().position().x > 132.0f);
    run(s, {}, 10);

    const unsigned use = stepWith(s, {}, false, false, true);
    CHECK(use & kSessionPowerUpUse);
    CHECK(s.powers().active(PowerUpType::SpeedShoes));
    CHECK(s.powers().stored() == PowerUpType::None);
    CHECK(s.player().modifiers().speedScale > 1.0f);

    // Slot is free again: walking back picks up the shield.
    run(s, {-1.0f, 0.0f}, 20);
    CHECK(s.powers().stored() == PowerUpType::ShieldBubble);

    // Timed power-ups run out (with an event).
    const unsigned end = run(s, {}, 60 * 9);
    CHECK(end & kSessionPowerUpEnd);
    CHECK(!s.powers().active(PowerUpType::SpeedShoes));
    CHECK(s.player().modifiers().speedScale == 1.0f);
}

void testSpeedAndSuperDashModifiers() {
    Player normal({0.0f, 0.0f});
    Player fast({0.0f, 0.0f});
    PlayerModifiers speed;
    speed.speedScale = 1.5f;
    fast.setModifiers(speed);
    PlayerInput right;
    right.move = {1.0f, 0.0f};
    simulate(normal, right, 60);
    simulate(fast, right, 60);
    CHECK(fast.position().x > normal.position().x * 1.4f);

    Player dash1({0.0f, 0.0f});
    Player dash2({0.0f, 0.0f});
    PlayerModifiers super;
    super.dashTimeScale = 2.0f;
    dash2.setModifiers(super);
    PlayerInput d;
    d.dashPressed = true;
    simulate(dash1, d, 1);
    simulate(dash2, d, 1);
    simulate(dash1, PlayerInput{}, 30);
    simulate(dash2, PlayerInput{}, 30);
    CHECK(dash2.position().y > dash1.position().y * 1.7f); // idle dash goes down

    // Size scales the hitbox around the feet.
    Player giant({100.0f, 100.0f});
    PlayerModifiers big;
    big.size = 1.45f;
    const RectF a = giant.hitbox();
    const RectF b = giant.hitboxWith(big);
    CHECK(b.w > a.w * 1.4f);
    CHECK_NEAR(b.center().x, a.center().x, 0.01f);
}

void testShieldBlocksOneHit() {
    const Level level = parseOrDie({
        "##########",
        "#P2..^..E#",
        "##########",
    });
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 80.0f);
    CHECK(s.powers().stored() == PowerUpType::ShieldBubble);
    stepWith(s, {}, false, false, true);
    CHECK(s.powers().active(PowerUpType::ShieldBubble));
    const unsigned ev = run(s, {1.0f, 0.0f}, 40); // into the thorns
    CHECK(ev & kSessionShieldPop);
    CHECK((ev & kSessionHurt) == 0);
    CHECK(s.hearts() == 3);
    CHECK(!s.powers().active(PowerUpType::ShieldBubble));
}

void testMagnetAndDoubleCoins() {
    const Level level = parseOrDie({
        "############",
        "#P3........#",
        "#..........#",
        "#.......c..#",
        "#.........E#",
        "############",
    });
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 80.0f);
    stepWith(s, {}, false, false, true);
    CHECK(s.powers().active(PowerUpType::Magnet));
    walkRightTo(s, 160.0f); // still ~100px from the coin
    run(s, {}, 60);
    CHECK(s.coins().collected() == 1);
    CHECK(s.player().position().x < 200.0f);

    const Level dbl = parseOrDie({
        "###########",
        "#P5.ccc..E#",
        "###########",
    });
    LevelSession d(dbl, Difficulty::Normal);
    walkRightTo(d, 80.0f);
    stepWith(d, {}, false, false, true);
    CHECK(d.powers().active(PowerUpType::DoubleCoins));
    walkRightTo(d, 220.0f);
    CHECK(d.coins().collected() == 3);
    CHECK(d.stats().coinPoints == 6);
}

void testTinyModeGaps() {
    const Level level = parseOrDie({
        "##########",
        "#P67:...E#",
        "##########",
    });
    // Normal size can't fit through the gap (tile x 128..160).
    LevelSession blocked(level, Difficulty::Normal);
    run(blocked, {1.0f, 0.0f}, 120);
    CHECK(blocked.player().position().x < 128.0f);

    // Tiny Mode slips in. Carry the Giant bubble along in the slot.
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 80.0f);
    stepWith(s, {}, false, false, true);
    CHECK(s.powers().active(PowerUpType::TinyMode));
    CHECK(s.player().modifiers().size < 1.0f);
    walkRightTo(s, 140.0f); // picks up the giant bubble on the way
    run(s, {}, 10);
    CHECK(s.powers().stored() == PowerUpType::GiantMode);
    CHECK(s.player().position().x > 132.0f && s.player().position().x < 156.0f);

    // Giant can't grow inside the gap: no room, it stays in the slot.
    const unsigned denied = stepWith(s, {}, false, false, true);
    CHECK(denied & kSessionNoRoom);
    CHECK(s.powers().stored() == PowerUpType::GiantMode);
    CHECK(!s.powers().active(PowerUpType::GiantMode));

    // Tiny Mode never runs out while the hero is still inside the gap...
    run(s, {}, 60 * 11);
    CHECK(s.powers().active(PowerUpType::TinyMode));
    CHECK(!s.level().overlapsSolid(s.player().hitbox(), kPassTinyGaps));

    // ...but ends once they walk out, leaving them at normal size and free.
    walkRightTo(s, 210.0f);
    run(s, {}, 30);
    CHECK(!s.powers().active(PowerUpType::TinyMode));
    CHECK(!s.level().overlapsSolid(s.player().hitbox()));

    // With room to spare, the stored Giant now works.
    const unsigned grow = stepWith(s, {}, false, false, true);
    CHECK(grow & kSessionPowerUpUse);
    CHECK(s.powers().active(PowerUpType::GiantMode));
    CHECK(!s.level().overlapsSolid(s.player().hitbox()));
}

void testGiantSmashesAndCrushes() {
    const Level level = parseOrDie({
        "###########",
        "#P7..X.m.E#",
        "###########",
    });
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 80.0f);
    const unsigned use = stepWith(s, {}, false, false, true);
    CHECK(use & kSessionPowerUpUse);
    CHECK(s.powers().active(PowerUpType::GiantMode));
    const unsigned ev = run(s, {1.0f, 0.0f}, 150);
    CHECK(ev & kSessionBlockBroken);
    CHECK(s.level().tileAt(5, 1) == Tile::Ground);
    CHECK(s.enemiesAlive() == 0);
    CHECK(s.hearts() == 3);
    CHECK(s.state() == LevelSession::State::Cleared);

    // Restart puts the boulder back.
    s.restart();
    CHECK(s.level().tileAt(5, 1) == Tile::Boulder);
}

void testDashSmashesCrates() {
    const Level level = parseOrDie({
        "#########",
        "#P..x..E#",
        "#########",
    });
    // Walking just bumps into the crate.
    LevelSession walk(level, Difficulty::Normal);
    run(walk, {1.0f, 0.0f}, 90);
    CHECK(walk.level().tileAt(4, 1) == Tile::Crate);
    CHECK(walk.player().position().x < 128.0f);

    // A dash smashes it.
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 90.0f);
    const unsigned ev = run(s, {1.0f, 0.0f}, 10, -1, 0);
    CHECK(ev & kSessionBlockBroken);
    CHECK(s.level().tileAt(4, 1) == Tile::Ground);
    CHECK(s.stats().blocksBroken == 1);
    run(s, {1.0f, 0.0f}, 90);
    CHECK(s.state() == LevelSession::State::Cleared);
}

void testRainbowStar() {
    const Level level = parseOrDie({
        "###########",
        "#P8.^.m..E#",
        "###########",
    });
    LevelSession s(level, Difficulty::Normal);
    walkRightTo(s, 80.0f);
    stepWith(s, {}, false, false, true);
    CHECK(s.powers().active(PowerUpType::RainbowStar));
    CHECK(s.player().modifiers().rainbow);
    const unsigned ev = run(s, {1.0f, 0.0f}, 150);
    CHECK((ev & kSessionHurt) == 0);
    CHECK(s.hearts() == 3);
    CHECK(s.enemiesAlive() == 0);
    CHECK(s.state() == LevelSession::State::Cleared);
}

// --- Phase 5: level files, objectives, rafts, keys, friends, signs ------------

void testLevelFileParsing() {
    Level level;
    std::string error;
    const char* good =
        "# a comment\n"
        "id=9-9\n"
        "name=TEST\n"
        "objective=coins\n"
        "goal=2\n"
        "time_limit=30\n"
        "[map]\n"
        "#######\n"
        "#PccS.E\n"
        "#######\n"
        "[signs]\n"
        "HELLO THERE\n";
    CHECK(levels::parse(good, level, &error));
    CHECK(level.id == "9-9" && level.name == "TEST");
    CHECK(level.objective == Objective::Coins && level.goal == 2);
    CHECK(level.timeLimit == 30.0f);
    CHECK(level.signs.size() == 1 && level.signs[0].text == "HELLO THERE");

    auto fails = [&](const char* text, const char* expect) {
        Level l;
        std::string e;
        const bool ok = levels::parse(text, l, &e);
        if (ok || e.find(expect) == std::string::npos) std::printf("  got: %s\n", e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    CHECK(fails("id=x\ncolour=red\n[map]\nP.E\n", "line 2: unknown key 'colour'"));
    CHECK(fails("id=x\nobjective=dance\n[map]\nP.E\n", "unknown objective"));
    CHECK(fails("id=x\n[map]\nP.E\nP.?\n", "line 4"));                     // map errors quote file lines
    CHECK(fails("id=x\n[map]\nPSE\n", "1 sign(s) but [signs] lists 0"));
    CHECK(fails("id=x\nobjective=coins\ngoal=5\n[map]\nPcE\n", "goal=1..1"));
    CHECK(fails("id=x\nobjective=rescue\n[map]\nP.E\n", "no friends"));
    CHECK(fails("name=x\n[map]\nP.E\n", "missing id"));
    CHECK(fails("id=x\n", "missing [map]"));

    CHECK(levels::nextLevelId("1-1") == "1-2");
    CHECK(levels::nextLevelId("1-8").empty());
    CHECK(levels::worldLevelIds(1).size() == 8);
}

// Tile flood fill shared by the shipped-level checks: walking, hops/dashes
// over 1-2 danger tiles (1-4 with Super Dash), raft tracks, and the
// crate/boulder/tiny-gap/lock abilities the level provides.
std::set<std::pair<int, int>> reachableTiles(const Level& level) {
    bool tiny = false, giant = false, superDash = false;
    for (const PowerUpSpawn& p : level.powerUps) {
        tiny |= p.type == PowerUpType::TinyMode;
        giant |= p.type == PowerUpType::GiantMode;
        superDash |= p.type == PowerUpType::SuperDash;
    }
    const bool key = !level.keys.empty();
    const int maxGap = superDash ? 4 : 2;

    std::set<std::pair<int, int>> raft;
    for (const RaftSpawn& r : level.rafts) {
        const int rx = static_cast<int>(r.pos.x) / 32;
        const int ry = static_cast<int>(r.pos.y) / 32;
        const int dx = static_cast<int>(r.dir.x);
        const int dy = static_cast<int>(r.dir.y);
        for (int sgn : {1, -1})
            for (int x = rx, y = ry; level.tileAt(x, y) == Tile::Water; x += dx * sgn, y += dy * sgn)
                raft.insert({x, y});
    }
    auto standable = [&](int x, int y) {
        if (!level.inBounds(x, y)) return false;
        if (raft.count({x, y})) return true;
        const Tile t = level.tileAt(x, y);
        if (Level::isDanger(t)) return false;
        if (t == Tile::Crate) return true;
        if (t == Tile::Boulder) return giant;
        if (t == Tile::Lock) return key;
        return !Level::isSolid(t, tiny ? kPassTinyGaps : kPassNone);
    };
    auto danger = [&](int x, int y) {
        return level.inBounds(x, y) && Level::isDanger(level.tileAt(x, y)) && !raft.count({x, y});
    };

    const std::pair<int, int> start{static_cast<int>(level.spawn.x) / 32, static_cast<int>(level.spawn.y) / 32};
    std::set<std::pair<int, int>> seen{start};
    std::vector<std::pair<int, int>> stack{start};
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& d : dirs) {
            for (int n = 0; n <= maxGap; ++n) {
                bool gapOk = true;
                for (int k = 1; k <= n; ++k) gapOk &= danger(x + d[0] * k, y + d[1] * k);
                if (!gapOk) break;
                const int tx = x + d[0] * (n + 1);
                const int ty = y + d[1] * (n + 1);
                if (standable(tx, ty)) {
                    if (seen.insert({tx, ty}).second) stack.push_back({tx, ty});
                    break;
                }
            }
        }
    }
    return seen;
}

void testShippedLevels() {
    for (const std::string& id : levels::worldLevelIds(1)) {
        Level level;
        std::string error;
        const std::string path = std::string(POCKETDASH_SOURCE_DIR) + "/assets/levels/" + id + ".lvl";
        const bool ok = levels::loadFile(path, level, &error);
        if (!ok) std::printf("  %s\n", error.c_str());
        CHECK(ok);
        if (!ok) continue;
        CHECK(level.id == id);
        CHECK(level.stars.size() == 3);
        CHECK(!level.overlapsSolid(Player(level.spawn).hitbox()));

        const auto seen = reachableTiles(level);
        auto tileOf = [](Vec2 p) { return std::make_pair(static_cast<int>(p.x) / 32, static_cast<int>(p.y) / 32); };
        std::vector<Vec2> must = level.coins;
        auto add = [&](const std::vector<Vec2>& v) { must.insert(must.end(), v.begin(), v.end()); };
        add(level.stars);
        add(level.gems);
        add(level.hearts);
        add(level.keys);
        add(level.friends);
        for (const PowerUpSpawn& p : level.powerUps) must.push_back(p.pos);
        for (const CheckpointSpawn& c : level.checkpoints) must.push_back(c.pos);
        for (const SignSpawn& sgn : level.signs) must.push_back(sgn.pos);
        must.push_back(level.exit);
        int unreachable = 0;
        for (const Vec2& p : must) {
            if (!seen.count(tileOf(p))) {
                ++unreachable;
                std::printf("  %s: unreachable at tile (%d,%d)\n", id.c_str(), tileOf(p).first, tileOf(p).second);
            }
        }
        CHECK(unreachable == 0);
        for (const SignSpawn& sgn : level.signs) CHECK(!sgn.text.empty());
    }
}

void testRaftCarriesAcrossWater() {
    const Level level = parseOrDie({
        "#########",
        "#P.R~~.E#",
        "#########",
    });
    LevelSession s(level, Difficulty::Normal);
    // The raft shuttles from the start; walking straight into the water is a splash.
    walkRightTo(s, 86.0f); // wait on the bank (tile 2 ends at x = 96)
    int guard = 0;
    while (!(s.rafts()[0].pos.x < 116.0f && s.rafts()[0].dir.x > 0.0f) && guard++ < 600) stepWith(s, {});
    CHECK(guard < 600); // it came back to our bank
    // Hop on and ride.
    walkRightTo(s, 104.0f);
    unsigned ev = 0;
    for (int i = 0; i < 70; ++i) ev |= stepWith(s, {});
    CHECK((ev & kSessionSplash) == 0); // riding, not swimming
    CHECK(s.player().position().x > 150.0f); // carried towards the far bank
    CHECK(s.hearts() == 3);
    run(s, {1.0f, 0.0f}, 90);
    CHECK(s.state() == LevelSession::State::Cleared);
}

void testKeysOpenGates() {
    const Level level = parseOrDie({
        "##########",
        "#PK..L.E.#",
        "##########",
    });
    // Without the key the gate stays shut (and says so).
    const Level noKey = parseOrDie({
        "##########",
        "#P...L.E.#",
        "##########",
    });
    LevelSession shut(noKey, Difficulty::Normal);
    const unsigned sev = run(shut, {1.0f, 0.0f}, 90);
    CHECK(sev & kSessionLocked);
    CHECK(shut.level().tileAt(5, 1) == Tile::Lock);

    LevelSession s(level, Difficulty::Normal);
    const unsigned ev = run(s, {1.0f, 0.0f}, 150);
    CHECK(ev & kSessionKey);
    CHECK(ev & kSessionGateOpened);
    CHECK(s.keysHeld() == 0);
    CHECK(s.level().tileAt(5, 1) == Tile::Ground);
    CHECK(s.state() == LevelSession::State::Cleared);
}

void testRescueObjectiveLocksExit() {
    Level level;
    std::string error;
    CHECK(levels::parse("id=t\nobjective=rescue\n[map]\n#######\n#P.f.E#\n#######\n", level, &error));
    LevelSession s(level, Difficulty::Normal);
    CHECK(!s.objectiveComplete());
    // Walk straight to the flag: it's locked until the friend is rescued.
    unsigned ev = run(s, {1.0f, 0.0f}, 90);
    CHECK(ev & kSessionExitLocked);
    CHECK(s.state() == LevelSession::State::Playing);
    // Go back, rescue with Y, return.
    int guard = 0;
    while (s.player().position().x > 112.0f && guard++ < 300) stepWith(s, {-1.0f, 0.0f});
    run(s, {}, 10);
    CHECK(s.interactTarget() == LevelSession::InteractKind::Friend);
    PlayerInput y;
    y.interactPressed = true;
    ev = s.update(y, kDt);
    CHECK(ev & kSessionRescue);
    CHECK(s.friendsRescued() == 1);
    CHECK(s.objectiveComplete());
    run(s, {1.0f, 0.0f}, 90);
    CHECK(s.state() == LevelSession::State::Cleared);
}

void testSignsAndSecrets() {
    Level level;
    std::string error;
    CHECK(levels::parse("id=t\n[map]\n#########\n#PS.%%.E#\n#########\n[signs]\nHI!\n", level, &error));
    CHECK(level.secretCount() == 1); // two adjacent % tiles = one secret
    LevelSession s(level, Difficulty::Normal);
    CHECK(s.interactTarget() == LevelSession::InteractKind::Sign);
    PlayerInput y;
    y.interactPressed = true;
    const unsigned ev = s.update(y, kDt);
    CHECK(ev & kSessionSign);
    CHECK(s.readingSign() == 0);
    const unsigned walk = run(s, {1.0f, 0.0f}, 120);
    CHECK(walk & kSessionSecret);
    CHECK(s.secretsFound() == 1);
    CHECK(s.state() == LevelSession::State::Cleared);
    CHECK(s.goldenStar()); // no stars/coins here, but the secret was found
}

void testTimeLimit() {
    Level level;
    std::string error;
    CHECK(levels::parse("id=t\ntime_limit=2\n[map]\n#####\n#P.E#\n#####\n", level, &error));
    LevelSession s(level, Difficulty::Normal);
    CHECK_NEAR(s.timeLimit(), 2.0f, 0.001f);
    const unsigned ev = run(s, {}, 60 * 2 + 5);
    CHECK(ev & kSessionTimeUp);
    CHECK(s.state() == LevelSession::State::TimeUp);
    s.restart();
    CHECK(s.state() == LevelSession::State::Playing);

    LevelSession relaxed(level, Difficulty::Relaxed);
    CHECK_NEAR(relaxed.timeLimit(), 3.0f, 0.001f); // 50% extra on Relaxed
}

void testCoinObjective() {
    Level level;
    std::string error;
    CHECK(levels::parse("id=t\nobjective=coins\ngoal=2\n[map]\n#########\n#P.E.cc.#\n#########\n", level, &error));
    LevelSession s(level, Difficulty::Normal);
    unsigned ev = run(s, {1.0f, 0.0f}, 30); // over the flag first: locked
    CHECK(ev & kSessionExitLocked);
    run(s, {1.0f, 0.0f}, 60); // collect both coins
    int have = 0, need = 0;
    s.objectiveProgress(have, need);
    CHECK(have == 2 && need == 2);
    run(s, {-1.0f, 0.0f}, 120);
    CHECK(s.state() == LevelSession::State::Cleared);
}

} // namespace


// --- Phase 6: menus, progress, scores ------------------------------------------

void testMenuAutoRepeat() {
    InputManager input;
    input.setInjected(Action::Down, true);
    int fires = 0;
    int firstRepeat = -1;
    for (int step = 0; step < 60; ++step) {
        if (input.repeated(Action::Down)) {
            ++fires;
            if (step > 0 && firstRepeat < 0) firstRepeat = step;
        }
        input.endUpdateStep();
    }
    // The press, then every kRepeatInterval steps after kRepeatDelay.
    CHECK(firstRepeat == InputManager::kRepeatDelay);
    CHECK(fires == 1 + (60 - 1 - InputManager::kRepeatDelay) / InputManager::kRepeatInterval + 1);
    input.setInjected(Action::Down, false);
    input.endUpdateStep();
    CHECK(!input.repeated(Action::Down));
}

void testProgressUnlocksAndRecords() {
    Progress p;
    CHECK(p.isUnlocked("1-1"));
    CHECK(!p.isUnlocked("1-2"));
    CHECK(p.isUnlocked("test")); // not on the map: always playable
    CHECK(p.continueLevel(1) == "1-1");

    RecordUpdate u = p.recordClear("1-1", ClearResult{1000, 50.0f, 0x5u, 1, 0, false});
    CHECK(u.firstClear && !u.newBestScore && !u.newBestTime);
    CHECK(u.newStars && u.newGems && !u.newGolden);
    CHECK(u.highScoreRank == 0);
    CHECK(p.isUnlocked("1-2") && !p.isUnlocked("1-3"));
    CHECK(p.continueLevel(1) == "1-2");

    u = p.recordClear("1-1", ClearResult{900, 40.0f, 0x2u, 1, 1, true});
    CHECK(!u.firstClear && !u.newBestScore && u.newBestTime && u.newStars && !u.newGems && u.newGolden);
    const LevelRecord& rec = p.record("1-1");
    CHECK(rec.bestScore == 1000);
    CHECK_NEAR(rec.bestTime, 40.0f, 1e-4f);
    CHECK(rec.starsMask == 0x7u);
    CHECK(rec.secrets == 1 && rec.goldenStar);
    CHECK(p.totalStars() == 3 && p.totalGems() == 1 && p.levelsCleared() == 1 && p.goldenStars() == 1);
    u = p.recordClear("1-1", ClearResult{1200, 45.0f, 0, 0, 0, false});
    CHECK(u.newBestScore && !u.newBestTime);

    // Everything cleared: "Play" resumes at the last level.
    Progress all;
    for (const auto& id : levels::worldLevelIds(1)) all.recordClear(id, ClearResult{10, 10.0f, 0, 0, 0, false});
    CHECK(all.continueLevel(1) == levels::worldLevelIds(1).back());
}

void testHighScoreTable() {
    Progress p;
    CHECK(p.highScoreRank("1-1", 0) == -1); // zero never ranks
    for (int i = 5; i >= 1; --i) p.addHighScore("1-1", "abc", i * 100);
    const auto& t = p.record("1-1").highScores;
    CHECK(t.size() == 5);
    CHECK(t.front().score == 500 && t.back().score == 100);
    CHECK(t.front().initials == "ABC");
    CHECK(p.highScoreRank("1-1", 50) == -1);
    CHECK(p.highScoreRank("1-1", 600) == 0);
    CHECK(p.highScoreRank("1-1", 300) == 3); // ties go below the existing entry
    p.addHighScore("1-1", "zed", 350);
    CHECK(t.size() == 5 && t[2].score == 350 && t[2].initials == "ZED" && t.back().score == 200);
    CHECK(p.lastInitials() == "ZED");
    CHECK(sanitizeInitials("ab") == "ABA");
    CHECK(sanitizeInitials("z9q!x") == "ZQX");
    CHECK(sanitizeInitials("") == "AAA");
}

void testOutfits() {
    Progress p;
    CHECK(p.outfitUnlocked(0));
    CHECK(!p.outfitUnlocked(1));
    p.selectOutfit(1); // locked: ignored
    CHECK(p.selectedOutfit() == 0);
    p.recordClear("1-1", ClearResult{10, 10.0f, 0, outfit(1).gemsNeeded, 0, false});
    CHECK(p.outfitUnlocked(1));
    p.selectOutfit(1);
    CHECK(p.selectedOutfit() == 1);
    CHECK(!p.outfitUnlocked(kOutfitCount)); // out of range
    for (int i = 1; i < kOutfitCount; ++i) CHECK(outfit(i).gemsNeeded >= outfit(i - 1).gemsNeeded);

    // World 1 must hold enough gems to unlock every outfit.
    int gems = 0;
    for (const auto& id : levels::worldLevelIds(1)) {
        Level level;
        std::string error;
        const std::string path = std::string(POCKETDASH_SOURCE_DIR) + "/assets/levels/" + id + ".lvl";
        CHECK(levels::loadFile(path, level, &error));
        gems += static_cast<int>(level.gems.size());
    }
    CHECK(gems >= outfit(kOutfitCount - 1).gemsNeeded);
}

void testProgressRoundTrip() {
    Progress p;
    p.recordClear("1-1", ClearResult{1234, 61.25f, 0x3u, 1, 1, true});
    p.recordClear("1-2", ClearResult{777, 90.0f, 0x1u, 0, 0, false});
    p.addHighScore("1-1", "AMY", 1234);
    p.addHighScore("1-1", "BOB", 999);
    p.selectOutfit(1);

    KeyValueStore out;
    p.save(out);
    KeyValueStore in;
    in.parse(out.serialize());
    Progress q;
    q.load(in);
    const LevelRecord& a = q.record("1-1");
    CHECK(a.cleared && a.bestScore == 1234 && a.starsMask == 0x3u && a.gems == 1 && a.secrets == 1 && a.goldenStar);
    CHECK_NEAR(a.bestTime, 61.25f, 0.002f);
    CHECK(a.highScores.size() == 2 && a.highScores[0].initials == "AMY" && a.highScores[1].score == 999);
    CHECK(q.record("1-2").cleared && q.record("1-2").bestScore == 777);
    CHECK(!q.record("1-3").cleared);
    CHECK(q.selectedOutfit() == 1);
    CHECK(q.lastInitials() == "BOB");

    // Through the save folder (progress.ini), as the game does it.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pocketdash_progress_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    SaveManager save(dir.string() + "/");
    CHECK(save.loadProgress().levelsCleared() == 0); // no file yet: fresh progress
    CHECK(save.saveProgress(p));
    CHECK(std::filesystem::exists(dir / "progress.ini"));
    const Progress f = save.loadProgress();
    CHECK(f.record("1-1").bestScore == 1234 && f.levelsCleared() == 2 && f.selectedOutfit() == 1);
    std::filesystem::remove_all(dir);

    // Damaged files load what they can.
    KeyValueStore bad;
    bad.parse("level.1-1.cleared=1\nlevel.1-1.best_score=lots\nlevel.1-1.hs1=ZZ\nlevel.1-1.hs2=QQQ -5\noutfit=99\n");
    Progress r;
    r.load(bad);
    CHECK(r.record("1-1").cleared && r.record("1-1").bestScore == 0 && r.record("1-1").highScores.empty());
    CHECK(r.selectedOutfit() == 0);
}

void testScoreBreakdown() {
    Level level;
    std::string error;
    CHECK(makeTestLevel(level, &error));
    LevelSession normal(level, Difficulty::Normal);
    const ScoreBreakdown s = computeScore(normal);
    CHECK(s.coins == 0 && s.foes == 0 && s.stars == 0 && s.golden == 0);
    CHECK(s.hearts == normal.maxHearts() * ScoreBreakdown::kHeart);
    const float par = level.timeLimit > 0.0f ? level.timeLimit : ScoreBreakdown::kDefaultPar;
    CHECK(s.time == static_cast<int>(par) * ScoreBreakdown::kPerSecond);
    CHECK(s.total() == s.subtotal());

    LevelSession hard(level, Difficulty::Challenge);
    const ScoreBreakdown h = computeScore(hard);
    CHECK(h.multiplier > 1.0f);
    CHECK(h.total() == static_cast<int>(std::lround(h.subtotal() * h.multiplier)));
}

// --- Phase 7: the Meadow Guardian --------------------------------------------------

const std::vector<std::string> kArenaMap = {
    "################",
    "#E.............#",
    "#.TTTTTTTTTTT..#",
    "#.T.........T..#",
    "#.T.........T..#",
    "#.T....G....T..#",
    "#.T.........T..#",
    "#.T.........T..#",
    "#.TTTTT|TTTTT..#",
    "#......P.......#",
    "################",
};

// Steps a boss alone until it reaches `state` (false if it never does).
bool runBossUntil(Boss& b, Boss::State state, Vec2 player, int maxSteps = 3000) {
    for (int i = 0; i < maxSteps; ++i) {
        if (b.state() == state) return true;
        b.update(kDt, player);
    }
    return b.state() == state;
}

void testBossMapParsing() {
    Level level = parseOrDie(kArenaMap);
    CHECK(level.hasBoss);
    CHECK_NEAR(level.bossArena.x, 96.0f, 0.01f);
    CHECK_NEAR(level.bossArena.y, 96.0f, 0.01f);
    CHECK_NEAR(level.bossArena.w, 288.0f, 0.01f);
    CHECK_NEAR(level.bossArena.h, 160.0f, 0.01f);
    CHECK(level.bossGates.size() == 1 && level.bossGates[0].first == 7 && level.bossGates[0].second == 8);
    CHECK(level.tileAt(7, 8) == Tile::Ground); // gates start open

    std::string error;
    Level bad;
    CHECK(!Level::fromAscii({"#######", "#P.G.E#", "#######"}, bad, &error)); // arena not closed off
    CHECK(error.find("arena") != std::string::npos);
    CHECK(!Level::fromAscii({"#######", "#P.|.E#", "#######"}, bad, &error)); // gate without a boss
    CHECK(!Level::fromAscii({"#########", "#P#G#G#E#", "#########"}, bad, &error)); // two bosses
    Objective o;
    CHECK(objectiveFromName("boss", o) && o == Objective::Boss);
    CHECK(std::string(objectiveName(Objective::Boss)) == "boss");
}

void testBossStateMachine() {
    Boss b;
    const RectF arena{0.0f, 0.0f, 320.0f, 320.0f};
    b.reset({160.0f, 160.0f}, arena);
    const Vec2 player{80.0f, 240.0f};
    CHECK(b.sleeping() && b.health() == Boss::kMaxHealth && b.phase() == 1);
    CHECK(!b.hit(player)); // asleep: no hits
    b.update(1.0f, player);
    CHECK(b.sleeping()); // sleeps until woken

    b.wake(true);
    CHECK(b.state() == Boss::State::Intro);
    CHECK(runBossUntil(b, Boss::State::Idle, player));
    CHECK(b.hurtsOnContact());
    CHECK(!b.hit(player)); // not dizzy yet

    // Telegraph, then a leap at where the player stood when it took off.
    CHECK(runBossUntil(b, Boss::State::Telegraph, player));
    CHECK(runBossUntil(b, Boss::State::Leap, player));
    CHECK((b.leapTarget() - player).length() < 1.0f);
    bool rose = false;
    while (b.state() == Boss::State::Leap) {
        b.update(kDt, player);
        if (b.height() > Boss::kLeapHeight * 0.9f) rose = true;
    }
    CHECK(rose);
    CHECK(b.state() == Boss::State::Landed && b.height() == 0.0f);
    int rings = 0;
    for (const auto& r : b.rings()) rings += r.active ? 1 : 0;
    CHECK(rings == Boss::rules(1).ringsPerLanding);
    const float r0 = b.rings()[0].radius;
    b.update(0.1f, player);
    CHECK(b.rings()[0].radius > r0); // the ring spreads

    // Dizzy: now it can be hit.
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.vulnerable() && !b.hurtsOnContact());
    CHECK(b.hit(player));
    CHECK(b.health() == Boss::kMaxHealth - 1 && b.state() == Boss::State::Hurt);
    for (const auto& r : b.rings()) CHECK(!r.active); // no cheap hit right after
    CHECK(!b.hit(player)); // one hit per daze

    // Second hit -> phase 2: two rings per landing, the second delayed.
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.hit(player));
    CHECK(b.phase() == 2);
    CHECK(runBossUntil(b, Boss::State::Landed, player));
    int active = 0, delayed = 0;
    for (const auto& r : b.rings()) {
        active += r.active ? 1 : 0;
        delayed += r.active && r.delay > 0.0f ? 1 : 0;
    }
    CHECK(active == 2 && delayed == 1);

    // Phase 3 leaps twice before getting dizzy.
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.hit(player));
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.hit(player));
    CHECK(b.phase() == 3);
    const int leapsBefore = b.leapsDone();
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.leapsDone() - leapsBefore == 2);

    // Leaps never leave the arena.
    CHECK(b.position().x >= arena.x && b.position().x <= arena.right());

    // Last two hits: it calms down for good.
    CHECK(b.hit(player));
    CHECK(runBossUntil(b, Boss::State::Dazed, player));
    CHECK(b.hit(player));
    CHECK(b.health() == 0);
    CHECK(runBossUntil(b, Boss::State::Defeated, player));
    CHECK(b.defeated() && !b.fighting() && !b.hurtsOnContact());
    b.sleep();
    CHECK(b.defeated()); // stays calm
}

void testBossArenaGatesAndRings() {
    const Level level = parseOrDie(kArenaMap);
    // Walking in wakes it and shuts the gate behind you.
    LevelSession s(level, Difficulty::Normal);
    CHECK(s.boss().sleeping());
    unsigned ev = 0;
    for (int i = 0; i < 120 && s.boss().sleeping(); ++i) ev |= run(s, {0.0f, -1.0f}, 1);
    CHECK(ev & kSessionBossIntro);
    CHECK(Level::isSolid(s.level().tileAt(7, 8)));
    CHECK(!s.objectiveComplete());

    // Standing still under the leap: the landing hurts.
    LevelSession stay = s;
    ev = 0;
    for (int i = 0; i < 600 && stay.boss().state() != Boss::State::Dazed; ++i) ev |= run(stay, {}, 1);
    CHECK(ev & kSessionHurt);

    // Stepping away from the shadow, then standing on the ground: the
    // shockwave ring hurts...
    auto dodge = [&](bool hop) {
        LevelSession t = s;
        unsigned all = 0;
        while (t.boss().state() != Boss::State::Leap) all |= run(t, {}, 1);
        all |= run(t, {-1.0f, 0.0f}, 30); // out of the landing zone
        bool hopped = false;
        for (int i = 0; i < 180; ++i) {
            PlayerInput in;
            for (const Boss::Ring& ring : t.boss().rings()) {
                const float gap = (t.player().position() - ring.center).length() - ring.radius;
                if (hop && !hopped && ring.active && ring.delay <= 0.0f && gap > 0.0f && gap < 16.0f) {
                    in.hopPressed = true;
                    hopped = true;
                }
            }
            all |= t.update(in, kDt);
        }
        return all;
    };
    CHECK(dodge(false) & kSessionHurt);
    // ...but hopping over it is safe.
    CHECK((dodge(true) & kSessionHurt) == 0);
}

void testBossStompAndContact() {
    const Level level = parseOrDie(kArenaMap);
    LevelSession s(level, Difficulty::Normal);
    for (int i = 0; i < 120 && s.boss().sleeping(); ++i) run(s, {0.0f, -1.0f}, 1);
    while (s.boss().state() != Boss::State::Idle) run(s, {}, 1);

    // Bumping into it while it is alert hurts.
    LevelSession bump = s;
    bump.player().respawnAt(bump.boss().position() + Vec2{20.0f, 0.0f}, false);
    CHECK(run(bump, {}, 3) & kSessionHurt);

    // Dizzy: hopping onto it lands a hit and bounces the hero.
    LevelSession stomp = s;
    while (stomp.boss().state() != Boss::State::Dazed) run(stomp, {-1.0f, 0.0f}, 1);
    stomp.player().respawnAt(stomp.boss().position(), false);
    const int heartsBefore = stomp.hearts();
    const unsigned ev = run(stomp, {}, 60, 0);
    CHECK(ev & kSessionBossHit);
    CHECK(stomp.boss().health() == Boss::kMaxHealth - 1);
    CHECK(stomp.hearts() == heartsBefore);
    int have = 0, need = 0;
    stomp.objectiveProgress(have, need);
    CHECK(have == 1 && need == Boss::kMaxHealth);

    // A dash works too.
    LevelSession dash = s;
    while (dash.boss().state() != Boss::State::Dazed) run(dash, {-1.0f, 0.0f}, 1);
    // (It followed the hero to the left wall, so dash in from the right.)
    dash.player().respawnAt(dash.boss().position() + Vec2{50.0f, 0.0f}, false);
    CHECK(run(dash, {-1.0f, 0.0f}, 20, -1, 0) & kSessionBossHit);
}

void testBossFightToTheEnd() {
    const Level level = parseOrDie(kArenaMap);
    LevelSession s(level, Difficulty::Relaxed);
    unsigned ev = 0;
    bool hopIssued = false;
    for (int i = 0; i < 60 * 240 && !s.boss().defeated(); ++i) {
        PlayerInput in;
        const Boss& b = s.boss();
        if (b.sleeping()) {
            in.move = {0.0f, -1.0f}; // (back) into the arena
        } else if (b.state() == Boss::State::Dazed && !hopIssued && s.state() == LevelSession::State::Playing) {
            s.player().respawnAt(b.position(), false);
            in.hopPressed = true;
            hopIssued = true;
        }
        if (b.state() != Boss::State::Dazed) hopIssued = false;
        ev |= s.update(in, kDt);
    }
    CHECK(s.boss().defeated());
    CHECK(ev & kSessionBossDefeated);
    CHECK(s.level().tileAt(7, 8) == Tile::Ground); // the gate opened again
    CHECK(s.objectiveComplete());
    CHECK(s.stats().bossDefeated);
    CHECK(computeScore(s).foes >= ScoreBreakdown::kBoss);

    // The calm guardian is harmless.
    s.player().respawnAt(s.boss().position(), false);
    CHECK((run(s, {}, 30) & kSessionHurt) == 0);
}

void testBossKnockOutSendsItToSleep() {
    const Level level = parseOrDie(kArenaMap);
    LevelSession s(level, Difficulty::Normal);
    for (int i = 0; i < 120 && s.boss().sleeping(); ++i) run(s, {0.0f, -1.0f}, 1);
    // Land one hit first.
    while (s.boss().state() != Boss::State::Dazed) run(s, {-1.0f, 0.0f}, 1);
    s.player().respawnAt(s.boss().position(), false);
    run(s, {}, 60, 0);
    CHECK(s.boss().health() == Boss::kMaxHealth - 1);
    // Then stand in its way until the hearts run out.
    unsigned ev = 0;
    for (int i = 0; i < 60 * 60 && !(ev & kSessionKnockedOut); ++i) {
        if (s.state() == LevelSession::State::Playing && s.boss().state() == Boss::State::Idle)
            s.player().respawnAt(s.boss().position() + Vec2{18.0f, 0.0f}, false);
        ev |= run(s, {}, 1);
    }
    CHECK(ev & kSessionKnockedOut);
    ev = run(s, {}, 120);
    CHECK(ev & kSessionRespawned);
    CHECK(s.boss().sleeping());
    CHECK(s.boss().health() == Boss::kMaxHealth - 1); // the hit you landed still counts
    CHECK(s.level().tileAt(7, 8) == Tile::Ground);    // gate open again
}

// --- Phase 8: built-in sound ----------------------------------------------------------

int peakOf(const synth::Buffer& b) {
    int peak = 0;
    for (int16_t v : b.samples) peak = std::max(peak, std::abs(static_cast<int>(v)));
    return peak;
}

void testSynthSfx() {
    for (int i = 0; i < static_cast<int>(Sfx::Count); ++i) {
        const synth::Buffer b = synth::makeSfx(static_cast<Sfx>(i));
        CHECK(b.frames() > 0);
        CHECK(b.seconds() < 2.0f);       // short and snappy
        CHECK(peakOf(b) > 8000);         // clearly audible
        CHECK(peakOf(b) <= 32000);       // never clips
        // Ends in silence (no click when the sound stops).
        const int tail = std::abs(static_cast<int>(b.samples[b.samples.size() - 2]));
        if (tail >= 2000) std::printf("  sfx %s ends at %d\n", AudioManager::sfxName(static_cast<Sfx>(i)), tail);
        CHECK(tail < 2000);
    }
    // Deterministic: the same sound every run.
    CHECK(synth::makeSfx(Sfx::Coin).samples == synth::makeSfx(Sfx::Coin).samples);
}

void testSynthMusic() {
    for (MusicTrack t : {MusicTrack::Title, MusicTrack::Meadow, MusicTrack::Boss}) {
        const synth::Buffer b = synth::makeMusic(t);
        const float expected = synth::musicLoopSeconds(t);
        CHECK(std::fabs(b.seconds() - expected) < 0.001f);
        CHECK(expected > 10.0f && expected < 30.0f);
        CHECK(peakOf(b) > 12000 && peakOf(b) <= 32000);
        // Seamless loop: the last frame flows into the first.
        const int jumpL = std::abs(b.samples[0] - b.samples[b.samples.size() - 2]);
        const int jumpR = std::abs(b.samples[1] - b.samples[b.samples.size() - 1]);
        CHECK(jumpL < 3000 && jumpR < 3000);
    }
    CHECK(synth::musicLoopSeconds(MusicTrack::Boss) < synth::musicLoopSeconds(MusicTrack::Title)); // boss is faster
}

void testWavContainer() {
    synth::Buffer b;
    b.samples = {0, 1, -1, 32000, -32000, 7};
    const std::vector<uint8_t> wav = synth::toWav(b);
    CHECK(wav.size() == 44 + 12);
    CHECK(std::string(wav.begin(), wav.begin() + 4) == "RIFF");
    CHECK(std::string(wav.begin() + 8, wav.begin() + 16) == "WAVEfmt ");
    CHECK(std::string(wav.begin() + 36, wav.begin() + 40) == "data");
    CHECK(wav[40] == 12 && wav[41] == 0);
    CHECK(wav[22] == 2 && wav[34] == 16); // stereo, 16-bit
    // Samples are little-endian: -1 -> FF FF, 32000 -> 00 7D.
    CHECK(wav[48] == 0xFF && wav[49] == 0xFF);
    CHECK(wav[50] == 0x00 && wav[51] == 0x7D);
}

int main(int, char*[]) {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"key/value parsing", testKeyValueParsing},
        {"menu auto-repeat", testMenuAutoRepeat},
        {"synth sound effects", testSynthSfx},
        {"synth music", testSynthMusic},
        {"wav container", testWavContainer},
        {"boss map parsing", testBossMapParsing},
        {"boss state machine", testBossStateMachine},
        {"boss arena gates and rings", testBossArenaGatesAndRings},
        {"boss stomp and contact", testBossStompAndContact},
        {"boss fight to the end", testBossFightToTheEnd},
        {"boss knock-out", testBossKnockOutSendsItToSleep},
        {"progress unlocks and records", testProgressUnlocksAndRecords},
        {"high-score table", testHighScoreTable},
        {"outfits", testOutfits},
        {"progress round trip", testProgressRoundTrip},
        {"score breakdown", testScoreBreakdown},
        {"key/value round trip", testKeyValueRoundTrip},
        {"settings round trip", testSettingsRoundTrip},
        {"bindings from config", testBindingsFromConfig},
        {"input latching", testInputLatching},
        {"joystick events", testJoystickEvents},
        {"player acceleration", testPlayerAcceleratesToMaxSpeed},
        {"player diagonal", testPlayerDiagonalNotFaster},
        {"player dash", testPlayerDash},
        {"player hop", testPlayerHop},
        {"player knockback", testPlayerKnockbackAndInvincibility},
        {"player constrain", testPlayerConstrain},
        {"art and font data", testArtAndFontData},
        {"power-ups", testPowerUps},
        {"worlds and level", testWorldsAndLevel},
        {"ascii map parsing", testAsciiParsing},
        {"collision stops flush", testCollisionStopsFlush},
        {"no tunnelling", testNoTunnelling},
        {"corner nudge", testCornerNudge},
        {"camera", testCamera},
        {"built-in level", testBuiltinLevel},
        {"coins", testCoins},
        {"effects pool", testEffectsPool},
        {"thorns hurt", testThornsHurtAndKnockBack},
        {"hop over thorns", testHopOverThorns},
        {"water fall and dash skim", testWaterFallAndDashSkim},
        {"stomp and dash", testStompAndDashDefeatEnemies},
        {"knock-out to checkpoint", testKnockOutReturnsToCheckpoint},
        {"heart pickup", testHeartPickup},
        {"difficulty rules", testDifficultyRules},
        {"slime telegraph", testSlimeTelegraphsThenChases},
        {"enemies avoid danger", testEnemiesAvoidDanger},
        {"mushroom shockwave", testMushroomShockwave},
        {"debug warp", testDebugWarp},
        {"stars, gems, golden star", testStarsGemsAndGoldenStar},
        {"power-up slot", testPowerUpSlot},
        {"speed / super dash / size", testSpeedAndSuperDashModifiers},
        {"shield", testShieldBlocksOneHit},
        {"magnet and double coins", testMagnetAndDoubleCoins},
        {"tiny mode gaps", testTinyModeGaps},
        {"giant smashes and crushes", testGiantSmashesAndCrushes},
        {"dash smashes crates", testDashSmashesCrates},
        {"rainbow star", testRainbowStar},
        {"level file parsing", testLevelFileParsing},
        {"shipped world 1 levels", testShippedLevels},
        {"raft", testRaftCarriesAcrossWater},
        {"keys and gates", testKeysOpenGates},
        {"rescue objective", testRescueObjectiveLocksExit},
        {"signs and secrets", testSignsAndSecrets},
        {"time limit", testTimeLimit},
        {"coin objective", testCoinObjective},
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("[%s] %s\n", g_failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
