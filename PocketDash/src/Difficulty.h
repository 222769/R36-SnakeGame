#pragma once

#include "Settings.h"

namespace pd {

// What each difficulty changes. No content is ever locked behind a harder
// setting: every level, secret and collectible exists on all three.
struct DifficultyRules {
    int maxHearts;
    float enemySpeed;      // multiplier on enemy movement and timing
    float scoreMultiplier;
};

inline DifficultyRules difficultyRules(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return {5, 0.75f, 1.0f};
    case Difficulty::Challenge: return {3, 1.25f, 1.5f};
    case Difficulty::Normal: break;
    }
    return {3, 1.0f, 1.0f};
}

// A checkpoint marked for difficulty `hardest` appears on that difficulty
// and every easier one (Relaxed < Normal < Challenge).
inline bool checkpointEnabled(Difficulty hardest, Difficulty current) {
    return static_cast<int>(current) <= static_cast<int>(hardest);
}

} // namespace pd
