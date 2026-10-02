#pragma once

#include <string>

namespace pd {

enum class Difficulty { Relaxed, Normal, Challenge };

inline const char* difficultyName(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "RELAXED";
    case Difficulty::Normal: return "NORMAL";
    case Difficulty::Challenge: return "CHALLENGE";
    }
    return "NORMAL";
}

inline Difficulty difficultyFromName(const std::string& s) {
    if (s == "RELAXED" || s == "relaxed") return Difficulty::Relaxed;
    if (s == "CHALLENGE" || s == "challenge") return Difficulty::Challenge;
    return Difficulty::Normal;
}

// User-facing options, persisted in save/settings.ini.
struct Settings {
    int musicVolume = 7;   // 0..10
    int sfxVolume = 8;     // 0..10
    bool screenShake = true;
    Difficulty difficulty = Difficulty::Normal;
};

} // namespace pd
