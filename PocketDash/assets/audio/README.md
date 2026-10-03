# Audio

Every file is optional. The game synthesises any sound or music track that
has no file here (see `src/Synth.cpp`): soft bells, warm tones and filtered
noise for the effects, and three looping tracks composed at startup on a
background thread. To hear the built-in versions, or to use them as a
starting point, export them as WAV files:

```bash
./pocketdash --export-audio exported-audio
```

To replace a sound, drop a file with the matching name in this folder.

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

Music (`.ogg`, `.mp3` or `.wav`, looped): `title` (title and menus),
`meadow` (levels) and `boss` (the Meadow Guardian fight). Built-in tracks
crossfade into each other; file music fades out before the next one fades in.
The music dips under the level-complete fanfare.

Example: `assets/audio/coin.wav`, `assets/audio/meadow.ogg`.

Keep sound effects short, 44.1 kHz and 16-bit. Use OGG for music to save space
on the SD card.
