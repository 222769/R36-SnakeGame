#!/bin/bash
# Pocket Dash launcher for ArkOS (and similar) "Ports" menus.
#
# Install: copy the whole PocketDash folder (this script, the pocketdash
# binary, assets/ and config/) to /roms/ports/PocketDash/ and copy this
# script to /roms/ports/ (one level up) so it appears in the Ports menu.
# The script finds the game folder either next to itself or in PocketDash/.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -x "$SCRIPT_DIR/pocketdash" ]; then
    GAME_DIR="$SCRIPT_DIR"
else
    GAME_DIR="$SCRIPT_DIR/PocketDash"
fi

cd "$GAME_DIR" || exit 1

# Console builds of SDL2 on ArkOS use KMSDRM; nothing else to configure.
export POCKETDASH_DATA="$GAME_DIR"
# If a firmware image lacks SDL2_image/ttf/mixer, copy the aarch64 .so files
# into PocketDash/libs/ and they will be picked up from there.
export LD_LIBRARY_PATH="$GAME_DIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

{
    echo "Pocket Dash launcher: $(date 2>/dev/null)"
    # Name any missing shared library up front (the most common reason a port
    # does not start), so log.txt says exactly what to install or copy.
    if command -v ldd > /dev/null 2>&1; then
        missing=$(ldd ./pocketdash 2>/dev/null | grep "not found")
        if [ -n "$missing" ]; then
            echo "MISSING LIBRARIES (copy aarch64 builds into $GAME_DIR/libs/):"
            echo "$missing"
        fi
    fi
} > "$GAME_DIR/log.txt" 2>&1

./pocketdash "$@" >> "$GAME_DIR/log.txt" 2>&1

# Return to a clean console (some frontends leave the TTY in graphics mode).
printf "\033c" > /dev/tty1 2>/dev/null || true
