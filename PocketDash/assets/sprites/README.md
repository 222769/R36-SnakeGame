# Sprites

The game generates placeholder pixel art at startup (see `src/Sprites.cpp`),
so this folder can stay empty. To replace a placeholder, add a PNG with the
same layout. Transparency is supported. Art is drawn at 2x with
nearest-neighbour scaling.

| File              | Layout                                                        |
|-------------------|---------------------------------------------------------------|
| `player.png`      | 32x48: 2 columns (idle, step) x 3 rows (front, back, side→right) of 16x16 frames |
| `heart_full.png`  | 9x8 HUD heart                                                 |
| `heart_empty.png` | 9x8 HUD heart (lost)                                          |

More sheets (tiles, enemies, items) are added in later phases.
