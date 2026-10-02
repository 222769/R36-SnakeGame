# Pocket Dash: development roadmap

Legend: `[x]` done · `[ ]` to do · `[~]` partly done / skeleton in place

---

## ✅ Phase 1: Foundations (complete)

- [x] CMake project (C++17, pkg-config with a CMake-config fallback, install rules)
- [x] Full project skeleton: every module from the spec compiles and links
- [x] 640x480 logical resolution, nearest-neighbour, integer scaling, vsync with software fallback
- [x] Fixed 60 Hz timestep loop with jitter snapping and a frame limiter when vsync is unavailable
- [x] RAII for all SDL resources (`SdlPtr.h`, `Game::SdlSystem`)
- [x] Platform layer isolating R36S decisions (data/save paths, fullscreen defaults)
- [x] Input: keyboard and raw SDL joystick (buttons, hat, analogue stick with radial deadzone)
- [x] `config/controller.cfg`: pad buttons, d-pad, axes, deadzone, inversion and keyboard bindings
- [x] Controller hot-plug; startup log of name, GUID, buttons, axes, hats and SDL mapping
- [x] Handheld hotkeys: hold Select+Start to quit, Select+L1 for the debug overlay
- [x] Press latching (no lost taps, no double presses)
- [x] Player: acceleration/friction movement, fast turnaround, 4-way facing, walk animation
- [x] Hop (height axis, shadow, squash and stretch), dash (afterimages, cooldown)
- [x] Knockback, stun and invincibility blink, implemented and unit-tested (wired to enemies in Phase 3)
- [x] Programmatic pixel-art player and HUD hearts, with PNG overrides
- [x] Built-in 5x7 bitmap font (no font file needed)
- [x] Title screen (animated logo, "press A")
- [x] Sandbox gameplay scene, pause menu (resume / restart / quit to title), Select info panel
- [x] Debug overlay (F1): FPS, frame time, renderer, scene/level, player position/velocity/Z, enemy count,
      controller name, held raw buttons, last button, hat, axes, move vector, collision boxes
- [x] AudioManager (SDL2_mixer). Runs silently when the device or files are missing.
- [x] Settings persistence (`save/settings.ini`, atomic writes)
- [x] Unit tests (93 checks) and a headless smoke test, both in CTest
- [x] aarch64 cross toolchain file; cross-compile verified (`-mcpu=cortex-a35`, zero warnings)
- [x] ArkOS launcher script (`dist/PocketDash.sh`) and Docker-based ArkOS build (`tools/build-arkos.sh`)

### Phase 1 open items / notes
- [ ] **Test on real R36S hardware.** Confirm the default button numbers, KMSDRM fullscreen, vsync and 60 FPS.
      If the buttons are wrong, update the defaults in `InputBindings::defaults()` and `controller.cfg`.
- [x] `tools/build-arkos.sh` verified end to end (Ubuntu 20.04 container, GCC 9: zero warnings;
      binary needs only GLIBC_2.17 and GLIBCXX_3.4.26, so it is ArkOS-compatible)
- [ ] Confirm SDL2_image/ttf/mixer are present on the target firmware (otherwise bundle them in `libs/`)
- [ ] Windows build untested.

---

## Phase 2: Maps, collision, camera, coins, exit

- [ ] Tile renderer: per-world tile atlas baked at load; draw only visible tiles
- [ ] Tile collision for the player: separate X/Y axis resolution against the solid tiles in `Level`
- [ ] Camera: follows the player with a dead-zone, clamps to the map edges, supports screen shake
- [ ] Coins: entity list, bobbing animation, pickup radius, sparkle, counter in the HUD
- [ ] Level exit tile and a "level complete" state (simple results panel)
- [ ] Temporary built-in test map (until Phase 5 loads files)
- [ ] Unit tests: collision corners, tunnelling at dash speed, camera clamping

## Phase 3: Enemies, damage, health, checkpoints

- [ ] Enemy behaviours: Slime (hop towards player), Beetle (patrol), Mushroom (bounce)
- [ ] Hop on an enemy or dash into it to defeat it (the rules need to be readable for kids)
- [ ] Contact damage → `Player::takeHit` (knockback and invincibility already exist)
- [ ] Hearts: lose and restore; collectible heart pickups
- [ ] Checkpoints (flag): respawn with full hearts; count depends on difficulty
- [ ] Hazards: thorns/spikes, water (respawn at the last safe tile)
- [ ] Difficulty scaling: Relaxed = +2 hearts, 0.75x enemy speed, extra checkpoints; Challenge = 1.25x speed, 1.5x score

