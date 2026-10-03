#pragma once

#include "Level.h"

#include <string>
#include <vector>

// Loads levels from plain-text .lvl files (assets/levels/<id>.lvl):
//
//   # comment
//   id=1-1
//   name=WELCOME MEADOW
//   world=1                 (1-based)
//   objective=exit          exit | coins | stars | rescue | defeat
//   goal=20                 coins needed (objective=coins only)
//   time_limit=90           seconds, optional (Relaxed gets 50% more)
//   hint=REACH THE FLAG!    banner subtitle, optional
//   [map]
//   ##########
//   #P..c...E#
//   ##########
//   [signs]
//   PRESS A TO HOP!         one line per S on the map, in reading order
//
// The map uses the legend in Level.h. Parsing is strict: unknown keys,
// characters or a sign count mismatch are errors that quote the line, so a
// typo never silently changes a level.
namespace pd::levels {

bool parse(const std::string& text, Level& out, std::string* error);
bool loadFile(const std::string& path, Level& out, std::string* error);

// Level ids of a world in play order ("1-1" ... "1-8").
const std::vector<std::string>& worldLevelIds(int world);
// The level after `id`, or "" after the last one.
std::string nextLevelId(const std::string& id);

// Loads assets/levels/<id>.lvl from the data directory.
bool load(const std::string& id, Level& out, std::string* error);

} // namespace pd::levels
