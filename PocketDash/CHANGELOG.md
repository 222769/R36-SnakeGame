# Changelog

## 1.0.0

The first complete release: World 1 (Sunny Meadows), eight levels ending in
a boss fight, for the R36S (ArkOS) and desktop Linux.

**Gameplay**
- Top-down movement with hop (A), dash (B), power-up (X) and interact (Y).
- Eight levels with goals: reach the flag, collect coins, find stars, rescue
  lost friends, beat the clock, and the boss.
- Enemies (slime, beetle, mushroom with shockwaves), thorns, water, rafts,
  keys and gates, crates and boulders, secret passages, signs.
- Eight power-ups, three stars and a hidden gem per level, Golden Star runs.
- No game over: a knock-out returns you to the last checkpoint.
- Three difficulties (Relaxed, Normal, Challenge).
- Level 1-8: the Meadow Guardian boss, with telegraphed leaps, shockwave
  rings to hop over and three phases.

**Menus and saves**
- Main menu, World 1 map, per-level top-5 high scores with initials,
  score breakdown, records, unlocks, gem-unlocked outfits.
- Settings: volumes, screen shake, difficulty, controller test, button
  remapping (saved to `save/controller.cfg`), erase save.

**Presentation**
- All art painted at startup (anti-aliased, shaded, natural colours),
  smooth TTF text, rounded panels, particles, iris-wipe transitions.
- All sound effects and three music loops synthesised at startup; any file
  in `assets/audio/` replaces the built-in version.

**Tools**
- `--bench` (frame-time report), `--check-levels`, `--export-audio DIR`,
  `--scene NAME`, `--smoke-test`, `--menu-test`, screenshots.
- Docker cross-build for ArkOS (glibc 2.30) producing a ready-to-unzip
  release archive.