## Phase 4: Collectibles, dash challenges, power-ups

- [ ] Stars (3 per level, hidden) and gems (rare)
- [ ] Golden Star (all objectives in one level)
- [ ] Power-up pickups and the stored slot (X to use). `PowerUpState` already exists.
  - [ ] Speed Shoes, Shield Bubble, Magnet, Super Dash, Double Coins
  - [ ] Tiny Mode (small gaps), Giant Mode (smash crates), Rainbow Star (invincible)
- [ ] Dash-breakable blocks and dash gates (for 1-5 Dash Valley)
- [ ] HUD: coin counter, stars found, stored power-up icon with timer ring

## Phase 5: Data-driven levels + World 1

- [ ] `.lvl` loader (format documented in `src/Level.h`): header, ASCII map, entity lines
- [ ] Clear error messages for malformed level files (line numbers), plus unit tests
- [ ] Objectives: reach exit, collect N stars/coins, rescue creatures, find key, defeat all, timed
- [ ] Y = interact (signs, levers, rescuing creatures)
- [ ] Secrets: secret walls and hidden areas
- [ ] World 1 levels:
  - [ ] 1-1 Welcome Meadow (tutorial signs)
  - [ ] 1-2 River Run (moving platforms, water)
  - [ ] 1-3 Coin Forest
  - [ ] 1-4 Lost Friends (rescue)
  - [ ] 1-5 Dash Valley
  - [ ] 1-6 Hidden Garden (keys, secrets)
  - [ ] 1-7 Star Sprint (timed)
  - [ ] 1-8 Meadow Guardian (boss arena, Phase 7)

## Phase 6: Menus, level select, scoring, saves

- [ ] Main menu: Play, Level Select, Collection, High Scores, Settings, Quit
- [ ] Menu auto-repeat for held directions
- [ ] Level select map for World 1 with lock state, stars and Golden Star icons
- [ ] Score: coins, enemies, stars, remaining hearts, time bonus, secrets (and the difficulty multiplier)
- [ ] Per-level records: best score, best time, stars found, secrets found, Golden Star
- [ ] Local high-score table (top 5 per level, 3-letter initials)
- [ ] `save/progress.ini`: unlocks, records, collectibles, gems
- [ ] Settings screen: music/SFX volume, screen shake, difficulty, controller test/remap screen
- [ ] Collection screen (gems → cosmetic items)

## Phase 7: Boss — The Meadow Guardian

- [ ] Arena level 1-8
- [ ] Boss states: idle → telegraph (crouch and flash, shadow grows) → leap → land shockwave ring → recover (vulnerable)
- [ ] Shockwave rings that you hop over; the hop reads clearly to younger players
- [ ] 3 phases, faster and adding a double-jump in phase 3; boss health bar
- [ ] Boss intro card and defeat celebration

## Phase 8: Audio, animation, particles, transitions, polish

- [ ] Placeholder SFX synthesised at startup when no files exist (chiptune beeps)
- [ ] Music: title, meadow, boss (and a crossfade between them)
- [ ] Particle pool (fixed size, no allocations): dust on dash/land, coin sparkles, hit stars
- [ ] Screen transitions (iris wipe), level title cards
- [ ] Screen shake (respects the setting), hit-stop on damage
- [ ] Optional TTF display font
- [ ] Performance pass on the device: profile with the debug overlay, target a stable 60 FPS

---

## Later: other worlds and modes

The architecture is ready for these: `World.cpp` already holds themes and rules
for all 7 worlds.

- [ ] World 2 Frozen Peaks (ice friction, `rules.frictionScale`), World 3 Desert Ruins (moving platforms),
      World 4 Candy Kingdom (bounce pads), World 5 Spooky Woods (moving shadows),
      World 6 Volcano Valley (falling rocks), World 7 Cosmic Zone (low gravity, `rules.gravityScale`)
- [ ] Modes: Time Attack, Coin Rush, Endless (procedural)
