#pragma once

#include "Level.h"

#include <string>

namespace pd {

// Levels compiled into the executable. Phase 2 uses a single test meadow;
// Phase 5 moves World 1 into data files under assets/levels/ and keeps this
// as a fallback so the game always has something to play.
bool makeTestLevel(Level& out, std::string* error);

} // namespace pd
