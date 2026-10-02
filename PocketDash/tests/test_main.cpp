// Pocket Dash unit tests. Dependency-free: no SDL window or audio needed.

#include "BuiltinLevels.h"
#include "Camera.h"
#include "Collectibles.h"
#include "Effects.h"
#include "Enemy.h"
#include "InputManager.h"
#include "LevelSession.h"
#include "Level.h"
#include "Player.h"
#include "PowerUp.h"
#include "SaveManager.h"
#include "Sprites.h"
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

    // Every coin and the exit must be reachable from the spawn (tile flood fill).
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
            if (level.inBounds(nx, ny) && !Level::isSolid(level.tileAt(nx, ny)) &&
                !Level::isDanger(level.tileAt(nx, ny)) && seen.insert({nx, ny}).second)
                stack.push_back({nx, ny});
        }
    }
    int unreachable = 0;
    for (const Vec2& c : level.coins)
        if (!seen.count({static_cast<int>(c.x) / 32, static_cast<int>(c.y) / 32})) ++unreachable;
    CHECK(unreachable == 0);
    CHECK(seen.count({static_cast<int>(level.exit.x) / 32, static_cast<int>(level.exit.y) / 32}) == 1);
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
    CHECK(fx.activeCount() == 48); // fixed pool, recycles the oldest
    for (int i = 0; i < 60; ++i) fx.update(kDt);
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

} // namespace

int main(int, char*[]) {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"key/value parsing", testKeyValueParsing},
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
    };
    for (const auto& [name, fn] : tests) {
        const int before = g_failures;
        fn();
        std::printf("[%s] %s\n", g_failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
