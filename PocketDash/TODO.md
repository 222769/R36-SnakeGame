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

## ✅ Phase 2: Maps, collision, camera, coins, exit (complete)

- [x] ASCII map format + parser (`Level::fromAscii`) with clear errors (row/column, missing spawn/exit).
      Phase 5's file loader will reuse it.
- [x] Tile types: ground, hedge wall, tree, rock, water, bridge, thorns (not harmful until Phase 3),
      secret wall (looks solid, walk-through), exit
- [x] TileSet: per-world procedural 16x16 art baked into one atlas (drawn at 2x). Covers grass
      variants and flowers, hedge top/front faces, shore foam, bridge direction, animated water and
      flag. Art choice per cell is precomputed; each frame draws only the visible tiles.
- [x] Tile collision: axis-separated sliding, 4 px sub-steps (no tunnelling at dash speed),
      map edges count as solid
- [x] Corner nudge: the player slips around wall corners (≤7 px), so gaps and doorways feel forgiving
- [x] Camera: dead-zone follow, velocity look-ahead, exponential easing, clamped to the map,
      a top margin so the HUD never hides the first row, centring for small maps, decaying screen shake
- [x] Coins: spinning and bobbing sprite, generous pickup radius, sparkle effect, sound hook, HUD counter
- [x] Effects pool: fixed 48 slots, no allocations (coin sparkles, dash/landing dust)
- [x] Exit flag, "LEVEL CLEAR!" results panel (coins, time to 0.1 s, all-coins bonus line),
      victory hops, play again / back to title
- [x] Level intro banner (placed on the screen half away from the hero), level clock in the HUD
- [x] Built-in 40x24 test meadow (`BuiltinLevels.cpp`): 45 coins, lakes, bridge, secret coin room
- [x] Debug: solid/exit tile outlines around the player, camera dead-zone, coin and camera readouts,
      **R1 warps next to the exit** while the overlay is on
- [x] Tests: map parsing and errors, flush stops, wall sliding, no tunnelling, corner nudge,
      camera (clamp, dead-zone, margin, centring), coins, effects pool, built-in level
      reachability (every coin and the exit can be reached from the spawn), and a smoke test that
      walks into the flag

## ✅ Phase 3: Enemies, damage, health, checkpoints (complete)

- [x] `LevelSession`: SDL-free gameplay simulation (player, enemies, hazards, pickups, checkpoints,
      exit) reporting events. `PlayScene` is now presentation only (camera, audio, HUD, menus).
- [x] Enemies, each with a readable telegraph:
  - Slime: rests, squishes, then hops towards the player (or wanders when far away)
  - Beetle: patrols left/right or up/down, pausing to turn
  - Mushroom: bounces; shivers, then big-bounces and sends out a shockwave ring to hop over
- [x] Enemies never step onto walls, water, thorns, secret walls or the exit
- [x] Stomp (land from a hop) or dash into an enemy to defeat it; touching one on foot costs a heart
- [x] Hearts HUD (wobbles and flashes red on damage), knockback, invincibility, screen shake
- [x] Thorns (feet-only, so hopping over one tile is comfortable) and water (non-solid: hop
      over it or skim across while dashing; falling in costs a heart and you're rescued to the
      last safe spot)
- [x] Heart pickups (left in place while at full health)
- [x] Checkpoints: flag turns colour when touched, heals to full, and sets the respawn point
- [x] No game over: at 0 hearts there's an "OOPS!" with dizzy stars, then you respawn at the
      checkpoint with full hearts and keep your coins
- [x] Difficulty: Relaxed (5 hearts, 0.75x enemies, extra `r` checkpoints), Normal, Challenge
      (1.25x enemies, only `C` checkpoints, 1.5x score multiplier, used in Phase 6). `--difficulty` on the command line.
- [x] Map characters: `s` `b` `B` `m` enemies, `h` heart, `C`/`k`/`r` checkpoints
- [x] Y-sorted drawing of the player and enemies
- [x] Debug: enemy hitboxes, danger tiles, last safe spot, HP readout; **L1 warps to the next enemy**
- [x] Tests: thorns, hop over thorns, water fall plus dash skim, stomp/dash/contact, knock-out →
      checkpoint respawn, heart pickups, difficulty rules, slime telegraph, enemies avoid danger
      (20 s sim), mushroom shockwave, debug warp. Smoke test checks the safe route takes no damage on
      all difficulties.
- [x] Bugs found by the tests: water rescues kept refreshing invincibility, so repeated falls
      were free. The debug warp could drop the hero onto the enemy it was aiming at.

## ✅ Phase 4: Collectibles, dash challenges, power-ups (complete)

