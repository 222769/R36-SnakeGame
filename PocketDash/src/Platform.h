#pragma once

// Platform-specific decisions live here so the rest of the game stays
// portable. Anything that differs between a desktop PC and the R36S
// (paths, fullscreen, window size, labels) is answered by this module.

#include <string>

namespace pd::platform {

// Human-readable platform label shown in logs and the debug overlay.
const char* name();

// True when built for the handheld (-DPOCKETDASH_R36S=ON).
bool isHandheld();

// Default to fullscreen on the handheld or when no desktop session exists
// (e.g. KMSDRM on ArkOS); windowed on a desktop PC.
bool defaultFullscreen();

// Initial window scale on desktop (640x480 * scale).
int defaultWindowScale();

// Directory that contains assets/ and config/. Resolution order:
//   1. $POCKETDASH_DATA
//   2. the executable's directory, then its parent and grandparent
//      (covers build/ and build/Release/ layouts during development)
//   3. the current working directory
// Always ends with a path separator.
const std::string& dataRoot();

// Writable directory for save files (always ends with a separator).
// Prefers <dataRoot>/save/, falling back to SDL's per-user pref path.
const std::string& saveDir();

// Joins the data root with a relative path like "config/controller.cfg".
std::string dataPath(const std::string& relative);

} // namespace pd::platform
