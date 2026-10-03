# Audio

Drop sound files here. Every file is optional: if one is missing, that sound
is silent and the game carries on.

Sound effects (`.wav` or `.ogg`):

| File               | Played when                    |
|--------------------|--------------------------------|
| coin               | a coin is collected            |
| jump               | the player hops                |
| dash               | the player dashes              |
| enemy_hit          | an enemy is defeated           |
| player_hurt        | the player takes damage        |
| powerup            | a power-up is collected/used   |
| star               | a star is collected            |
| level_complete     | a level is finished            |
| menu_move          | the menu cursor moves          |
| menu_select        | a menu item is chosen          |
| pause              | the game is paused / resumed   |
| heart              | a heart pickup is collected    |
| checkpoint         | a checkpoint flag is touched   |
| splash             | the player falls in water      |
| gem                | a gem is collected             |
| break              | a crate or boulder is smashed  |
| shield_pop         | the shield bubble absorbs a hit|
| powerup_use        | a power-up is activated (X)    |
| denied             | not allowed: no room for Giant, locked gate, flag not open yet |

Music (`.ogg`, `.mp3` or `.wav`, looped): `title`, `meadow`, `boss`.

Example: `assets/audio/coin.wav`, `assets/audio/meadow.ogg`.

Keep sound effects short, 44.1 kHz and 16-bit. Use OGG for music to save space
on the SD card.