- [x] Stars (`*`, three per level), shown as HUD slots; gems (`g`) with a HUD counter
- [x] Golden Star for clearing a level with every star, coin and gem in one run
- [x] Power-up bubbles (`1`–`8`), a one-slot inventory (X uses it). While the slot is full,
      other bubbles stay on the ground for later.
  - [x] Speed Shoes (1.5x speed), Shield Bubble (absorbs one hit or fall), Magnet (pulls coins
        within 4 tiles), Super Dash (2x dash length, half cooldown), Double Coins (coins count
        twice for score)
  - [x] Tiny Mode (slips through `:` tiny gaps, never ends while inside one)
  - [x] Giant Mode (smashes crates and boulders, crushes enemies; refuses to activate without
        room, so it can't trap you)
  - [x] Rainbow Star (invulnerable, defeats enemies on touch, colour-cycling hero)
- [x] Breakable blocks: `x` crates (dash or giant), `X` boulders (giant only). The session works
      on a level copy and restart restores it; tile art is re-baked when a block breaks.
- [x] HUD: star slots, gem counter, stored power-up with an X hint, active power-ups with draining
      (and end-blinking) timer bars, pulsing x2 badge for Double Coins, shield bubble around the hero
- [x] Results panel: stars pop in one by one, gems, GOLDEN STAR banner
- [x] Test level: three stars hidden behind the secret hedge, crates, and a tiny-gap nook; a gem
      sealed by a boulder; all eight power-ups
- [x] Tests: stars/gems/golden star, power-up slot rules and expiry, speed/super-dash/size
      modifiers, shield, magnet + double coins, tiny gaps (including no-room Giant and not shrinking
      inside a gap), giant smashing/crushing and restart restoring blocks, dash vs crates, rainbow.
      The level test checks every collectible is reachable, and that any boulders or tiny gaps come
      with a reachable Giant or Tiny power-up.

## ✅ Phase 5: Data-driven levels and World 1 (complete)

- [x] `.lvl` loader (`LevelLoader`): key=value header, `[map]`, `[signs]`. Strict, with file line
      numbers in errors (unknown keys/characters, sign count mismatch, impossible objectives).
- [x] Falls back to the built-in meadow (with an on-screen notice) if a file is missing or broken
- [x] Objectives: reach exit, collect N coins, find all stars, rescue friends, defeat all enemies.
      The exit shows a padlock until done; an early touch gives a "NOT YET!" hint.
- [x] Time limits (`time_limit=`, 50% longer on Relaxed), a countdown clock, and a TIME UP retry panel
- [x] Y = interact: read signs (word-wrapped dialog) and rescue lost friends; bobbing Y prompt
- [x] Rafts (`R` left/right, `V` up/down) that carry the hero; forgiving boarding margin
- [x] Keys (`K`) and locked gates (`L`); a whole connected gate opens with one key
- [x] Secrets: each connected group of `%` tiles counts; "SECRET FOUND!" toast
- [x] Golden Star now also needs every friend and secret
- [x] Toast messages (key, gate, rescue, secret, locked, no room)
- [x] HUD: objective progress, key count, countdown
- [x] World 1 levels 1-1 … 1-8 generated by `tools/make_world1.py`, which refuses to write
      unreachable content
- [x] Level flow: title → 1-1, clear → next level (A) or replay (B); `--level ID`, `--check-levels`
- [x] Tests: loader parsing and every error path, all shipped levels (load, 3 stars, every item
      reachable with the abilities each level provides), raft, keys and gates, rescue objective
      and exit lock, signs and secrets, time limit (and Relaxed bonus), coin objective. New CTest
      `level_files` loads the levels through the runtime data path.
- [x] Bugs found along the way:
  - [x] Rafts turned around 2 px short of the bank, leaving water exactly where you board
        (found by a test)
  - [x] Sign texts were paired with the wrong signs (found in a screenshot; the generator now
        sorts them into reading order)
  - [x] Long bridges alternated plank direction (found in a screenshot)
  - [x] 1-2's raft channel cut the map in half (found by the reachability validator)
  - [x] 1-6's key pocket didn't need Giant Mode, and 1-5's star could be reached by walking
        around (found by reading the maps)

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

## ✅ Visual overhaul: modern look, natural colours (complete)

- [x] `Canvas` software painter: SDF anti-aliasing, sphere lighting, soft shadows, tileable value noise
- [x] 32x32 tiles painted with noise-textured grass, leafy hedges, calm water with glints, shaded props
- [x] Smooth, shaded sprites drawn 1:1 (hero 32x40, enemies 32x32, items 24x24) plus a large title hero
- [x] Natural world palettes (`World.cpp`)
- [x] Smooth TTF text, rounded anti-aliased panels with drop shadows, anti-aliased circles and shadows
- [x] Painted title backdrop: gradient sky, glowing sun, hazy hill layers, shaded clouds
- [ ] Check on the device that the extra startup painting stays under ~2 s

## Phase 8: Audio, animation, particles, transitions, polish

- [ ] Placeholder SFX synthesised at startup when no files exist (chiptune beeps)
- [ ] Music: title, meadow, boss (and a crossfade between them)
- [~] Particle pool: a basic fixed-size `Effects` pool exists (Phase 2). Extend with hit stars and leaf bursts.
- [ ] Screen transitions (iris wipe), level title cards
- [ ] Screen shake (respects the setting), hit-stop on damage
- [x] TTF UI font (DejaVu Sans Bold) with pixel-font fallback (visual overhaul)
- [ ] Performance pass on the device: profile with the debug overlay, target a stable 60 FPS

---

## Later: other worlds and modes

The architecture is ready for these: `World.cpp` already holds themes and rules
for all 7 worlds.

- [ ] World 2 Frozen Peaks (ice friction, `rules.frictionScale`), World 3 Desert Ruins (moving platforms),
      World 4 Candy Kingdom (bounce pads), World 5 Spooky Woods (moving shadows),
      World 6 Volcano Valley (falling rocks), World 7 Cosmic Zone (low gravity, `rules.gravityScale`)
- [ ] Modes: Time Attack, Coin Rush, Endless (procedural)
