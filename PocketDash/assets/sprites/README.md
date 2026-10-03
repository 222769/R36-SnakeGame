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
| `coin.png`        | 36x12: 3 spin frames of 12x12                                 |
| `enemies.png`     | 32x64: 2 frames x 4 rows (slime, beetle facing right, mushroom, lost chick) of 16x16 |
| `props.png`       | 48x16: checkpoint flag (not reached), flag (reached), sign    |
| `items.png`       | 144x12: star, empty star, gem, power-up icons 1–8, key (12x12 each; icon 8 is drawn white and tinted) |

Tiles are generated per world from the theme colours in `src/World.cpp`.
