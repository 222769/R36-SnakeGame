# Sprites

The game paints all of its artwork at startup with a small software painter
(`src/Canvas.*`): anti-aliased shapes, lit "3D" shading and soft shadows.
The painters are in `src/Sprites.cpp`, so this folder can stay empty. To
replace a sheet, add a PNG with the same layout (transparency supported).
Sprites are drawn 1:1 at the sizes below, with smooth (linear) filtering.

| File              | Layout                                                        |
|-------------------|---------------------------------------------------------------|
| `player.png`      | 64x120: 2 columns (idle, step) x 3 rows (front, back, side→right) of 32x40 frames |
| `heart_full.png`  | 28x24 HUD heart                                               |
| `heart_empty.png` | 28x24 HUD heart (lost)                                        |
| `coin.png`        | 72x24: 3 spin frames of 24x24                                 |
| `enemies.png`     | 64x128: 2 frames x 4 rows (slime, beetle facing right, mushroom, lost chick) of 32x32 |
| `props.png`       | 96x32: checkpoint flag (not reached), flag (reached), sign (32x32 each) |
| `items.png`       | 288x24: star, empty star, gem, power-up icons 1–8, key (24x24 each; icon 8 is drawn white and tinted) |

The title screen's large hero (2 frames of 80x100) is always painted.

Tiles (32x32) are generated per world from the theme colours in `src/World.cpp`.
